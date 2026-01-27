#include "inference.h"
#include <algorithm>
#include <numeric>
#include <QDebug>

YOLO_V8::YOLO_V8() {}

YOLO_V8::~YOLO_V8() {
    if (session) {
        delete session;
        session = nullptr;
    }
}

bool YOLO_V8::CreateSession(DL_INIT_PARAM& params) {
    this->params = params;
    
    try {
        env = Ort::Env(ORT_LOGGING_LEVEL_WARNING, "YOLO");
        sessionOptions = Ort::SessionOptions();
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        
        std::wstring wideModelPath(params.modelPath.begin(), params.modelPath.end());
        session = new Ort::Session(env, wideModelPath.c_str(), sessionOptions);
        
        Ort::AllocatorWithDefaultOptions allocator;
        
        size_t numInputNodes = session->GetInputCount();
        for (size_t i = 0; i < numInputNodes; i++) {
            inputNodeNamesPtr.push_back(session->GetInputNameAllocated(i, allocator));
            inputNodeNames.push_back(inputNodeNamesPtr.back().get());
        }
        
        size_t numOutputNodes = session->GetOutputCount();
        for (size_t i = 0; i < numOutputNodes; i++) {
            outputNodeNamesPtr.push_back(session->GetOutputNameAllocated(i, allocator));
            outputNodeNames.push_back(outputNodeNamesPtr.back().get());
        }
        
        classes = {"object"};
        
        return true;
        
    } catch (const Ort::Exception& e) {
        qDebug() << "ONNX Runtime Exception:" << e.what();
        return false;
    } catch (const std::exception& e) {
        qDebug() << "Exception:" << e.what();
        return false;
    }
}

void YOLO_V8::preprocessImage(cv::Mat& img, float*& blob) {
    try {
        cv::Mat rgbImg;
        if (img.channels() == 1) {
            cv::cvtColor(img, rgbImg, cv::COLOR_GRAY2RGB);
        } else if (img.channels() == 3) {
            rgbImg = img.clone();
        } else {
            blob = nullptr;
            return;
        }
        
        cv::Mat resized;
        cv::resize(rgbImg, resized, cv::Size(params.imgSize[0], params.imgSize[1]));
        
        resized.convertTo(resized, CV_32FC3, 1.0 / 255.0);
        
        int channels = 3;
        int width = params.imgSize[0];
        int height = params.imgSize[1];
        size_t totalSize = channels * width * height;
        
        blob = new float[totalSize];
        if (!blob) return;
        
        for (int c = 0; c < channels; ++c) {
            for (int h = 0; h < height; ++h) {
                for (int w = 0; w < width; ++w) {
                    blob[c * width * height + h * width + w] = 
                        resized.at<cv::Vec3f>(h, w)[c];
                }
            }
        }
        
    } catch (const std::exception& e) {
        qDebug() << "Preprocess error:" << e.what();
        if (blob) {
            delete[] blob;
            blob = nullptr;
        }
    }
}

void YOLO_V8::postprocessOutput(float* output, std::vector<DL_RESULT>& results, 
                                 int originalWidth, int originalHeight) {
    if (!output) return;
    
    try {
        std::vector<cv::Rect> boxes;
        std::vector<float> confidences;
        std::vector<int> classIds;
        
        int numDetections = 5376;
        int valuesPerDetection = 6;
        
        float scaleX = (float)originalWidth / params.imgSize[0];
        float scaleY = (float)originalHeight / params.imgSize[1];
        
        auto sigmoid = [](float x) { return 1.0f / (1.0f + std::exp(-x)); };
        
        for (int i = 0; i < numDetections; ++i) {
            int offset = i * valuesPerDetection;
            
            float cx = output[offset + 0];
            float cy = output[offset + 1];
            float w = output[offset + 2];
            float h = output[offset + 3];
            float conf_raw = output[offset + 4];
            
            float confidence = sigmoid(conf_raw);
            
            if (confidence >= params.rectConfidenceThreshold) {
                int x = (int)((cx - w/2) * scaleX);
                int y = (int)((cy - h/2) * scaleY);
                int width = (int)(w * scaleX);
                int height = (int)(h * scaleY);
                
                if (x < 0) x = 0;
                if (y < 0) y = 0;
                if (x + width > originalWidth) width = originalWidth - x;
                if (y + height > originalHeight) height = originalHeight - y;
                
                if (width > 0 && height > 0) {
                    boxes.push_back(cv::Rect(x, y, width, height));
                    confidences.push_back(confidence);
                    classIds.push_back(0);
                }
            }
        }
        
        if (!boxes.empty()) {
            std::vector<int> indices;
            cv::dnn::NMSBoxes(boxes, confidences, params.rectConfidenceThreshold, 
                              params.iouThreshold, indices);
            
            for (int idx : indices) {
                DL_RESULT result;
                result.classId = 0;
                result.confidence = confidences[idx];
                result.box = boxes[idx];
                results.push_back(result);
            }
        }
        
    } catch (const std::exception& e) {
        qDebug() << "Postprocess error:" << e.what();
    }
}

void YOLO_V8::RunSession(cv::Mat& inputImg, std::vector<DL_RESULT>& results) {
    results.clear();
    
    if (!session || inputImg.empty()) return;
    
    try {
        float* blob = nullptr;
        
        preprocessImage(inputImg, blob);
        if (!blob) return;
        
        std::vector<int64_t> inputShape = {1, 3, params.imgSize[0], params.imgSize[1]};
        size_t tensorSize = 3 * params.imgSize[0] * params.imgSize[1];
        
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(
            OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);
        
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, blob, tensorSize, 
            inputShape.data(), inputShape.size());
        
        auto outputTensors = session->Run(
            Ort::RunOptions{nullptr},
            inputNodeNames.data(), &inputTensor, 1,
            outputNodeNames.data(), 1);
        
        float* outputData = outputTensors[0].GetTensorMutableData<float>();
        if (!outputData) {
            delete[] blob;
            return;
        }
        
        postprocessOutput(outputData, results, inputImg.cols, inputImg.rows);
        
        delete[] blob;
        
    } catch (const std::exception& e) {
        qDebug() << "YOLO error:" << e.what();
        results.clear();
    }
}

#include "inference.h"
#include <algorithm>
#include <numeric>

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

        // Multi-class support: numFeatures = 4 (bbox) + numClasses
        auto outputInfo = session->GetOutputTypeInfo(0);
        auto tensorInfo = outputInfo.GetTensorTypeAndShapeInfo();
        auto shape = tensorInfo.GetShape();

        int numFeatures = static_cast<int>(shape[1]);
        int numClasses = numFeatures - 4;

        if (params.classNames.empty()) {
            if (numClasses == 1) {
                classes = {"Object"};
            } else {
                classes.clear();
                for (int i = 0; i < numClasses; i++) {
                    classes.push_back("Class_" + std::to_string(i));
                }
            }
        } else {
            classes = params.classNames;
        }

        return true;

    } catch (const Ort::Exception& e) {
        return false;
    } catch (const std::exception& e) {
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

        int targetWidth = params.imgSize[0];
        int targetHeight = params.imgSize[1];

        float scale = std::min((float)targetWidth / rgbImg.cols,
                               (float)targetHeight / rgbImg.rows);

        int newWidth = (int)(rgbImg.cols * scale);
        int newHeight = (int)(rgbImg.rows * scale);

        cv::Mat resized;
        cv::resize(rgbImg, resized, cv::Size(newWidth, newHeight));

        cv::Mat padded(targetHeight, targetWidth, CV_8UC3, cv::Scalar(114, 114, 114));

        int padX = (targetWidth - newWidth) / 2;
        int padY = (targetHeight - newHeight) / 2;

        resized.copyTo(padded(cv::Rect(padX, padY, newWidth, newHeight)));

        padded.convertTo(padded, CV_32FC3, 1.0 / 255.0);

        int channels = 3;
        int width = targetWidth;
        int height = targetHeight;
        size_t totalSize = channels * width * height;

        blob = new float[totalSize];
        if (!blob) return;

        for (int c = 0; c < channels; ++c) {
            for (int h = 0; h < height; ++h) {
                for (int w = 0; w < width; ++w) {
                    blob[c * width * height + h * width + w] =
                        padded.at<cv::Vec3f>(h, w)[c];
                }
            }
        }

    } catch (const std::exception& e) {
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

        auto outputInfo = session->GetOutputTypeInfo(0);
        auto tensorInfo = outputInfo.GetTensorTypeAndShapeInfo();
        auto shape = tensorInfo.GetShape();

        if (shape.size() < 3) return;

        int numFeatures = static_cast<int>(shape[1]);
        int numPredictions = static_cast<int>(shape[2]);
        int numClasses = numFeatures - 4;

        float scale = std::min((float)params.imgSize[0] / originalWidth,
                               (float)params.imgSize[1] / originalHeight);

        int newWidth = (int)(originalWidth * scale);
        int newHeight = (int)(originalHeight * scale);
        int padX = (params.imgSize[0] - newWidth) / 2;
        int padY = (params.imgSize[1] - newHeight) / 2;

        for (int i = 0; i < numPredictions; ++i) {
            float cx_norm = output[0 * numPredictions + i];
            float cy_norm = output[1 * numPredictions + i];
            float w_norm = output[2 * numPredictions + i];
            float h_norm = output[3 * numPredictions + i];

            // Multi-class: En yüksek confidence'lı sınıfı bul
            int bestClassId = 0;
            float bestConfidence = 0.0f;
            for (int c = 0; c < numClasses; c++) {
                float classConf = output[(4 + c) * numPredictions + i];
                if (classConf > bestConfidence) {
                    bestConfidence = classConf;
                    bestClassId = c;
                }
            }
            float confidence = bestConfidence;

            if (confidence >= params.rectConfidenceThreshold) {
                float cx_pixel = cx_norm;
                float cy_pixel = cy_norm;
                float w_pixel = w_norm;
                float h_pixel = h_norm;

                float x1_unpadded = (cx_pixel - w_pixel / 2.0f) - padX;
                float y1_unpadded = (cy_pixel - h_pixel / 2.0f) - padY;

                float x1_original = x1_unpadded / scale;
                float y1_original = y1_unpadded / scale;
                float w_original = w_pixel / scale;
                float h_original = h_pixel / scale;

                int x = static_cast<int>(x1_original);
                int y = static_cast<int>(y1_original);
                int width = static_cast<int>(w_original);
                int height = static_cast<int>(h_original);

                // Clamp to image boundaries
                if (x < 0) { width += x; x = 0; }
                if (y < 0) { height += y; y = 0; }
                if (x >= originalWidth) { x = originalWidth - 1; width = 1; }
                if (y >= originalHeight) { y = originalHeight - 1; height = 1; }
                if (x + width > originalWidth) { width = originalWidth - x; }
                if (y + height > originalHeight) { height = originalHeight - y; }

                if (width > 0 && height > 0) {
                    boxes.push_back(cv::Rect(x, y, width, height));
                    confidences.push_back(confidence);
                    classIds.push_back(bestClassId);
                }
            }
        }

        // Apply Non-Maximum Suppression
        if (!boxes.empty()) {
            std::vector<int> indices;
            cv::dnn::NMSBoxes(boxes, confidences, params.rectConfidenceThreshold,
                              params.iouThreshold, indices);

            for (size_t i = 0; i < indices.size(); i++) {
                int idx = indices[i];
                DL_RESULT result;
                result.classId = classIds[idx];
                result.confidence = confidences[idx];
                result.box = boxes[idx];
                result.className = (result.classId < static_cast<int>(classes.size()))
                                   ? classes[result.classId] : "Unknown";
                results.push_back(result);
            }
        }

    } catch (const std::exception& e) {
        // Silent fail
    }
}

void YOLO_V8::RunSession(cv::Mat& inputImg, std::vector<DL_RESULT>& results) {
    results.clear();

    if (!session || inputImg.empty()) return;

    try {
        float* blob = nullptr;

        preprocessImage(inputImg, blob);
        if (!blob) return;

        std::vector<int64_t> inputShape = {1, 3, params.imgSize[1], params.imgSize[0]};
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
        results.clear();
    }
}

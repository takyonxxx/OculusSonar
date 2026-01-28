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

        // YOLOv8 output shape'ini kontrol et ve logla
        auto outputInfo = session->GetOutputTypeInfo(0);
        auto tensorInfo = outputInfo.GetTensorTypeAndShapeInfo();
        auto shape = tensorInfo.GetShape();

        qDebug() << "=== YOLO Model Info ===";
        qDebug() << "Output shape size:" << shape.size();
        for (size_t i = 0; i < shape.size(); i++) {
            qDebug() << "  Dim" << i << ":" << shape[i];
        }

        classes = {"Object"};

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

        // Letterbox resize - preserve aspect ratio with padding
        int targetWidth = params.imgSize[0];
        int targetHeight = params.imgSize[1];

        float scale = std::min((float)targetWidth / rgbImg.cols,
                               (float)targetHeight / rgbImg.rows);

        int newWidth = (int)(rgbImg.cols * scale);
        int newHeight = (int)(rgbImg.rows * scale);

        cv::Mat resized;
        cv::resize(rgbImg, resized, cv::Size(newWidth, newHeight));

        // Create padded image (letterbox)
        cv::Mat padded(targetHeight, targetWidth, CV_8UC3, cv::Scalar(114, 114, 114));

        int padX = (targetWidth - newWidth) / 2;
        int padY = (targetHeight - newHeight) / 2;

        resized.copyTo(padded(cv::Rect(padX, padY, newWidth, newHeight)));

        qDebug() << "Letterbox: original=" << rgbImg.cols << "x" << rgbImg.rows
                 << ", resized=" << newWidth << "x" << newHeight
                 << ", pad=(" << padX << "," << padY << ")";

        // Normalize to [0, 1]
        padded.convertTo(padded, CV_32FC3, 1.0 / 255.0);

        int channels = 3;
        int width = targetWidth;
        int height = targetHeight;
        size_t totalSize = channels * width * height;

        blob = new float[totalSize];
        if (!blob) return;

        // Convert to CHW format (NCHW without batch)
        for (int c = 0; c < channels; ++c) {
            for (int h = 0; h < height; ++h) {
                for (int w = 0; w < width; ++w) {
                    blob[c * width * height + h * width + w] =
                        padded.at<cv::Vec3f>(h, w)[c];
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
    if (!output) {
        qDebug() << "Output is NULL";
        return;
    }

    try {
        std::vector<cv::Rect> boxes;
        std::vector<float> confidences;
        std::vector<int> classIds;

        // YOLOv8 output shape: [1, num_features, num_predictions]
        auto outputInfo = session->GetOutputTypeInfo(0);
        auto tensorInfo = outputInfo.GetTensorTypeAndShapeInfo();
        auto shape = tensorInfo.GetShape();

        if (shape.size() < 3) {
            qDebug() << "Invalid output shape size:" << shape.size();
            return;
        }

        int numFeatures = static_cast<int>(shape[1]);
        int numPredictions = static_cast<int>(shape[2]);

        qDebug() << "=== Postprocess Info ===";
        qDebug() << "Num Features:" << numFeatures << "(Expected: 4 bbox + 1 conf = 5)";
        qDebug() << "Num Predictions:" << numPredictions;
        qDebug() << "Original Image:" << originalWidth << "x" << originalHeight;
        qDebug() << "Model Input Size:" << params.imgSize[0] << "x" << params.imgSize[1];
        qDebug() << "Confidence Threshold:" << params.rectConfidenceThreshold << "(TUNED for better recall)";
        qDebug() << "IOU Threshold:" << params.iouThreshold;

        // Calculate letterbox parameters
        float scale = std::min((float)params.imgSize[0] / originalWidth,
                               (float)params.imgSize[1] / originalHeight);

        int newWidth = (int)(originalWidth * scale);
        int newHeight = (int)(originalHeight * scale);
        int padX = (params.imgSize[0] - newWidth) / 2;
        int padY = (params.imgSize[1] - newHeight) / 2;

        qDebug() << "Letterbox params: scale=" << scale
                 << ", padded_size=" << newWidth << "x" << newHeight
                 << ", offset=(" << padX << "," << padY << ")";

        int validBoxes = 0;
        int highConfBoxes = 0;
        int zeroSizeRejected = 0;
        int outOfBoundsRejected = 0;
        int acceptedBoxes = 0;

        qDebug() << "\n=== First 10 High-Confidence Detections ===";

        for (int i = 0; i < numPredictions; ++i) {
            float cx_norm = output[0 * numPredictions + i];
            float cy_norm = output[1 * numPredictions + i];
            float w_norm = output[2 * numPredictions + i];
            float h_norm = output[3 * numPredictions + i];
            float confidence = output[4 * numPredictions + i];

            validBoxes++;

            if (confidence >= params.rectConfidenceThreshold) {
                highConfBoxes++;

                // Model already outputs pixel coordinates (0-640), NOT normalized!
                // No need to multiply by 640
                float cx_pixel = cx_norm;  // Already in pixels
                float cy_pixel = cy_norm;  // Already in pixels
                float w_pixel = w_norm;    // Already in pixels
                float h_pixel = h_norm;    // Already in pixels

                // Remove letterbox padding (still in 640x640 space)
                float x1_padded = cx_pixel - w_pixel / 2.0f;
                float y1_padded = cy_pixel - h_pixel / 2.0f;

                // Adjust for padding offset
                float x1_unpadded = x1_padded - padX;
                float y1_unpadded = y1_padded - padY;

                // Scale from resized dimension to original image size
                float x1_original = x1_unpadded / scale;
                float y1_original = y1_unpadded / scale;
                float w_original = w_pixel / scale;
                float h_original = h_pixel / scale;

                int x = static_cast<int>(x1_original);
                int y = static_cast<int>(y1_original);
                int width = static_cast<int>(w_original);
                int height = static_cast<int>(h_original);

                // Debug first 10 high-confidence detections BEFORE any filtering
                if (highConfBoxes <= 10) {
                    qDebug() << "\nDetection #" << highConfBoxes << ":";
                    qDebug() << "  Confidence:" << QString::number(confidence, 'f', 3);
                    qDebug() << "  RAW (model output): cx=" << QString::number(cx_norm, 'f', 4)
                             << "cy=" << QString::number(cy_norm, 'f', 4)
                             << "w=" << QString::number(w_norm, 'f', 4)
                             << "h=" << QString::number(h_norm, 'f', 4);
                    qDebug() << "  Pixel (640x640): cx=" << QString::number(cx_pixel, 'f', 1)
                             << "cy=" << QString::number(cy_pixel, 'f', 1)
                             << "w=" << QString::number(w_pixel, 'f', 1)
                             << "h=" << QString::number(h_pixel, 'f', 1);
                    qDebug() << "  After unpad: x1=" << QString::number(x1_unpadded, 'f', 1)
                             << "y1=" << QString::number(y1_unpadded, 'f', 1);
                    qDebug() << "  BEFORE clamp: x=" << x << "y=" << y
                             << "w=" << width << "h=" << height;
                }

                // Clamp to image boundaries
                bool wasOutOfBounds = false;
                if (x < 0) {
                    width += x;
                    x = 0;
                    wasOutOfBounds = true;
                }
                if (y < 0) {
                    height += y;
                    y = 0;
                    wasOutOfBounds = true;
                }
                if (x >= originalWidth) {
                    wasOutOfBounds = true;
                    x = originalWidth - 1;
                    width = 1;
                }
                if (y >= originalHeight) {
                    wasOutOfBounds = true;
                    y = originalHeight - 1;
                    height = 1;
                }
                if (x + width > originalWidth) {
                    width = originalWidth - x;
                }
                if (y + height > originalHeight) {
                    height = originalHeight - y;
                }

                if (highConfBoxes <= 10) {
                    qDebug() << "  AFTER clamp: x=" << x << "y=" << y
                             << "w=" << width << "h=" << height;
                }

                // False positive filtering
                if (width > 0 && height > 0) {
                    // Calculate aspect ratio and relative size
                    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
                    float boxArea = static_cast<float>(width * height);
                    float imageArea = static_cast<float>(originalWidth * originalHeight);
                    float areaRatio = boxArea / imageArea;

                    bool rejectBox = false;
                    QString rejectReason;

                    // Sonar-specific: Reject detections in near-field/water column area
                    // False positives consistently appear at y=800-900 (40-45% from top)
                    // Real objects appear at y>1400 (>70% from top)
                    float yCenter = y + height / 2.0f;
                    float yRatioFromTop = yCenter / static_cast<float>(originalHeight);

                    if (yRatioFromTop < 0.60f) {  // Upper 60% of image = near-field artifacts
                        rejectBox = true;
                        rejectReason = QString("near-field artifact (y=%1, %2% from top)")
                                           .arg(static_cast<int>(yCenter))
                                           .arg(static_cast<int>(yRatioFromTop * 100));
                    }
                    // Reject extremely elongated boxes (likely false positives)
                    else if (aspectRatio > 6.0f) {
                        rejectBox = true;
                        rejectReason = QString("too wide (AR=%1)").arg(aspectRatio, 0, 'f', 2);
                    } else if (aspectRatio < 0.15f) {
                        rejectBox = true;
                        rejectReason = QString("too tall (AR=%1)").arg(aspectRatio, 0, 'f', 2);
                    }
                    // Reject boxes that are too large (>85% of image)
                    else if (areaRatio > 0.85f) {
                        rejectBox = true;
                        rejectReason = QString("too large (%1% of image)").arg(areaRatio * 100, 0, 'f', 1);
                    }

                    if (!rejectBox) {
                        boxes.push_back(cv::Rect(x, y, width, height));
                        confidences.push_back(confidence);
                        classIds.push_back(0);
                        acceptedBoxes++;

                        if (highConfBoxes <= 10) {
                            qDebug() << "  AR=" << QString::number(aspectRatio, 'f', 2)
                            << "Area=" << QString::number(areaRatio * 100, 'f', 1) << "%"
                            << "Y=" << y << "(" << QString::number(yRatioFromTop * 100, 'f', 0) << "% from top)";
                            qDebug() << "  Status: ✓ ACCEPTED";
                        }
                    } else {
                        zeroSizeRejected++;
                        if (highConfBoxes <= 10) {
                            qDebug() << "  Status: ✗ REJECTED (" << rejectReason << ")";
                        }
                    }
                } else {
                    zeroSizeRejected++;
                    if (highConfBoxes <= 10) {
                        qDebug() << "  Status: ✗ REJECTED (zero/negative size)";
                    }
                }
            }
        }

        qDebug() << "\n=== Summary ===";
        qDebug() << "Valid boxes checked:" << validBoxes;
        qDebug() << "High confidence boxes (>=" << params.rectConfidenceThreshold << "):" << highConfBoxes;
        qDebug() << "Accepted boxes:" << acceptedBoxes;
        qDebug() << "Rejected - size/shape issues:" << zeroSizeRejected;
        qDebug() << "Boxes for NMS:" << boxes.size();

        // Apply Non-Maximum Suppression
        if (!boxes.empty()) {
            std::vector<int> indices;
            cv::dnn::NMSBoxes(boxes, confidences, params.rectConfidenceThreshold,
                              params.iouThreshold, indices);

            qDebug() << "After NMS:" << indices.size() << "detections";

            for (size_t i = 0; i < indices.size(); i++) {
                int idx = indices[i];
                DL_RESULT result;
                result.classId = 0;
                result.confidence = confidences[idx];
                result.box = boxes[idx];
                results.push_back(result);

                qDebug() << "\nFinal Detection #" << (i+1) << ":";
                qDebug() << "  Box: [" << result.box.x << "," << result.box.y
                         << "," << result.box.width << "," << result.box.height << "]";
                qDebug() << "  Confidence:" << result.confidence;
            }
        } else {
            qDebug() << "No boxes passed filters";
        }

        qDebug() << "=== Inference Complete ===\n";

    } catch (const std::exception& e) {
        qDebug() << "Postprocess error:" << e.what();
    }
}

void YOLO_V8::RunSession(cv::Mat& inputImg, std::vector<DL_RESULT>& results) {
    results.clear();

    if (!session || inputImg.empty()) {
        qDebug() << "Session is NULL or input image is empty";
        return;
    }

    qDebug() << "\n=== Running YOLO Inference ===";
    qDebug() << "Input image size:" << inputImg.cols << "x" << inputImg.rows;

    try {
        float* blob = nullptr;

        preprocessImage(inputImg, blob);
        if (!blob) {
            qDebug() << "Preprocessing failed";
            return;
        }

        std::vector<int64_t> inputShape = {1, 3, params.imgSize[1], params.imgSize[0]};
        size_t tensorSize = 3 * params.imgSize[0] * params.imgSize[1];

        qDebug() << "Input tensor shape: [" << inputShape[0] << "," << inputShape[1]
                 << "," << inputShape[2] << "," << inputShape[3] << "]";

        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(
            OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);

        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, blob, tensorSize,
            inputShape.data(), inputShape.size());

        qDebug() << "Running inference...";
        auto outputTensors = session->Run(
            Ort::RunOptions{nullptr},
            inputNodeNames.data(), &inputTensor, 1,
            outputNodeNames.data(), 1);

        float* outputData = outputTensors[0].GetTensorMutableData<float>();
        if (!outputData) {
            qDebug() << "Output data is NULL";
            delete[] blob;
            return;
        }

        qDebug() << "Inference completed, postprocessing...";
        postprocessOutput(outputData, results, inputImg.cols, inputImg.rows);

        delete[] blob;

    } catch (const std::exception& e) {
        qDebug() << "YOLO error:" << e.what();
        results.clear();
    }
}

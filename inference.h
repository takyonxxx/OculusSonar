#ifndef INFERENCE_H
#define INFERENCE_H

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <string>

struct DL_INIT_PARAM {
    std::string modelPath;
    int modelType;
    std::vector<int> imgSize = {640, 640};

    float rectConfidenceThreshold = 0.15f;
    float iouThreshold = 0.45f;
    std::vector<std::string> classNames;

    // Box Geometry Filters
    float minAspectRatio = 0.15f;
    float maxAspectRatio = 3.0f;
    int minBoxArea = 200;
    int maxBoxArea = 15000;
    float minSquareness = 0.15f;

    // Sonar Range Filter (radial)
    bool enableSonarFilter = false;
    float sonarMinRange = 0.15f;
    float sonarMaxRange = 0.95f;
    float sonarOriginY = 1.0f;

    // Y Position Filter (simple vertical filter)
    bool enableYFilter = false;
    float minYRatio = 0.0f;
    float maxYRatio = 1.0f;
};

struct DL_RESULT {
    int classId;
    float confidence;
    cv::Rect box;
    std::string className;
};

enum MODEL_TYPE {
    YOLO_DETECT_V8 = 1
};

class YOLO_V8 {
public:
    YOLO_V8();
    ~YOLO_V8();

    bool CreateSession(DL_INIT_PARAM& params);
    void RunSession(cv::Mat& inputImg, std::vector<DL_RESULT>& results);

    std::vector<std::string> classes;

private:
    Ort::Env env{nullptr};
    Ort::SessionOptions sessionOptions;
    Ort::Session* session{nullptr};

    std::vector<const char*> inputNodeNames;
    std::vector<const char*> outputNodeNames;
    std::vector<Ort::AllocatedStringPtr> inputNodeNamesPtr;
    std::vector<Ort::AllocatedStringPtr> outputNodeNamesPtr;

    DL_INIT_PARAM params;

    void preprocessImage(cv::Mat& img, float*& blob);
    void postprocessOutput(float* output, std::vector<DL_RESULT>& results,
                           int originalWidth, int originalHeight);
    bool passesGeometryFilter(const cv::Rect& box, int imgWidth, int imgHeight);
};

#endif // INFERENCE_H

#ifndef INFERENCE_H
#define INFERENCE_H

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <string>

enum MODEL_TYPE {
    YOLO_DETECT_V8 = 1
};

struct DL_INIT_PARAM {
    std::string modelPath;
    int modelType = YOLO_DETECT_V8;
    std::vector<int> imgSize = {640, 640};
    float rectConfidenceThreshold = 0.25f;  // Düşük tut - model küçük objeler için
    float iouThreshold = 0.45f;
    std::vector<std::string> classNames = {"Object"};

    // Box Geometry Filters - VERİDEN HESAPLANDI
    float minAspectRatio = 0.20f;     // Data min: 0.27, margin ile
    float maxAspectRatio = 2.50f;     // Data max: 1.76, margin ile
    int minBoxArea = 150;              // Data min: 331, margin ile
    int maxBoxArea = 10000;            // Data max: 4853, margin ile
    float minSquareness = 0.15f;       // Data min: 0.27, margin ile

    // Sonar Geometry Filters
    bool enableSonarFilter = true;
    float sonarMinRange = 0.20f;       // Data min: 0.33, margin ile
    float sonarMaxRange = 1.00f;       // Data max: 0.87, margin ile (disable upper)
    float sonarOriginY = 1.0f;         // Sonar kaynağı altta
};

struct DL_RESULT {
    int classId;
    float confidence;
    cv::Rect box;
    std::string className;
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

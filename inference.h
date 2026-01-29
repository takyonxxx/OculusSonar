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
    float rectConfidenceThreshold = 0.4f;  // Küçük objeler için biraz düşük tut
    float iouThreshold = 0.4f;              // Küçük kutularda overlap hassas
    std::vector<std::string> classNames;

    // Box Geometry Filters - 40x40 hedef için
    float minAspectRatio = 0.5f;    // 1:2 oranına kadar izin ver (20x40)
    float maxAspectRatio = 2.0f;    // 2:1 oranına kadar izin ver (40x20)
    int minBoxArea = 400;           // ~20x20 - hedefin yarısı
    int maxBoxArea = 10000;          // ~100x100 - hedefin 2 katı
    float minSquareness = 0.4f;     // Kutu benzeri şekiller için sıkı tut

    // Sonar Geometry Filters
    bool enableSonarFilter = true;
    float sonarMinRange = 0.06f;    // 640'ın %6'sı ≈ 38px merkez dead zone
    float sonarMaxRange = 0.94f;    // Kenar halkalarını at
    float sonarOriginY = 1.0f;
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

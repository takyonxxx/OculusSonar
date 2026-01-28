#ifndef INFERENCE_H
#define INFERENCE_H

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <string>

struct DL_INIT_PARAM {
    std::string modelPath;
    int modelType;
    std::vector<int> imgSize;
    float rectConfidenceThreshold;
    float iouThreshold;
};

struct DL_RESULT {
    int classId;
    float confidence;
    cv::Rect box;
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
    
    // Helper functions
    void preprocessImage(cv::Mat& img, float*& blob);
    void postprocessOutput(float* output, std::vector<DL_RESULT>& results, 
                          int originalWidth, int originalHeight);
};

#endif // INFERENCE_H

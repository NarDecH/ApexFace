// ApexFace - YuNet face detection (cv::FaceDetectorYN) with CPU / CUDA backends
#pragma once
#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/objdetect.hpp>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Detection {
    cv::Rect box;  // coordinates in the ORIGINAL image space
    float conf = 0;
};

enum class Backend { Auto, CPU, CUDA };

class FaceDetector {
public:
    // Loads the ONNX model and warms the requested backend up.
    // backendOut receives "cuda" or "cpu" (what actually got activated).
    bool init(const fs::path& modelPath, Backend pref, std::string& backendOut, std::string& errorOut);

    // Returns detections sorted by confidence (descending).
    std::vector<Detection> detect(const cv::Mat& bgr, double& inferMs);

    void setMinFacePx(int px) { minFacePx_ = px; }
    void setScoreTh(float th) { scoreTh_ = th; }
    const std::string& backendName() const { return backendName_; }

private:
    bool createNet(const fs::path& modelPath, bool cuda, std::string& errorOut);

    cv::Ptr<cv::FaceDetectorYN> net_;
    fs::path modelPath_;
    std::mutex mtx_; // needed when one instance is shared across worker threads (CUDA mode)
    std::string backendName_ = "cpu";
    float scoreTh_ = 0.6f;
    int minFacePx_ = 28;
    int maxDim_ = 640;
};

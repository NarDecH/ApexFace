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
    // YuNet landmarks, original-image space:
    // [0]=right eye [1]=left eye [2]=nose tip [3]=right mouth corner [4]=left mouth corner
    cv::Point2f lm[5]{};
};

// Facial-component completeness derived from the landmarks: a face whose eyes
// or mouth fall outside the face box (cropped by the frame) or whose landmark
// geometry collapses (strong profile) is missing components the user cares
// about, and is treated as grade D.
struct FaceComponents {
    bool rightEye = false;
    bool leftEye = false;
    bool nose = false;    // informational; not required for "complete"
    bool mouth = false;
    bool all() const { return rightEye && leftEye && mouth; }
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

    // Landmark-based completeness check for one detection. A landmark outside
    // the image bounds (negative coords / past the frame) is never visible,
    // e.g. an eye cropped by the frame edge.
    static FaceComponents componentsOf(const Detection& d, const cv::Size& imgSize);
    // Human-readable list of the missing parts, e.g. "an eye + mouth" ("" if complete).
    static std::string missingText(const FaceComponents& c);

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

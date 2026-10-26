#include "FaceDetector.h"

#include "Logger.h"
#include "Platform.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>

bool FaceDetector::createNet(const fs::path& modelPath, bool cuda, std::string& errorOut) {
    try {
        net_ = cv::FaceDetectorYN::create(
            modelPath.string(), "",
            {320, 320}, 0.6f, 0.3f, 5000,
            cuda ? cv::dnn::DNN_BACKEND_CUDA : cv::dnn::DNN_BACKEND_OPENCV,
            cuda ? cv::dnn::DNN_TARGET_CUDA : cv::dnn::DNN_TARGET_CPU);
    } catch (const cv::Exception& e) {
        errorOut = std::string("FaceDetectorYN::create failed: ") + e.what();
        net_.release();
        return false;
    }
    return true;
}

bool FaceDetector::init(const fs::path& modelPath, Backend pref, std::string& backendOut,
                        std::string& errorOut) {
    errorOut.clear();
    modelPath_ = modelPath;
    if (!fs::exists(modelPath)) {
        errorOut = "model not found: " + platform::utf8str(modelPath);
        return false;
    }
    if (pref != Backend::CPU) {
        if (createNet(modelPath, true, errorOut)) {
            double ms = 0;
            cv::Mat dummy(320, 320, CV_8UC3, cv::Scalar(180, 180, 180));
            try {
                cv::Mat faces;
                net_->setInputSize(dummy.size());
                net_->detect(dummy, faces);
                backendName_ = "cuda";
                backendOut = backendName_;
                AF_INFO("detect", "DNN backend = CUDA (GPU acceleration active)");
                return true;
            } catch (const cv::Exception& e) {
                AF_WARN("detect", "CUDA warmup failed (" << e.what() << "), falling back to CPU");
            }
        } else {
            AF_WARN("detect", "CUDA backend unavailable (" << errorOut << "), falling back to CPU");
        }
    }
    if (!createNet(modelPath, false, errorOut)) return false;
    double ms = 0;
    cv::Mat dummy(320, 320, CV_8UC3, cv::Scalar(180, 180, 180));
    try {
        cv::Mat faces;
        net_->setInputSize(dummy.size());
        net_->detect(dummy, faces);
    } catch (const cv::Exception& e) {
        errorOut = std::string("CPU warmup failed: ") + e.what();
        return false;
    }
    backendName_ = "cpu";
    backendOut = backendName_;
    AF_INFO("detect", "DNN backend = CPU");
    return true;
}

std::vector<Detection> FaceDetector::detect(const cv::Mat& bgr, double& inferMs) {
    inferMs = 0;
    std::vector<Detection> out;
    if (bgr.empty() || !net_) return out;

    std::lock_guard<std::mutex> lock(mtx_);

    double scale = std::min(1.0, (double)maxDim_ / (double)std::max(bgr.cols, bgr.rows));
    int w = std::max(160, (int)std::lround(bgr.cols * scale / 32.0) * 32);
    int h = std::max(160, (int)std::lround(bgr.rows * scale / 32.0) * 32);

    cv::Mat small;
    if (w != bgr.cols || h != bgr.rows) {
        cv::resize(bgr, small, {w, h}, 0, 0, cv::INTER_AREA);
    } else {
        small = bgr;
    }

    auto t0 = std::chrono::steady_clock::now();
    cv::Mat faces;
    try {
        net_->setInputSize({w, h});
        net_->detect(small, faces);
    } catch (const cv::Exception&) {
        throw; // let the caller log it with file context
    }
    auto t1 = std::chrono::steady_clock::now();
    inferMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    if (faces.empty() || faces.cols < 15) return out;
    double invX = (double)bgr.cols / w, invY = (double)bgr.rows / h;
    for (int i = 0; i < faces.rows; ++i) {
        const float* r = faces.ptr<float>(i);
        float conf = r[14];
        if (conf < scoreTh_) continue;
        int x = std::max(0, (int)std::lround(r[0] * invX));
        int y = std::max(0, (int)std::lround(r[1] * invY));
        int bw = (int)std::lround(r[2] * invX);
        int bh = (int)std::lround(r[3] * invY);
        bw = std::min(bgr.cols - x, bw);
        bh = std::min(bgr.rows - y, bh);
        if (bw < minFacePx_ || bh < minFacePx_) continue;
        Detection det;
        det.box = {x, y, bw, bh};
        det.conf = conf;
        // landmarks: [re, le, nose, r-mouth, l-mouth] in the scaled input space
        for (int k = 0; k < 5; ++k) {
            det.lm[k] = {(float)(r[4 + 2 * k] * invX), (float)(r[5 + 2 * k] * invY)};
        }
        out.push_back(std::move(det));
    }
    std::sort(out.begin(), out.end(), [](const Detection& a, const Detection& b) { return a.conf > b.conf; });
    return out;
}

FaceComponents FaceDetector::componentsOf(const Detection& d, const cv::Size& imgSize) {
    FaceComponents c;
    const cv::Rect& b = d.box;
    const float tolX = 0.10f * b.width, tolY = 0.10f * b.height;
    auto inside = [&](const cv::Point2f& p) {
        // outside the image frame = the part is cropped away, never visible
        if (p.x < 0 || p.y < 0 || p.x >= imgSize.width || p.y >= imgSize.height) return false;
        return p.x >= b.x - tolX && p.x <= b.x + b.width + tolX && p.y >= b.y - tolY &&
               p.y <= b.y + b.height + tolY;
    };
    c.rightEye = inside(d.lm[0]);
    c.leftEye = inside(d.lm[1]);
    c.nose = inside(d.lm[2]);
    c.mouth = inside(d.lm[3]) && inside(d.lm[4]);
    // degenerate geometry (landmarks bunching up in a strong profile) counts as missing too
    const float eyeDist = (float)cv::norm(d.lm[0] - d.lm[1]);
    const float mouthDist = (float)cv::norm(d.lm[3] - d.lm[4]);
    if (eyeDist < 0.15f * b.width) c.rightEye = c.leftEye = false;
    if (mouthDist < 0.15f * b.width) c.mouth = false;
    if (getenv("APEXFACE_DEBUG_ORI"))
        fprintf(stderr, "[components] box=(%d,%d %dx%d) re=(%.0f,%.0f) le=(%.0f,%.0f) nose=(%.0f,%.0f) "
                        "rm=(%.0f,%.0f) lm=(%.0f,%.0f) -> eyes=%d%d mouth=%d\n",
                b.x, b.y, b.width, b.height, d.lm[0].x, d.lm[0].y, d.lm[1].x, d.lm[1].y,
                d.lm[2].x, d.lm[2].y, d.lm[3].x, d.lm[3].y, d.lm[4].x, d.lm[4].y,
                (int)c.rightEye, (int)c.leftEye, (int)c.mouth);
    return c;
}

std::string FaceDetector::missingText(const FaceComponents& c) {
    std::string s;
    if (!c.rightEye && !c.leftEye) s = "eyes";
    else if (!c.rightEye || !c.leftEye) s = "an eye";
    if (!c.mouth) {
        if (!s.empty()) s += " + ";
        s += "mouth";
    }
    return s;
}

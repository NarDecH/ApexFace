#include "Sharpness.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace sharp {

static Config g_cfg = {/*kLap*/ 350.0, /*kTen*/ 1200.0, /*fftRef*/ 0.0018,
                       /*wLap*/ 0.55, /*wTen*/ 0.35, /*wFft*/ 0.10,
                       /*smallFacePx*/ 96, /*smallFaceFloor*/ 0.55};

const Config& config() { return g_cfg; }

static cv::Rect expandClamp(const cv::Mat& img, const cv::Rect& box, int marginPct) {
    int mx = std::max(2, box.width * marginPct / 100);
    int my = std::max(2, box.height * marginPct / 100);
    int x = std::max(0, box.x - mx);
    int y = std::max(0, box.y - my);
    int x2 = std::min(img.cols, box.x + box.width + mx);
    int y2 = std::min(img.rows, box.y + box.height + my);
    if (x2 - x < 4 || y2 - y < 4) return {};
    return {x, y, x2 - x, y2 - y};
}

Metrics computeMetrics(const cv::Mat& bgr, const cv::Rect& faceBox, int marginPct) {
    Metrics m;
    cv::Rect roi = expandClamp(bgr, faceBox, marginPct);
    if (roi.empty()) return m;
    cv::Mat crop = bgr(roi);

    cv::Mat gray;
    cv::cvtColor(crop, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, {3, 3}, 0.8);

    // 1) Variance of Laplacian
    cv::Mat lap;
    cv::Laplacian(gray, lap, CV_32F, 3);
    cv::Scalar mean, sd;
    cv::meanStdDev(lap, mean, sd);
    m.lapVar = (double)sd.val[0] * sd.val[0];

    // 2) Tenengrad: mean of squared Sobel gradient magnitude
    cv::Mat gx, gy;
    cv::Sobel(gray, gx, CV_32F, 1, 0, 3);
    cv::Sobel(gray, gy, CV_32F, 0, 1, 3);
    cv::Mat g2 = gx.mul(gx) + gy.mul(gy);
    cv::meanStdDev(g2, mean, sd);
    m.tenengrad = (double)mean.val[0];

    // 3) FFT high-frequency energy ratio (on a 256x256 normalized crop)
    cv::Mat f256;
    cv::resize(gray, f256, {256, 256}, 0, 0, cv::INTER_AREA);
    f256.convertTo(f256, CV_32F);
    cv::dft(f256, f256, cv::DFT_COMPLEX_OUTPUT);
    // split into re/im planes and compute power spectrum on centered layout
    std::vector<cv::Mat> ch;
    cv::split(f256, ch);
    cv::Mat pow = ch[0].mul(ch[0]) + ch[1].mul(ch[1]);
    // quadrant-shifted energy: compare pixels whose distance from DC (wrapped) > 64
    double total = 0, hf = 0;
    for (int y = 0; y < 256; ++y) {
        const float* row = pow.ptr<float>(y);
        int dy = (y + 128) % 256 - 128;
        for (int x = 0; x < 256; ++x) {
            int dx = (x + 128) % 256 - 128;
            double p = row[x];
            total += p;
            if (dx * dx + dy * dy > 64 * 64) hf += p;
        }
    }
    m.fftHF = total > 0 ? hf / total : 0.0;

    // 4) RMS contrast
    cv::meanStdDev(gray, mean, sd);
    m.contrastRMS = (double)sd.val[0];
    return m;
}

static double soft(double v, double k) { return 100.0 * v / (v + k); }

Score scoreFace(const cv::Mat& bgr, const cv::Rect& faceBox, const Config* cfgOverride) {
    const Config& c = cfgOverride ? *cfgOverride : g_cfg;
    Score s;
    s.raw = computeMetrics(bgr, faceBox);
    s.lapScore = soft(s.raw.lapVar, c.kLap);
    s.tenScore = soft(s.raw.tenengrad, c.kTen);
    s.fftScore = 100.0 * std::clamp(s.raw.fftHF / c.fftRef, 0.0, 1.0);

    // Attenuate tiny faces: a 40px face carries little detail no matter what.
    double fw = (double)std::max(faceBox.width, 1);
    s.resFactor = fw >= c.smallFacePx
                      ? 1.0
                      : c.smallFaceFloor + (1.0 - c.smallFaceFloor) * (fw / c.smallFacePx);

    double v = c.wLap * s.lapScore + c.wTen * s.tenScore + c.wFft * s.fftScore;
    s.final0to100 = std::clamp(v * s.resFactor, 0.0, 100.0);
    return s;
}

char ratingLetter(double score) {
    if (score >= 72) return 'A';
    if (score >= 58) return 'B';
    if (score >= 42) return 'C';
    return 'D';
}

bool ratingPlus(double score) { return score >= 85; }

const char* ratingLabel(double score) {
    if (score >= 85) return "A+";
    if (score >= 72) return "A";
    if (score >= 58) return "B";
    if (score >= 42) return "C";
    return "D";
}

const char* ratingHex(double score) {
    if (score >= 85) return "#10b981"; // emerald
    if (score >= 72) return "#4ade80"; // green
    if (score >= 58) return "#38bdf8"; // sky
    if (score >= 42) return "#f59e0b"; // amber
    return "#ef4444";                  // red
}

cv::Scalar ratingBgr(double score) {
    if (score >= 85) return {129, 185, 16};  // #10b981
    if (score >= 72) return {128, 222, 74};  // #4ade80
    if (score >= 58) return {248, 189, 56};  // #38bdf8
    if (score >= 42) return {11, 158, 245};  // #f59e0b
    return {68, 68, 239};                    // #ef4444
}

} // namespace sharp

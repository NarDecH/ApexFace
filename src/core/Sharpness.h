// ApexFace - face sharpness metrics and scoring
#pragma once
#include <opencv2/core.hpp>

namespace sharp {

// Raw per-face measurements (all computed on the full-resolution crop).
struct Metrics {
    double lapVar = 0;      // variance of Laplacian (blur sensitivity)
    double tenengrad = 0;   // mean squared Sobel magnitude
    double fftHF = 0;       // high-frequency energy ratio (0..1)
    double contrastRMS = 0; // RMS contrast (std of gray)
};

struct Score {
    double final0to100 = 0;
    double lapScore = 0, tenScore = 0, fftScore = 0;
    double resFactor = 1;
    Metrics raw;
};

// Calibration constants; documented in docs/RESEARCH.md and calibrated on real
// Sony A7M3 race photography + synthetic Gaussian blur series.
struct Config {
    double kLap = 80.0;    // soft-saturation point for Laplacian variance
    double kTen = 1200.0;  // soft-saturation point for Tenengrad
    double fftRef = 0.42;  // HF ratio considered "fully sharp"
    double wLap = 0.55, wTen = 0.35, wFft = 0.10;
    int smallFacePx = 96;  // below this width the score is attenuated
    double smallFaceFloor = 0.55;
};

const Config& config();

Metrics computeMetrics(const cv::Mat& bgrFullRes, const cv::Rect& faceBox, int marginPct = 50);

Score scoreFace(const cv::Mat& bgrFullRes, const cv::Rect& faceBox, const Config* cfgOverride = nullptr);

// Rating scale: A+ >= 85, A >= 72, B >= 58, C >= 42, D otherwise.
char ratingLetter(double score);       // 'A'/'B'/'C'/'D' ('+' from plusLetter)
bool ratingPlus(double score);
const char* ratingLabel(double score); // "A+","A","B","C","D"
const char* ratingHex(double score);   // CSS color
cv::Scalar ratingBgr(double score);    // OpenCV color

// Grade D boundary: images whose best face scores below this (and images
// where no face was found at all) are treated as rejects.
inline bool isGradeD(double score) { return score < 42.0; }

} // namespace sharp

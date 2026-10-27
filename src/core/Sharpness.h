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

// Face-only scoring (whole-crop metrics).
Score scoreFace(const cv::Mat& bgrFullRes, const cv::Rect& faceBox, const Config* cfgOverride = nullptr);

// Landmark-aware scoring: blends the whole-crop metrics with eye-patch and
// mouth-patch metrics (viewers judge sharpness mostly by eyes and mouth).
// Pass lm = nullptr to score the whole crop only. The soft-saturation mapping
// and constants are unchanged, so the calibrated 0-100 scale is preserved.
Score scoreFace(const cv::Mat& bgrFullRes, const cv::Rect& faceBox, const cv::Point2f* lm /*5*/,
                const Config* cfgOverride = nullptr);

// Rating scale: A+ >= 85, A >= 72, B >= 58, C >= 42, D otherwise.
// Grade scale: A+ >= aPlus, A >= a, B >= b, C >= c, D below c.
// The thresholds are user-configurable (GUI sliders / CLI --grades); the
// setter sanitizes the values so they stay strictly increasing.
struct GradeThresholds {
    double aPlus = 85;
    double a = 72;
    double b = 58;
    double c = 42;
};

const GradeThresholds& grades();
void setGradeThresholds(const GradeThresholds& g); // sorts + clamps to [0,100]

char ratingLetter(double score);       // 'A'/'B'/'C'/'D' ('+' from plusLetter)
bool ratingPlus(double score);
const char* ratingLabel(double score); // "A+","A","B","C","D"
const char* ratingHex(double score);   // CSS color
cv::Scalar ratingBgr(double score);    // OpenCV color

// Grade D boundary (user-configurable): images whose best face scores below
// this - and images where no complete face was found - are treated as rejects.
inline bool isGradeD(double score) { return score < grades().c; }

} // namespace sharp

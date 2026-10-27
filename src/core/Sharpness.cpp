#include "Sharpness.h"
#include "Logger.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace sharp {

static Config g_cfg = {/*kLap*/ 350.0, /*kTen*/ 1200.0, /*fftRef*/ 0.0018,
                       /*wLap*/ 0.55, /*wTen*/ 0.35, /*wFft*/ 0.10,
                       /*smallFacePx*/ 96, /*smallFaceFloor*/ 0.55};

const Config& config() { return g_cfg; }

static GradeThresholds g_grades; // defaults 85/72/58/42

const GradeThresholds& grades() { return g_grades; }

void setGradeThresholds(const GradeThresholds& g) {
    double v[4] = {g.aPlus, g.a, g.b, g.c};
    for (double& x : v) x = std::clamp(x, 0.0, 100.0);
    std::sort(v, v + 4); // ascending -> c, b, a, aPlus
    g_grades.c = v[0];
    g_grades.b = v[1];
    g_grades.a = v[2];
    g_grades.aPlus = v[3];
    AF_DEBUG("grades", "thresholds: A+>=" << g_grades.aPlus << " A>=" << g_grades.a
                                          << " B>=" << g_grades.b << " C>=" << g_grades.c);
}

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

// Laplacian variance + Tenengrad of one small patch (same prefilter pipeline
// as the face crop), for landmark-region weighting.
static bool patchLapTen(const cv::Mat& bgr, cv::Rect roi, double& lapVar, double& ten) {
    roi &= cv::Rect(0, 0, bgr.cols, bgr.rows);
    if (roi.width < 8 || roi.height < 8) return false;
    cv::Mat gray;
    cv::cvtColor(bgr(roi), gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, {3, 3}, 0.8);
    cv::Mat lap;
    cv::Laplacian(gray, lap, CV_32F, 3);
    cv::Scalar mean, sd;
    cv::meanStdDev(lap, mean, sd);
    lapVar = (double)sd.val[0] * sd.val[0];
    cv::Mat gx, gy;
    cv::Sobel(gray, gx, CV_32F, 1, 0, 3);
    cv::Sobel(gray, gy, CV_32F, 0, 1, 3);
    cv::meanStdDev(gx.mul(gx) + gy.mul(gy), mean, sd);
    ten = (double)mean.val[0];
    return true;
}

static cv::Rect patchRect(const cv::Point2f& c, float w, float h, const cv::Mat& bgr) {
    return {cvRound(c.x - w / 2), cvRound(c.y - h / 2), cvRound(w), cvRound(h)};
}

Score scoreFace(const cv::Mat& bgr, const cv::Rect& faceBox, const cv::Point2f* lm,
                const Config* cfgOverride) {
    const Config& c = cfgOverride ? *cfgOverride : g_cfg;
    Score s = scoreFace(bgr, faceBox, cfgOverride);
    if (!lm || bgr.empty()) return s;

    // eye patches (each eye) and mouth patch (between the mouth corners)
    const float pw = 0.42f * faceBox.width, ph = 0.30f * faceBox.height;
    double eLap = 0, eTen = 0, mLap = 0, mTen = 0;
    int eN = 0, mN = 0;
    for (int k = 0; k < 2; ++k) {
        double lv, tn;
        if (patchLapTen(bgr, patchRect(lm[k], pw, ph, bgr), lv, tn)) {
            eLap += lv;
            eTen += tn;
            ++eN;
        }
    }
    cv::Point2f mc((lm[3].x + lm[4].x) / 2, (lm[3].y + lm[4].y) / 2);
    double lv, tn;
    if (patchLapTen(bgr, patchRect(mc, 0.55f * faceBox.width, 0.30f * faceBox.height, bgr), lv, tn)) {
        mLap = lv;
        mTen = tn;
        ++mN;
    }
    if (eN == 0 && mN == 0) return s; // no usable patches -> whole-crop score

    // blend: whole crop 0.75, eyes 0.15, mouth 0.10 (renormalized for missing parts)
    double wBase = 0.75, wEyes = eN ? 0.15 : 0.0, wMouth = mN ? 0.10 : 0.0;
    const double sum = wBase + wEyes + wMouth;
    wBase /= sum;
    wEyes /= sum;
    wMouth /= sum;
    const double lapW = wBase * s.raw.lapVar + wEyes * (eN ? eLap / eN : 0.0) +
                        wMouth * (mN ? mLap / mN : 0.0);
    const double tenW = wBase * s.raw.tenengrad + wEyes * (eN ? eTen / eN : 0.0) +
                        wMouth * (mN ? mTen / mN : 0.0);

    s.lapScore = soft(lapW, c.kLap);
    s.tenScore = soft(tenW, c.kTen);
    const double v = c.wLap * s.lapScore + c.wTen * s.tenScore + c.wFft * s.fftScore;
    s.final0to100 = std::clamp(v * s.resFactor, 0.0, 100.0);
    s.raw.lapVar = lapW;    // the blended values are what the score used
    s.raw.tenengrad = tenW;
    return s;
}

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
    if (score >= g_grades.a) return 'A';
    if (score >= g_grades.b) return 'B';
    if (score >= g_grades.c) return 'C';
    return 'D';
}

bool ratingPlus(double score) { return score >= g_grades.aPlus; }

const char* ratingLabel(double score) {
    if (score >= g_grades.aPlus) return "A+";
    if (score >= g_grades.a) return "A";
    if (score >= g_grades.b) return "B";
    if (score >= g_grades.c) return "C";
    return "D";
}

const char* ratingHex(double score) {
    if (score >= g_grades.aPlus) return "#10b981"; // emerald
    if (score >= g_grades.a) return "#4ade80";     // green
    if (score >= g_grades.b) return "#38bdf8";     // sky
    if (score >= g_grades.c) return "#f59e0b";     // amber
    return "#ef4444";                              // red
}

cv::Scalar ratingBgr(double score) {
    if (score >= g_grades.aPlus) return {129, 185, 16}; // #10b981
    if (score >= g_grades.a) return {128, 222, 74};     // #4ade80
    if (score >= g_grades.b) return {248, 189, 56};     // #38bdf8
    if (score >= g_grades.c) return {11, 158, 245};     // #f59e0b
    return {68, 68, 239};                               // #ef4444
}

} // namespace sharp

// ApexFace core unit tests (run via ctest). Exit code 1 on any failure.
#include "core/FaceDetector.h"
#include "core/ImageIO.h"
#include "core/Sharpness.h"

#include <opencv2/imgproc.hpp>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                                              \
    do {                                                                                         \
        if (!(cond)) {                                                                           \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                               \
            ++failures;                                                                          \
        }                                                                                        \
    } while (0)

int main() {
    // ---- rating boundaries
    CHECK(std::string(sharp::ratingLabel(96.0)) == "A+");
    CHECK(std::string(sharp::ratingLabel(85.0)) == "A+");
    CHECK(std::string(sharp::ratingLabel(84.9)) == "A");
    CHECK(std::string(sharp::ratingLabel(72.0)) == "A");
    CHECK(std::string(sharp::ratingLabel(58.0)) == "B");
    CHECK(std::string(sharp::ratingLabel(42.0)) == "C");
    CHECK(std::string(sharp::ratingLabel(41.9)) == "D");
    CHECK(sharp::isGradeD(41.9));
    CHECK(!sharp::isGradeD(42.0));

    // ---- applyOrientation: 90-degree rotations swap dims, work out of place
    cv::Mat m = cv::Mat::zeros(100, 50, CV_8UC3);
    imageio::applyOrientation(m, 8);
    CHECK(m.cols == 100 && m.rows == 50);
    cv::Mat m2 = cv::Mat::zeros(100, 50, CV_8UC3);
    imageio::applyOrientation(m2, 6);
    CHECK(m2.cols == 100 && m2.rows == 50);
    cv::Mat m3 = cv::Mat::zeros(100, 50, CV_8UC3);
    imageio::applyOrientation(m3, 3);
    CHECK(m3.cols == 50 && m3.rows == 100);

    // ---- sharpness: sharp (noise) must beat a blurred copy; bounds hold
    cv::Mat sharpImg(200, 200, CV_8UC3);
    cv::randu(sharpImg, 0, 255);
    cv::Mat blurredImg;
    cv::GaussianBlur(sharpImg, blurredImg, {0, 0}, 5);
    const cv::Rect box(50, 50, 100, 100);
    const double sSharp = sharp::scoreFace(sharpImg, box).final0to100;
    const double sBlur = sharp::scoreFace(blurredImg, box).final0to100;
    CHECK(sSharp > sBlur);
    CHECK(sSharp >= 0.0 && sSharp <= 100.0);
    CHECK(sBlur >= 0.0 && sBlur <= 100.0);

    // ---- landmark completeness
    Detection d;
    d.box = {100, 100, 100, 100};
    d.lm[0] = {125, 140}; // right eye
    d.lm[1] = {175, 140}; // left eye
    d.lm[2] = {150, 170}; // nose
    d.lm[3] = {120, 180}; // right mouth corner
    d.lm[4] = {180, 180}; // left mouth corner
    auto complete = FaceDetector::componentsOf(d, {400, 400});
    CHECK(complete.all());
    CHECK(FaceDetector::missingText(complete).empty());

    // eye beyond the image frame -> incomplete
    auto cutEye = FaceDetector::componentsOf(d, {120, 400});
    CHECK(!cutEye.all());
    CHECK(!cutEye.leftEye || !cutEye.rightEye);

    // collapsed eye geometry (strong profile) -> incomplete
    d.lm[0] = {149, 140};
    d.lm[1] = {151, 140};
    auto collapsed = FaceDetector::componentsOf(d, {400, 400});
    CHECK(!collapsed.all());

    FaceComponents onlyMouth{};
    onlyMouth.rightEye = true;
    onlyMouth.leftEye = false;
    onlyMouth.mouth = false;
    CHECK(std::string(FaceDetector::missingText(onlyMouth)) == "an eye + mouth");

    // ---- EXIF ground truth on a committed sample (orientation 1 = upright)
    const std::string sample = SAMPLES_DIR "/obama_portrait.jpg";
    std::ifstream in(sample, std::ios::binary);
    CHECK((bool)in);
    if (in) {
        std::vector<uchar> bytes((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
        CHECK(imageio::exifOrientation(bytes) == 1);
        std::string err;
        cv::Mat img = imageio::loadBgr(sample, err);
        CHECK(!img.empty());
        CHECK(err.empty());
    }

    // ---- configurable grade thresholds
    sharp::setGradeThresholds({90, 80, 70, 60});
    CHECK(std::string(sharp::ratingLabel(95.0)) == "A+");
    CHECK(std::string(sharp::ratingLabel(85.0)) == "A"); // was A+ with defaults
    CHECK(std::string(sharp::ratingLabel(55.0)) == "D"); // below C=60
    CHECK(sharp::isGradeD(55.0));
    CHECK(!sharp::isGradeD(65.0));
    // out-of-order input is sanitized (sorted ascending -> c,b,a,aPlus)
    sharp::setGradeThresholds({10, 90, 50, 30});
    const auto& g = sharp::grades();
    CHECK(g.c == 10 && g.b == 30 && g.a == 50 && g.aPlus == 90);
    // restore defaults
    sharp::setGradeThresholds(sharp::GradeThresholds{});
    CHECK(std::string(sharp::ratingLabel(85.0)) == "A+");
    CHECK(std::string(sharp::ratingLabel(41.9)) == "D");

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("all core tests passed\n");
    return 0;
}

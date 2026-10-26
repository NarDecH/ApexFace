// ApexFace icon generator: draws the logo with OpenCV and writes
//   assets/icon/apexface.ico      (16..256, PNG-compressed ICO entries)
//   assets/icon/apexface_256.png  (README / docs)
// usage: make_icon.exe [outDir]
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void put16(std::vector<uchar>& v, uint16_t x) {
    v.push_back((uchar)(x & 0xFF));
    v.push_back((uchar)(x >> 8));
}
static void put32(std::vector<uchar>& v, uint32_t x) {
    v.push_back((uchar)(x & 0xFF));
    v.push_back((uchar)((x >> 8) & 0xFF));
    v.push_back((uchar)((x >> 16) & 0xFF));
    v.push_back((uchar)((x >> 24) & 0xFF));
}

// Magnifier + face + sparkle on a blue->violet gradient, rounded-square canvas.
static cv::Mat drawLogo(int S) {
    cv::Mat img(S, S, CV_8UC4);
    const double R = S * 0.225; // corner radius

    // diagonal gradient #4F8CFF (top-left) -> #9333EA (bottom-right), BGRA
    cv::Vec4f c0(255, 140, 79, 255), c1(234, 51, 147, 255);
    auto inRounded = [&](double x, double y) {
        double rx = std::min(x, S - 1 - x), ry = std::min(y, S - 1 - y);
        if (rx >= R || ry >= R) return true; // central band
        double dx = R - rx, dy = R - ry;
        return dx * dx + dy * dy <= R * R;
    };

    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            double t = (x + y) / (2.0 * (S - 1));
            cv::Vec4b px;
            for (int c = 0; c < 3; ++c) px[c] = (uchar)std::lround(c0[c] + (c1[c] - c0[c]) * t);
            px[3] = inRounded(x, y) ? 255 : 0;
            img.at<cv::Vec4b>(y, x) = px;
        }
    }

    // lens interior: lighten the gradient inside the lens circle (manual blend)
    const cv::Point2f lensC(0.42f * S, 0.42f * S);
    const float lensR = 0.273f * S;
    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            double dx = x - lensC.x, dy = y - lensC.y;
            if (dx * dx + dy * dy <= lensR * lensR) {
                cv::Vec4b& px = img.at<cv::Vec4b>(y, x);
                for (int c = 0; c < 3; ++c) px[c] = (uchar)std::lround(px[c] * 0.72 + 255 * 0.28);
            }
        }
    }

    auto white = cv::Scalar(255, 255, 255, 255);
    auto navy = cv::Scalar(76, 38, 22, 255); // #16264C in BGR

    // face bust (dark navy): head + shoulders, inside the lens
    cv::circle(img, {cvRound(0.42 * S), cvRound(0.43 * S)}, cvRound(0.128 * S), navy, -1, cv::LINE_AA);
    cv::ellipse(img, {cvRound(0.42 * S), cvRound(0.625 * S)}, {cvRound(0.147 * S), cvRound(0.068 * S)},
                0, 0, 360, navy, -1, cv::LINE_AA);
    // eyes + smile (white)
    cv::circle(img, {cvRound(0.42 * S - 0.048 * S), cvRound(0.415 * S)}, cvRound(0.017 * S), white, -1, cv::LINE_AA);
    cv::circle(img, {cvRound(0.42 * S + 0.048 * S), cvRound(0.415 * S)}, cvRound(0.017 * S), white, -1, cv::LINE_AA);
    cv::ellipse(img, {cvRound(0.42 * S), cvRound(0.455 * S)}, {cvRound(0.058 * S), cvRound(0.040 * S)},
                0, 15, 165, white, cvRound(0.012 * S) + 1, cv::LINE_AA);

    // lens ring (white)
    cv::circle(img, lensC, cvRound(lensR), white, cvRound(0.052 * S), cv::LINE_AA);

    // handle: thick rounded line to the lower right
    cv::Point2f h0(0.42f * S + 0.20f * S, 0.42f * S + 0.20f * S);
    cv::Point2f h1(0.42f * S + 0.345f * S, 0.42f * S + 0.345f * S);
    cv::line(img, h0, h1, white, cvRound(0.055 * S), cv::LINE_AA);
    cv::circle(img, h1, cvRound(0.028 * S), white, -1, cv::LINE_AA);

    // sparkle (4-point star, top right) = the "sharp" cue
    {
        cv::Point2f sc(0.835f * S, 0.175f * S);
        float ro = 0.075f * S, ri = 0.020f * S;
        std::vector<cv::Point> pts;
        for (int k = 0; k < 8; ++k) {
            float ang = (float)(k * CV_PI / 4 - CV_PI / 2);
            float r = (k % 2 == 0) ? ro : ri;
            pts.push_back({cvRound(sc.x + r * cos(ang)), cvRound(sc.y + r * sin(ang))});
        }
        cv::fillPoly(img, pts, white, cv::LINE_AA);
    }
    return img;
}

static std::vector<uchar> encodePng(const cv::Mat& bgra) {
    std::vector<uchar> png;
    cv::imencode(".png", bgra, png);
    return png;
}

static void writeIco(const fs::path& out, const std::vector<int>& sizes, const cv::Mat& hi) {
    struct Entry {
        int size;
        std::vector<uchar> png;
    };
    std::vector<Entry> entries;
    for (int s : sizes) {
        cv::Mat small;
        cv::resize(hi, small, {s, s}, 0, 0, cv::INTER_AREA);
        entries.push_back({s, encodePng(small)});
    }
    std::vector<uchar> f;
    put16(f, 0);
    put16(f, 1); // type: icon
    put16(f, (uint16_t)entries.size());
    uint32_t offset = 6 + 16 * (uint32_t)entries.size();
    for (const auto& e : entries) {
        f.push_back(e.size == 256 ? 0 : (uchar)e.size); // width (0 = 256)
        f.push_back(e.size == 256 ? 0 : (uchar)e.size); // height
        f.push_back(0);                                 // palette
        f.push_back(0);                                 // reserved
        put16(f, 1);                                    // planes
        put16(f, 32);                                   // bpp
        put32(f, (uint32_t)e.png.size());
        put32(f, offset);
        offset += (uint32_t)e.png.size();
    }
    for (const auto& e : entries) f.insert(f.end(), e.png.begin(), e.png.end());

    std::error_code ec;
    fs::create_directories(out.parent_path(), ec);
    FILE* fp = _wfopen(out.c_str(), L"wb");
    if (!fp) {
        fprintf(stderr, "cannot write %s\n", out.string().c_str());
        exit(1);
    }
    fwrite(f.data(), 1, f.size(), fp);
    fclose(fp);
    printf("written %s (%zu bytes, %zu sizes)\n", out.string().c_str(), f.size(), entries.size());
}

int main(int argc, char** argv) {
    fs::path outDir = argc > 1 ? argv[1] : "assets/icon";
    cv::Mat hi = drawLogo(1024);
    writeIco(outDir / "apexface.ico", {16, 24, 32, 48, 64, 128, 256}, hi);
    cv::Mat p256;
    cv::resize(hi, p256, {256, 256}, 0, 0, cv::INTER_AREA);
    std::vector<uchar> png = encodePng(p256);
    std::error_code ec;
    fs::create_directories(outDir, ec);
    FILE* fp = _wfopen((outDir / "apexface_256.png").c_str(), L"wb");
    fwrite(png.data(), 1, png.size(), fp);
    fclose(fp);
    printf("written %s\n", (outDir / "apexface_256.png").string().c_str());
    return 0;
}

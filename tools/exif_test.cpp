// Dev tool: print EXIF orientation using ApexFace's parser, for ground-truth comparison.
// usage: exif_test.exe <folder> | exif_test.exe file.jpg [file2.jpg ...]
#include "ImageIO.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void report(const fs::path& p) {
    std::vector<uchar> bytes;
    FILE* f = _wfopen(p.c_str(), L"rb");
    if (!f) { printf("%s:OPEN_ERR\n", p.filename().string().c_str()); return; }
    char buf[65536];
    size_t n;
    size_t total = 0;
    // EXIF APP1 lives at the very start of the file; 1 MB is plenty
    while (total < (1u << 20) && (n = fread(buf, 1, sizeof(buf), f)) > 0) {
        size_t keep = ((1u << 20) - total < n) ? (1u << 20) - total : n;
        bytes.insert(bytes.end(), buf, buf + keep);
        total += keep;
    }
    fclose(f);
    int ori = imageio::exifOrientation(bytes);
    // also exercise the production path: loadBgr must return a rotated image
    std::string err;
    cv::Mat img = imageio::loadBgr(p, err);
    // direct synthetic test of applyOrientation
    cv::Mat probe = cv::Mat::zeros(100, 50, CV_8UC3);
    imageio::applyOrientation(probe, 8);
    cv::Mat direct;
    cv::rotate(cv::Mat::zeros(100, 50, CV_8UC3), direct, cv::ROTATE_90_COUNTERCLOCKWISE);
    // does imdecode honor IMREAD_IGNORE_ORIENTATION?
    FILE* f2 = _wfopen(p.c_str(), L"rb");
    std::vector<uchar> raw;
    char buf2[65536];
    size_t n2;
    while ((n2 = fread(buf2, 1, sizeof(buf2), f2)) > 0) raw.insert(raw.end(), buf2, buf2 + n2);
    fclose(f2);
    cv::Mat withFlag = cv::imdecode(raw, cv::IMREAD_COLOR | cv::IMREAD_IGNORE_ORIENTATION);
    cv::Mat noFlag = cv::imdecode(raw, cv::IMREAD_COLOR);
    printf("%s:exif=%d:loadBgr=%dx%d:synthAfter8=%dx%d:directRotate=%dx%d:imdecode_flag=%dx%d:imdecode_plain=%dx%d:%s\n",
           p.filename().string().c_str(), ori, img.cols, img.rows, probe.cols, probe.rows,
           direct.cols, direct.rows, withFlag.cols, withFlag.rows, noFlag.cols, noFlag.rows,
           err.c_str());
}

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: exif_test <folder|file...>\n"); return 2; }
    std::error_code ec;
    if (fs::is_directory(argv[1], ec)) {
        for (fs::directory_iterator it(argv[1], ec), end; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            std::error_code e2;
            if (it->is_regular_file(e2)) report(it->path());
        }
    } else {
        for (int i = 1; i < argc; ++i) report(argv[i]);
    }
    return 0;
}

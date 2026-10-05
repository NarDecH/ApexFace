#include "ImageIO.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <fstream>

namespace imageio {
namespace {

cv::Mat decodeFromMemory(const std::vector<uchar>& bytes, std::string& err) {
    if (bytes.size() < 16) {
        err = "file too small";
        return {};
    }
    cv::Mat img = cv::imdecode(bytes, cv::IMREAD_COLOR);
    if (img.empty()) err = "decode failed (unsupported or corrupt image)";
    return img;
}
} // namespace

cv::Mat loadBgr(const fs::path& file, std::string& errorOut) {
    errorOut.clear();
    try {
        std::ifstream in(file, std::ios::binary);
        if (!in) {
            errorOut = "cannot open file";
            return {};
        }
        std::vector<uchar> bytes((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
        cv::Mat img = decodeFromMemory(bytes, errorOut);
        if (img.empty()) return {};
        int ori = exifOrientation(bytes);
        applyOrientation(img, ori);
        return img;
    } catch (const std::exception& e) {
        errorOut = e.what();
        return {};
    }
}

bool saveJpeg(const fs::path& file, const cv::Mat& bgr, int quality, std::string& errorOut) {
    errorOut.clear();
    std::vector<uchar> out;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, quality};
    if (!cv::imencode(".jpg", bgr, out, params)) {
        errorOut = "jpeg encode failed";
        return false;
    }
    try {
        std::error_code ec;
        fs::create_directories(file.parent_path(), ec);
        std::ofstream out_f(file, std::ios::binary | std::ios::trunc);
        if (!out_f) {
            errorOut = "cannot create output file";
            return false;
        }
        out_f.write(reinterpret_cast<const char*>(out.data()), (std::streamsize)out.size());
        return (bool)out_f;
    } catch (const std::exception& e) {
        errorOut = e.what();
        return false;
    }
}

// Minimal EXIF reader: find APP1 "Exif\0\0" segment, read IFD0 orientation tag (0x0112).
int exifOrientation(const std::vector<uchar>& b) {
    if (b.size() < 4 || b[0] != 0xFF || b[1] != 0xD8) return 0; // not JPEG
    size_t pos = 2;
    while (pos + 4 <= b.size()) {
        if (b[pos] != 0xFF) return 0;
        uchar marker = b[pos + 1];
        if (marker == 0xD8 || (marker >= 0xD0 && marker <= 0xD7) || marker == 0x01) {
            pos += 2;
            continue;
        }
        if (pos + 4 > b.size()) return 0;
        size_t segLen = (b[pos + 2] << 8) | b[pos + 3];
        if (marker == 0xE1 && segLen >= 8 && pos + 2 + segLen <= b.size()) {
            // check "Exif\0\0"
            const uchar* seg = &b[pos + 4];
            size_t segBody = segLen - 2;
            if (segBody >= 6 && seg[0] == 'E' && seg[1] == 'x' && seg[2] == 'i' && seg[3] == 'f' &&
                seg[4] == 0 && seg[5] == 0) {
                const uchar* tiff = seg + 6;
                size_t tiffLen = segBody - 6;
                if (tiffLen < 8) return 0;
                bool le = (tiff[0] == 'I' && tiff[1] == 'I');
                bool be = (tiff[0] == 'M' && tiff[1] == 'M');
                if (!le && !be) return 0;
                auto u16 = [&](const uchar* p) { return le ? (p[0] | (p[1] << 8)) : ((p[0] << 8) | p[1]); };
                auto u32 = [&](const uchar* p) {
                    return le ? (p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24))
                              : (((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3]);
                };
                uint32_t ifdOff = u32(tiff + 4);
                if (ifdOff + 2 > tiffLen) return 0;
                uint16_t entries = u16(tiff + ifdOff);
                for (uint16_t i = 0; i < entries; ++i) {
                    size_t e = ifdOff + 2 + (size_t)i * 12;
                    if (e + 12 > tiffLen) return 0;
                    if (u16(tiff + e) == 0x0112) {
                        return (int)u16(tiff + e + 8);
                    }
                }
                return 0;
            }
        }
        pos += 2 + segLen;
    }
    return 0;
}

void applyOrientation(cv::Mat& img, int orientation) {
    switch (orientation) {
    case 2: cv::flip(img, img, 1); break;
    case 3: cv::rotate(img, img, cv::ROTATE_180); break;
    case 4: cv::flip(img, img, 0); break;
    case 5: cv::transpose(img, img); break;
    case 6: cv::rotate(img, img, cv::ROTATE_90_CLOCKWISE); break;
    case 7: cv::transpose(img, img), cv::flip(img, img, 1); break;
    case 8: cv::rotate(img, img, cv::ROTATE_90_COUNTERCLOCKWISE); break;
    default: break;
    }
}

} // namespace imageio

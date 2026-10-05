// ApexFace - image file IO with full Unicode path support and EXIF orientation
#pragma once
#include <opencv2/core.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace imageio {

// Decode a JPEG/PNG/... file from a possibly non-ASCII path and apply the
// EXIF orientation flag so the result matches what the user sees.
cv::Mat loadBgr(const fs::path& file, std::string& errorOut);

// Encode to JPEG in memory, then write via std::ofstream (Unicode-safe).
bool saveJpeg(const fs::path& file, const cv::Mat& bgr, int quality, std::string& errorOut);

// Raw EXIF orientation (1..8), 0 if absent / not a JPEG.
int exifOrientation(const std::vector<uchar>& bytes);

void applyOrientation(cv::Mat& img, int orientation);

} // namespace imageio

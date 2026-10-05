// probe: parse orientation from full bytes vs first-1MB bytes
#include "ImageIO.h"
#include <opencv2/core.hpp>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
namespace fs = std::filesystem;
int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::vector<uchar> full;
        FILE* f = fopen(argv[i], "rb");
        char buf[65536]; size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) full.insert(full.end(), buf, buf + n);
        fclose(f);
        std::vector<uchar> capped(full.begin(), full.begin() + std::min<size_t>(full.size(), 1u << 20));
        printf("%s:size=%zu:full=%d:capped1M=%d\n", fs::path(argv[i]).filename().string().c_str(),
               full.size(), imageio::exifOrientation(full), imageio::exifOrientation(capped));
    }
    return 0;
}

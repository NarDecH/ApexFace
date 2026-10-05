// One-off dev tool: create a synthetic blurred copy of an image for demo docs.
// usage: make_blur.exe <in.jpg> <out.jpg> <sigma>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 4) { std::cerr << "usage: make_blur <in> <out> <sigma>\n"; return 2; }
    cv::Mat img = cv::imread(argv[1], cv::IMREAD_COLOR);
    if (img.empty()) { std::cerr << "cannot read\n"; return 1; }
    cv::Mat out;
    cv::GaussianBlur(img, out, {0, 0}, atof(argv[3]));
    std::vector<int> p = {cv::IMWRITE_JPEG_QUALITY, 90};
    cv::imwrite(argv[2], out, p);
    std::cout << "written " << argv[2] << "\n";
    return 0;
}

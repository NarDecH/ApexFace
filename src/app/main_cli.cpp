// ApexFace - command-line interface (same engine as the GUI)
#include "../core/Analyzer.h"
#include "../core/ImageIO.h"
#include "../core/Logger.h"
#include "../core/Platform.h"
#include "../core/ReportGenerator.h"
#include "../core/Settings.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

void printUsage() {
    std::cout <<
        "ApexFace " APEXFACE_VERSION " - find the sharpest face in every photo of a folder\n"
        "\n"
        "Usage: apexface-cli <folder> [options]\n"
        "\n"
        "Options:\n"
        "  --out <dir>          output folder (default: <folder>/_apexface_report)\n"
        "  --no-recursive       do not scan subfolders\n"
        "  --min-face <px>      minimum face size in pixels (default 28)\n"
        "  --backend <name>     auto | cpu | cuda   (default auto)\n"
        "  --workers <n>        worker threads (0 = auto)\n"
        "  --no-annotated       do not save full-resolution annotated copies\n"
        "  --no-csv             skip data.csv\n"
        "  --no-json            skip data.json\n"
        "  --move-d             move grade-D / no-face originals to\n"
        "                       <folder>/_apexface_rejects after the run\n"
        "  --max-images <n>     analyze only the first n images (testing)\n"
        "  --dump-metrics <f>   write per-face raw metrics CSV (calibration)\n"
        "  --quiet              less console output\n"
        "  -h, --help           this help\n";
}

bool argValue(int argc, char** argv, int& i, const char* name, std::string& out) {
    if (i + 1 >= argc) {
        std::cerr << "missing value for " << name << "\n";
        return false;
    }
    out = argv[++i];
    return true;
}

LogLevel parseLevel(const std::string& s) {
    if (s == "debug") return LogLevel::Debug;
    if (s == "info") return LogLevel::Info;
    if (s == "warn") return LogLevel::Warn;
    if (s == "error") return LogLevel::Error;
    return LogLevel::Trace;
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    if (argc < 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        printUsage();
        return argc < 2 ? 2 : 0;
    }

    std::string folderArg, outArg, backend = "auto", dumpMetrics;
    bool recursive = true, exportAnnotated = true, exportCsv = true, exportJson = true, quiet = false;
    bool moveD = false;
    int minFace = 28, workers = 0, maxImages = 0;

    folderArg = argv[1];
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        bool ok = true;
        if (a == "--out") ok = argValue(argc, argv, i, "--out", outArg);
        else if (a == "--backend") ok = argValue(argc, argv, i, "--backend", backend);
        else if (a == "--dump-metrics") ok = argValue(argc, argv, i, "--dump-metrics", dumpMetrics);
        else if (a == "--min-face" && i + 1 < argc) minFace = atoi(argv[++i]);
        else if (a == "--workers" && i + 1 < argc) workers = atoi(argv[++i]);
        else if (a == "--max-images" && i + 1 < argc) maxImages = atoi(argv[++i]);
        else if (a == "--no-recursive") recursive = false;
        else if (a == "--no-annotated") exportAnnotated = false;
        else if (a == "--no-csv") exportCsv = false;
        else if (a == "--no-json") exportJson = false;
        else if (a == "--move-d") moveD = true;
        else if (a == "--quiet") quiet = true;
        else {
            std::cerr << "unknown option: " << a << "\n";
            return 2;
        }
        if (!ok) return 2;
    }

    fs::path folder = platform::utf8path(folderArg);
    std::error_code ec;
    if (!fs::exists(folder, ec) || !fs::is_directory(folder, ec)) {
        std::cerr << "folder not found: " << folderArg << "\n";
        return 2;
    }
    fs::path model = platform::findResource("models/face_detection_yunet_2023mar.onnx");
    if (model.empty()) {
        std::cerr << "model not found: models/face_detection_yunet_2023mar.onnx\n";
        return 2;
    }
    fs::path outDir = outArg.empty() ? folder / "_apexface_report" : platform::utf8path(outArg);
    fs::create_directories(outDir, ec);

    Logger::Config lc;
    lc.dir = outDir / "logs";
    lc.sessionTag = "cli";
    lc.consoleEcho = true;
    lc.consoleLevel = quiet ? LogLevel::Warn : LogLevel::Info;
    lc.fileLevel = LogLevel::Trace;
    Logger::instance().start(lc);
    AF_INFO("cli", "ApexFace " APEXFACE_VERSION " CLI started");

    AnalyzeOptions o;
    o.folder = folder;
    o.outDir = outDir;
    o.recursive = recursive;
    o.minFacePx = minFace;
    o.backend = backend == "cuda" ? Backend::CUDA : backend == "cpu" ? Backend::CPU : Backend::Auto;
    o.workers = workers;
    o.exportAnnotated = exportAnnotated;
    o.moveRejects = moveD;
    o.modelPathUtf8 = platform::utf8str(model);
    o.maxImages = maxImages;

    std::mutex dumpMtx;
    std::ofstream dump;
    if (!dumpMetrics.empty()) {
        dump.open(platform::utf8path(dumpMetrics), std::ios::binary | std::ios::trunc);
        dump << "file,face_idx,score,rating,box_x,box_y,box_w,box_h,img_w,img_h,"
                "lap_var,tenengrad,fft_hf,contrast_rms,res_factor,conf\n";
    }

    std::atomic<int> done{0}, total{0};
    std::atomic<bool> cancel{false};
    CurrentFileReporter current;

    Analyzer analyzer;
    RunSummary sum = analyzer.run(
        o,
        [&](ImageResult&& r) {
            if (!dumpMetrics.empty()) {
                std::lock_guard<std::mutex> lk(dumpMtx);
                for (size_t i = 0; i < r.faces.size(); ++i) {
                    const FaceResult& f = r.faces[i];
                    dump << '"' << r.relPath << "\"," << i << ',' << f.score << ','
                         << sharp::ratingLabel(f.score) << ',' << f.box.x << ',' << f.box.y << ','
                         << f.box.width << ',' << f.box.height << ',' << r.width << ',' << r.height
                         << ',' << f.metrics.lapVar << ',' << f.metrics.tenengrad << ','
                         << f.metrics.fftHF << ',' << f.metrics.contrastRMS << ',' << f.resFactor
                         << ',' << f.conf << "\n";
                }
            }
        },
        done, total, cancel, current);

    if (!dumpMetrics.empty()) {
        dump.flush();
        AF_INFO("cli", "metrics dumped to " << dumpMetrics);
    }

    if (sum.backend == "init_failed") {
        std::cerr << "detector init failed - see logs\n";
        return 3;
    }
    if (sum.total == 0) {
        std::cout << "no image files found in folder\n";
        return 4;
    }

    ReportOptions ro;
    ro.outDir = outDir;
    ro.sourceFolderUtf8 = folderArg;
    ro.exportCsv = exportCsv;
    ro.exportJson = exportJson;
    ro.cardsPerPage = 200;
    ro.backend = sum.backend;
    ro.workers = sum.workers;
    ro.elapsedSec = sum.elapsedSec;
    ro.logFileToCopy = Logger::instance().filePath();
    ro.canceled = sum.canceled;

    std::string idxRel, err;
    bool ok = ReportGenerator::generate(sum.results, sum, ro, idxRel, err);
    if (!ok) {
        std::cerr << "report generation failed: " << err << "\n";
        return 5;
    }

    std::cout << "\n=== ApexFace summary ===\n"
              << "images      : " << sum.total << "\n"
              << "with faces  : " << sum.ok << "\n"
              << "no face     : " << sum.noFace << "\n"
              << "errors      : " << sum.errors << "\n"
              << "faces found : " << sum.totalFaces << "\n"
              << "best score  : " << sum.maxBest << " (" << sum.maxFile << ")\n"
              << "backend     : " << sum.backend << "\n"
              << "workers     : " << sum.workers << "\n";
    if (sum.movedCount > 0)
        std::cout << "moved D     : " << sum.movedCount << " -> "
                  << platform::utf8str(folder / "_apexface_rejects") << "\n";
    std::cout << "elapsed     : " << sum.elapsedSec << " s\n"
              << "report      : " << platform::utf8str(outDir / "index.html") << "\n";
    return sum.canceled ? 6 : 0;
}

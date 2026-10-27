// ApexFace - folder scanner + multithreaded analyze pipeline
#pragma once
#include "FaceDetector.h"
#include "Sharpness.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

// Thread-safe "currently processing X" reporter (std::atomic<std::string> is
// not supported by MSVC).
struct CurrentFileReporter {
    std::mutex mtx;
    std::string name;
    void set(const std::string& s) {
        std::lock_guard<std::mutex> lk(mtx);
        name = s;
    }
    std::string get() {
        std::lock_guard<std::mutex> lk(mtx);
        return name;
    }
};

struct AnalyzeOptions {
    fs::path folder;       // source folder
    fs::path outDir;       // report output folder
    bool recursive = true;
    int minFacePx = 28;
    Backend backend = Backend::Auto;
    int workers = 0;       // 0 = auto
    bool exportAnnotated = true;
    std::string modelPathUtf8; // resolved by caller
    int maxImages = 0;         // 0 = no limit (useful for tests/demos)
    bool moveRejects = false;  // move grade-D / no-face originals to <folder>/_apexface_rejects
    bool generateReport = true; // false = scoring pass only: no thumbs/annotated files
    bool useCache = true;      // reuse results of unchanged files (cached in <outDir>)
    int scoreMinPx = 0;        // score only faces at least this wide (0 = all detected faces)
};

struct FaceResult {
    cv::Rect box;            // original-image coordinates
    double score = 0;
    float conf = 0;
    sharp::Metrics metrics;
    double resFactor = 1;
    bool incomplete = false; // missing key components (occluded eye / mouth / profile / cropped)
    std::string missing;     // which parts, e.g. "an eye + mouth" (empty if complete)
    bool tooSmall = false;   // below the user's scoring size threshold: detected, not scored
    FaceComponents components;
};

struct ImageResult {
    int idx = 0;
    std::string relPath;     // UTF-8, generic (forward) slashes
    std::string name;        // filename only
    std::string srcPath;     // absolute UTF-8 path of the source file
    int width = 0, height = 0;
    std::vector<FaceResult> faces;
    int bestIdx = -1;
    double bestScore = 0;
    std::string status;      // "ok" | "no_face" | "incomplete" | "too_small" | "error"
    std::string error;
    double loadMs = 0, detectMs = 0, analyzeMs = 0, totalMs = 0;
    std::string thumbRel;      // relative to outDir
    std::string annotatedRel;  // empty if not exported
    std::string movedTo;       // relative path inside the source folder after a reject move
    std::string backend;
    std::uint64_t dHash = 0;   // 64-bit difference hash (0 = unavailable)
    int group = -1;            // burst/near-duplicate group id (1..N, -1 = unique)
    bool groupBest = false;    // this image is the best of its group
    int groupSize = 0;         // number of images in the group (0 = unique)
    double percentile = -1;    // bestScore percentile within the run (ok status only, -1 = n/a)
};

struct RunSummary {
    int total = 0, ok = 0, noFace = 0, incomplete = 0, smallOnly = 0, errors = 0;
    int totalFaces = 0;
    double maxBest = 0, avgBest = 0;
    std::string maxFile;
    double elapsedSec = 0;
    std::string backend;
    int workers = 0;
    bool canceled = false;
    int movedCount = 0;                 // files moved to _apexface_rejects (if enabled)
    int burstGroups = 0;                // near-duplicate groups with 2+ images
    std::vector<ImageResult> results;   // final state of every result (post move)
};

class Analyzer {
public:
    using ResultFn = std::function<void(ImageResult&&)>;

    // Blocks until the whole folder is processed (call from a worker thread).
    RunSummary run(const AnalyzeOptions& opt, const ResultFn& onResult,
                   std::atomic<int>& done, std::atomic<int>& total,
                   std::atomic<bool>& cancel, CurrentFileReporter& currentFile);

    static std::vector<fs::path> scanFolder(const AnalyzeOptions& opt);

    // Move files listed in <folder>/_apexface_rejects/_apexface_moved.tsv back to
    // their original locations. Returns the number of restored files.
    static int restoreMoved(const fs::path& folder);

private:
    ImageResult processOne(const fs::path& file, int idx, const AnalyzeOptions& opt,
                           FaceDetector& det, const std::string& backendName);
};

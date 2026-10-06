#include "Analyzer.h"
#include "ImageIO.h"
#include "Logger.h"
#include "Platform.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <memory>
#include <sstream>
#include <thread>

namespace {

using clock_st = std::chrono::steady_clock;
double msSince(clock_st::time_point t0) {
    return std::chrono::duration<double, std::milli>(clock_st::now() - t0).count();
}

bool isImageExt(const fs::path& p) {
    static const char* exts[] = {".jpg", ".jpeg", ".png", ".bmp", ".webp", ".tif", ".tiff"};
    std::string e = p.extension().string();
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return (char)tolower(c); });
    for (const char* k : exts)
        if (e == k) return true;
    return false;
}

std::string jsonStr(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        default: out += (char)c;
        }
    }
    out += '"';
    return out;
}

void drawLabel(cv::Mat& img, const cv::Point& org, const std::string& text, double fontScale,
               int thickness, const cv::Scalar& bg) {
    int base = 0;
    cv::Size ts = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, fontScale, thickness, &base);
    int pad = std::max(2, (int)(4 * fontScale));
    cv::Rect bgRect(org.x - pad, org.y - ts.height - 2 * pad, ts.width + 2 * pad, ts.height + 2 * pad);
    bgRect &= cv::Rect(0, 0, img.cols, img.rows);
    cv::rectangle(img, bgRect, bg, cv::FILLED);
    cv::Point tp(org.x, org.y - pad);
    cv::putText(img, text, tp, cv::FONT_HERSHEY_SIMPLEX, fontScale, {0, 0, 0}, thickness + 1,
                cv::LINE_AA);
    cv::putText(img, text, tp, cv::FONT_HERSHEY_SIMPLEX, fontScale, {255, 255, 255}, thickness,
                cv::LINE_AA);
}

// Draw all boxes; the best face gets a fat colored box + star, others thin gray.
void drawResults(cv::Mat& img, const ImageResult& r, bool watermark) {
    if (r.faces.empty()) {
        if (watermark) {
            cv::putText(img, "no face detected", {14, img.rows - 16}, cv::FONT_HERSHEY_SIMPLEX,
                        std::max(0.5, img.cols / 1400.0), {200, 200, 200}, 1, cv::LINE_AA);
        }
        return;
    }
    double fs = std::clamp(img.cols / 1600.0, 0.4, 1.1);
    int th = std::max(1, (int)std::lround(img.cols / 900.0));
    for (size_t i = 0; i < r.faces.size(); ++i) {
        const FaceResult& f = r.faces[i];
        bool best = (int)i == r.bestIdx;
        cv::Scalar color = best ? sharp::ratingBgr(f.score) : cv::Scalar(160, 160, 160);
        cv::rectangle(img, f.box, color, best ? th + 2 : th, cv::LINE_AA);
        std::string label = (std::ostringstream() << std::fixed << std::setprecision(1) << f.score
                             << (best ? " #1" : ""))
                                .str();
        int base = 0;
        cv::Size ts = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, fs, th + 1, &base);
        cv::Point org(f.box.x, std::max(ts.height + 10, f.box.y - 6));
        drawLabel(img, org, label, fs, th + 1, color);
    }
    if (watermark) {
        cv::putText(img, "ApexFace", {img.cols - 150, img.rows - 16}, cv::FONT_HERSHEY_SIMPLEX,
                    0.55, {180, 180, 180}, 1, cv::LINE_AA);
    }
}

std::string flatName(int idx, const std::string& stem) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%06d", idx);
    std::string safe;
    for (char c : stem) safe += (isalnum((unsigned char)c) || c == '-' || c == '_') ? c : '_';
    return std::string(buf) + "_" + safe + ".jpg";
}

} // namespace

std::vector<fs::path> Analyzer::scanFolder(const AnalyzeOptions& opt) {
    std::vector<fs::path> files;
    std::error_code ec;
    auto push = [&](const fs::path& p) {
        std::error_code e2;
        if (!p.has_filename() || p.filename().string().rfind(".", 0) == 0) return;
        if (fs::is_regular_file(p, e2) && isImageExt(p)) {
            if (!opt.outDir.empty() && platform::isDirInside(opt.outDir, p)) return; // skip own output
            // skip any (re)generated report folders nested in the source tree
            for (const auto& part : p)
                if (part.string().rfind("_apexface", 0) == 0) return;
            files.push_back(p);
        }
    };
    if (opt.recursive) {
        for (fs::recursive_directory_iterator it(opt.folder, fs::directory_options::skip_permission_denied, ec), end;
             it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            push(it->path());
        }
    } else {
        for (fs::directory_iterator it(opt.folder, fs::directory_options::skip_permission_denied, ec), end;
             it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            push(it->path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

ImageResult Analyzer::processOne(const fs::path& file, int idx, const AnalyzeOptions& opt,
                                 FaceDetector& det, const std::string& backendName) {
    ImageResult r;
    r.idx = idx;
    r.srcPath = platform::utf8str(file);
    fs::path rel = fs::relative(file, opt.folder);
    r.relPath = platform::utf8str(rel);
    r.name = platform::utf8str(file.filename());

    auto t0 = clock_st::now();
    std::string err;
    cv::Mat img = imageio::loadBgr(file, err);
    r.loadMs = msSince(t0);
    r.backend = backendName;
    if (img.empty()) {
        r.status = "error";
        r.error = err.empty() ? "load failed" : err;
        r.totalMs = msSince(t0);
        AF_ERROR("scan", "load failed: " << r.relPath << " (" << r.error << ")");
        Logger::instance().event("image", "error",
                                 "\"file\":" + jsonStr(r.relPath) + ",\"error\":" + jsonStr(r.error));
        return r;
    }
    r.width = img.cols;
    r.height = img.rows;
    AF_TRACE("scan", "loaded " << r.relPath << " " << img.cols << "x" << img.rows << " in "
                               << (int)r.loadMs << " ms");

    auto t1 = clock_st::now();
    auto dets = det.detect(img, r.detectMs);
    auto t2 = clock_st::now();

    for (const auto& d : dets) {
        sharp::Score s = sharp::scoreFace(img, d.box);
        FaceResult fr;
        fr.box = d.box;
        fr.conf = d.conf;
        fr.score = s.final0to100;
        fr.resFactor = s.resFactor;
        fr.metrics = s.raw;
        r.faces.push_back(std::move(fr));
    }
    r.analyzeMs = msSince(t2);

    if (!r.faces.empty()) {
        r.bestIdx = 0;
        r.bestScore = r.faces[0].score;
        for (size_t i = 1; i < r.faces.size(); ++i)
            if (r.faces[i].score > r.faces[r.bestIdx].score) r.bestIdx = (int)i;
        r.bestScore = r.faces[r.bestIdx].score;
    }
    r.status = r.faces.empty() ? "no_face" : "ok";

    // Outputs: thumbnail always (unless the whole report is disabled);
    // annotated full-res copy optional.
    std::string fname = flatName(idx, platform::utf8str(file.stem()));
    if (opt.generateReport) {
        fs::path thumbPath = opt.outDir / "thumbs" / fname;
        double tw = std::min<double>(480, img.cols);
        double sc = tw / img.cols;
        cv::Mat thumb;
        cv::resize(img, thumb, {(int)std::lround(img.cols * sc), (int)std::lround(img.rows * sc)},
                   0, 0, cv::INTER_AREA);
        drawResults(thumb, r, true);
        imageio::saveJpeg(thumbPath, thumb, 88, err);
        r.thumbRel = "thumbs/" + fname;

        if (opt.exportAnnotated && !r.faces.empty()) {
            cv::Mat full = img.clone();
            drawResults(full, r, true);
            fs::path annPath = opt.outDir / "annotated" / fname;
            if (imageio::saveJpeg(annPath, full, 90, err)) {
                r.annotatedRel = "annotated/" + fname;
            } else {
                AF_WARN("report", "annotated save failed for " << r.relPath << ": " << err);
            }
        }
    }
    r.totalMs = msSince(t0);

    double best = r.bestScore;
    AF_DEBUG("scan", r.relPath << " -> faces=" << r.faces.size() << " best=" << (int)best
                               << " load=" << (int)r.loadMs << "ms det=" << (int)r.detectMs
                               << "ms total=" << (int)r.totalMs << "ms");
    Logger::instance().event(
        "image", "processed",
        "\"file\":" + jsonStr(r.relPath) + ",\"w\":" + std::to_string(r.width) +
            ",\"h\":" + std::to_string(r.height) + ",\"faces\":" + std::to_string(r.faces.size()) +
            ",\"best\":" + (r.faces.empty() ? "null" : std::to_string(best)) +
            ",\"status\":" + jsonStr(r.status) + ",\"backend\":" + jsonStr(backendName) +
            ",\"ms\":" + std::to_string(r.totalMs));
    return r;
}

RunSummary Analyzer::run(const AnalyzeOptions& opt, const ResultFn& onResult,
                         std::atomic<int>& done, std::atomic<int>& total,
                         std::atomic<bool>& cancel, CurrentFileReporter& currentFile) {
    RunSummary sum;
    auto runStart = clock_st::now();
    AF_INFO("scan", "analysis started: folder=" << platform::utf8str(opt.folder)
                                                << (opt.recursive ? " (recursive)" : "")
                                                << " minFace=" << opt.minFacePx << "px workers="
                                                << (opt.workers ? std::to_string(opt.workers) : "auto"));
    Logger::instance().event("run", "start", "\"folder\":" + jsonStr(platform::utf8str(opt.folder)));

    auto files = scanFolder(opt);
    if (opt.maxImages > 0 && (int)files.size() > opt.maxImages) files.resize(opt.maxImages);
    total.store((int)files.size(), std::memory_order_relaxed);
    sum.total = (int)files.size();
    AF_INFO("scan", "found " << files.size() << " image files");

    int hw = (int)std::thread::hardware_concurrency();
    int workers = opt.workers > 0 ? std::clamp(opt.workers, 1, 16)
                                  : std::clamp(hw - 1, 1, 8);
    if ((int)files.size() < workers) workers = std::max(1, (int)files.size());
    sum.workers = workers;

    FaceDetector shared;
    std::string backendName;
    {
        std::string err;
        if (!shared.init(platform::utf8path(opt.modelPathUtf8), opt.backend, backendName, err)) {
            AF_ERROR("detect", "detector init failed: " << err);
            sum.backend = "init_failed";
            sum.elapsedSec = msSince(runStart) / 1000.0;
            return sum;
        }
    }
    shared.setMinFacePx(opt.minFacePx);
    sum.backend = backendName;
    bool cudaMode = backendName == "cuda";

    std::atomic<int> nextIdx{0};
    std::mutex onResultMtx;
    std::vector<ImageResult> collected;

    // Wrap the caller's callback to aggregate summary stats (guarded by onResultMtx).
    ResultFn cb = [&](ImageResult&& r) {
        if (r.status == "ok") ++sum.ok;
        else if (r.status == "no_face") ++sum.noFace;
        else if (r.status == "error") ++sum.errors;
        sum.totalFaces += (int)r.faces.size();
        sum.avgBest += r.bestScore;
        if (r.bestScore > sum.maxBest) {
            sum.maxBest = r.bestScore;
            sum.maxFile = r.relPath;
        }
        collected.push_back(r);
        onResult(std::move(r));
    };

    auto worker = [&](int) {
        // CPU mode: one detector instance per thread. CUDA mode: share one instance.
        FaceDetector* det = &shared;
        std::unique_ptr<FaceDetector> own;
        if (!cudaMode && workers > 1) {
            own = std::make_unique<FaceDetector>();
            std::string be, err;
            if (own->init(platform::utf8path(opt.modelPathUtf8), Backend::CPU, be, err)) {
                own->setMinFacePx(opt.minFacePx);
                det = own.get();
            } else {
                AF_WARN("detect", "thread detector init failed, using shared: " << err);
            }
        }
        while (!cancel.load(std::memory_order_relaxed)) {
            int i = nextIdx.fetch_add(1, std::memory_order_relaxed);
            if (i >= (int)files.size()) break;
            const fs::path& f = files[i];
            currentFile.set(platform::utf8str(f.filename()));
            ImageResult r;
            try {
                r = processOne(f, i, opt, *det, cudaMode ? shared.backendName() : det->backendName());
            } catch (const std::exception& e) {
                r.idx = i;
                r.status = "error";
                r.error = std::string("exception: ") + e.what();
                AF_ERROR("scan", "exception on " << platform::utf8str(f) << ": " << e.what());
            }
            {
                std::lock_guard<std::mutex> lk(onResultMtx);
                cb(std::move(r));
            }
            done.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> pool;
    for (int i = 0; i < workers; ++i) pool.emplace_back(worker, i);
    for (auto& t : pool) t.join();

    sum.canceled = cancel.load();
    if (sum.canceled) AF_WARN("scan", "analysis canceled by user");

    // Move grade-D / no-face originals into <folder>/_apexface_rejects
    if (opt.moveRejects && !sum.canceled) {
        fs::path rejRoot = opt.folder / "_apexface_rejects";
        for (auto& r : collected) {
            const bool reject = (r.status == "no_face") ||
                                (r.status == "ok" && sharp::isGradeD(r.bestScore));
            if (!reject || r.srcPath.empty()) continue;
            std::error_code ec;
            fs::path src = platform::utf8path(r.srcPath);
            if (!fs::exists(src, ec)) continue;
            fs::path rel = fs::relative(src, opt.folder, ec);
            if (ec || rel.empty()) continue;
            fs::path dst = rejRoot / rel;
            fs::create_directories(dst.parent_path(), ec);
            int alt = 1;
            while (fs::exists(dst, ec)) {
                dst = rejRoot / rel.parent_path() /
                      (rel.stem().string() + "_" + std::to_string(alt++) + rel.extension().string());
            }
            fs::rename(src, dst, ec);
            if (ec) { // cross-device fallback: copy + remove
                std::error_code ec2;
                fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec2);
                if (!ec2) {
                    fs::remove(src, ec2);
                    if (ec2) { AF_WARN("scan", "moved but could not remove source: " << r.relPath); ec2.clear(); }
                } else {
                    ec = ec2;
                }
            }
            if (ec) {
                AF_ERROR("scan", "reject move failed for " << r.relPath << ": " << ec.message());
                Logger::instance().event("move", "failed",
                                         "\"file\":" + jsonStr(r.relPath) + ",\"error\":" + jsonStr(ec.message()));
                continue;
            }
            r.movedTo = platform::utf8str(dst.generic_wstring());
            ++sum.movedCount;
            AF_INFO("scan", "moved grade-D reject: " << r.relPath << " -> " << r.movedTo);
            Logger::instance().event("move", "done",
                                     "\"file\":" + jsonStr(r.relPath) + ",\"to\":" + jsonStr(r.movedTo) +
                                         ",\"best\":" + std::to_string(r.bestScore) +
                                         ",\"status\":" + jsonStr(r.status));
        }
        AF_INFO("scan", "moved " << sum.movedCount << " reject file(s) to "
                                 << platform::utf8str(rejRoot));
    }

    if (sum.ok > 0) sum.avgBest /= sum.ok;
    sum.elapsedSec = msSince(runStart) / 1000.0;
    sum.results = std::move(collected);
    AF_INFO("scan", "analysis finished in " << sum.elapsedSec << "s (ok=" << sum.ok
                                            << " noface=" << sum.noFace << " err=" << sum.errors << ")");
    Logger::instance().event("run", "end",
                             "\"elapsed\":" + std::to_string(sum.elapsedSec) +
                                 ",\"canceled\":" + (sum.canceled ? "true" : "false") +
                                 ",\"files\":" + std::to_string(files.size()));
    return sum;
}

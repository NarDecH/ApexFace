#include "Analyzer.h"
#include "ImageIO.h"
#include "Logger.h"
#include "Platform.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <bit>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <memory>
#include <sstream>
#include <thread>
#include <unordered_map>

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
        cv::Scalar color;
        std::string label;
        if (f.tooSmall) {
            color = cv::Scalar(160, 160, 160); // informational: below scoring threshold
        } else if (f.incomplete) {
            color = {11, 158, 245}; // amber: missing components -> grade D
            label = "D";
        } else {
            color = best ? sharp::ratingBgr(f.score) : cv::Scalar(160, 160, 160);
            label = (std::ostringstream() << std::fixed << std::setprecision(1) << f.score
                     << (best ? " #1" : ""))
                        .str();
        }
        cv::rectangle(img, f.box, color, best ? th + 2 : (f.tooSmall ? std::max(1, th - 1) : th),
                      cv::LINE_AA);
        if (label.empty()) continue;
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

// 64-bit difference hash: downscaled 9x8 gray, bit k = px[c] > px[c+1].
// Near-duplicate frames (bursts) differ by only a few bits.
std::uint64_t computeDHash(const cv::Mat& bgr) {
    if (bgr.empty()) return 0;
    cv::Mat gray, small;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    cv::resize(gray, small, {9, 8}, 0, 0, cv::INTER_AREA);
    std::uint64_t h = 0;
    int bit = 0;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            if (small.at<uchar>(y, x) > small.at<uchar>(y, x + 1)) h |= (1ULL << bit);
            ++bit;
        }
    return h;
}

// ---------------------------------------------------------------- result cache
struct CacheRec {
    std::uintmax_t size = 0;
    std::int64_t mtime = 0;
    ImageResult r;
};

static std::string cacheSettingsKey(const AnalyzeOptions& opt) {
    std::ostringstream os;
    os << "s2|minFace=" << opt.minFacePx << "|backend=" << (int)opt.backend << "|score=2"
       << "|scoreFrom=" << opt.scoreMinPx;
    return os.str();
}

static std::int64_t mtimeOf(const fs::path& p) {
    std::error_code ec;
    auto ft = fs::last_write_time(p, ec);
    if (ec) return 0;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(ft.time_since_epoch()).count();
}

// Split a helper: fields separated by sep.
template <typename Fn>
static void splitFields(const std::string& s, char sep, Fn&& fn) {
    size_t start = 0;
    while (true) {
        size_t pos = s.find(sep, start);
        if (pos == std::string::npos) {
            fn(s.substr(start));
            break;
        }
        fn(s.substr(start, pos - start));
        start = pos + 1;
    }
}

static void loadResultCache(const AnalyzeOptions& opt, const std::string& key,
                            std::unordered_map<std::string, CacheRec>& out) {
    std::ifstream in(opt.outDir / "analysis_cache.tsv");
    if (!in) return;
    std::string line;
    if (!std::getline(in, line) || line != key) return; // settings changed -> stale cache
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> f;
        splitFields(line, '\t', [&](std::string tok) { f.push_back(std::move(tok)); });
        if (f.size() < 14) continue;
        CacheRec rec;
        const std::string& rel = f[0];
        rec.size = std::strtoull(f[1].c_str(), nullptr, 10);
        rec.mtime = std::strtoll(f[2].c_str(), nullptr, 10);
        ImageResult& r = rec.r;
        r.relPath = rel;
        r.width = atoi(f[3].c_str());
        r.height = atoi(f[4].c_str());
        r.status = f[5];
        r.bestIdx = atoi(f[6].c_str());
        r.bestScore = atof(f[7].c_str());
        r.backend = f[8];
        r.dHash = std::strtoull(f[9].c_str(), nullptr, 10);
        r.thumbRel = f[10];
        r.annotatedRel = f[11];
        r.movedTo = f[12];
        if (!f[13].empty()) {
            splitFields(f[13], ';', [&](const std::string& fs_) {
                std::vector<std::string> g;
                splitFields(fs_, ',', [&](std::string t) { g.push_back(std::move(t)); });
                if (g.size() < 13) return;
                FaceResult fr;
                fr.box = {atoi(g[0].c_str()), atoi(g[1].c_str()), atoi(g[2].c_str()),
                          atoi(g[3].c_str())};
                fr.conf = (float)atof(g[4].c_str());
                fr.score = atof(g[5].c_str());
                fr.resFactor = atof(g[6].c_str());
                fr.incomplete = g[7] == "1";
                fr.missing = g[8];
                fr.metrics.lapVar = atof(g[9].c_str());
                fr.metrics.tenengrad = atof(g[10].c_str());
                fr.metrics.fftHF = atof(g[11].c_str());
                fr.metrics.contrastRMS = atof(g[12].c_str());
                r.faces.push_back(std::move(fr));
            });
        }
        out[rel] = std::move(rec);
    }
    AF_INFO("scan", "loaded " << out.size() << " cached result(s)");
}

static void saveResultCache(const AnalyzeOptions& opt, const std::string& key,
                            const std::vector<ImageResult>& results) {
    std::error_code ec;
    fs::create_directories(opt.outDir, ec);
    std::ofstream o(opt.outDir / "analysis_cache.tsv", std::ios::binary | std::ios::trunc);
    if (!o) return;
    o << key << "\n";
    for (const auto& r : results) {
        if (r.status == "error" || r.srcPath.empty()) continue;
        fs::path src = platform::utf8path(r.srcPath);
        std::error_code e2;
        if (!fs::exists(src, e2)) continue; // moved away (reject) -> out of scan scope anyway
        std::uintmax_t size = fs::file_size(src, e2);
        if (e2) continue;
        std::ostringstream faces;
        for (size_t i = 0; i < r.faces.size(); ++i) {
            const FaceResult& fr = r.faces[i];
            if (i) faces << ';';
            faces << fr.box.x << ',' << fr.box.y << ',' << fr.box.width << ',' << fr.box.height
                  << ',' << fr.conf << ',' << fr.score << ',' << fr.resFactor << ','
                  << (fr.incomplete ? 1 : 0) << ',' << fr.missing << ',' << fr.metrics.lapVar
                  << ',' << fr.metrics.tenengrad << ',' << fr.metrics.fftHF << ','
                  << fr.metrics.contrastRMS;
        }
        o << r.relPath << '\t' << size << '\t' << mtimeOf(src) << '\t' << r.width << '\t'
          << r.height << '\t' << r.status << '\t' << r.bestIdx << '\t' << r.bestScore << '\t'
          << r.backend << '\t' << r.dHash << '\t' << r.thumbRel << '\t' << r.annotatedRel << '\t'
          << r.movedTo << '\t' << faces.str() << '\n';
    }
    AF_INFO("scan", "result cache saved (" << results.size() << " entries)");
}

} // namespace

int Analyzer::restoreMoved(const fs::path& folder) {
    const fs::path manifest = folder / "_apexface_rejects" / "_apexface_moved.tsv";
    std::error_code ec;
    if (!fs::exists(manifest, ec)) return 0;
    std::ifstream in(manifest);
    if (!in) return 0;
    std::vector<std::pair<fs::path, fs::path>> moves; // [current dst, original src]
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back(); // tolerate CRLF manifests
        if (line.empty()) continue;
        const auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        moves.emplace_back(folder / platform::utf8path(line.substr(0, tab)),
                           folder / platform::utf8path(line.substr(tab + 1)));
    }
    in.close();

    int restored = 0;
    for (const auto& [dst, src] : moves) {
        if (!fs::exists(dst, ec)) continue; // already restored or removed
        fs::create_directories(src.parent_path(), ec);
        fs::path finalSrc = src;
        int alt = 1;
        while (fs::exists(finalSrc, ec)) {
            finalSrc = src.parent_path() / (src.stem().string() + "_r" + std::to_string(alt++) +
                                            src.extension().string());
        }
        fs::rename(dst, finalSrc, ec);
        if (ec) {
            AF_WARN("scan", "restore failed for " << platform::utf8str(dst) << ": " << ec.message());
            continue;
        }
        ++restored;
        AF_INFO("scan", "restored: " << platform::utf8str(finalSrc));
        Logger::instance().event("move", "restored",
                                 "\"to\":" + jsonStr(platform::utf8str(finalSrc)));
    }
    if (restored > 0) {
        fs::remove(manifest, ec);
        Logger::instance().event("move", "restored_done",
                                 "{\"count\":" + std::to_string(restored) + "}");
    }
    return restored;
}

std::vector<fs::path> Analyzer::scanFolder(const AnalyzeOptions& opt) {
    std::vector<fs::path> files;
    std::error_code ec;
    auto push = [&](const fs::path& p) {
        std::error_code e2;
        if (!p.has_filename() || p.filename().string().rfind(".", 0) == 0) return;
        if (fs::is_regular_file(p, e2) && isImageExt(p)) {
            if (!opt.outDir.empty() && platform::isDirInside(opt.outDir, p)) return; // skip own output
            // Skip our own generated folders (report thumbs/annotated would pollute
            // re-runs; moved rejects would be re-processed) - but only when they are
            // BELOW the scanned folder, so pointing the scanner directly at
            // _apexface_rejects still works.
            std::error_code e3;
            fs::path rel = fs::relative(p, opt.folder, e3);
            if (!e3) {
                for (const auto& part : rel) {
                    std::string name = part.string();
                    for (auto& ch : name) ch = (char)tolower((unsigned char)ch);
                    if (name == "_apexface_report" || name == "_apexface_rejects") return;
                }
            }
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
    r.dHash = computeDHash(img);
    AF_TRACE("scan", "loaded " << r.relPath << " " << img.cols << "x" << img.rows << " in "
                               << (int)r.loadMs << " ms");

    auto t1 = clock_st::now();
    auto dets = det.detect(img, r.detectMs);
    auto t2 = clock_st::now();

    for (const auto& d : dets) {
        FaceResult fr;
        fr.box = d.box;
        fr.conf = d.conf;
        if (opt.scoreMinPx > 0 && d.box.width < opt.scoreMinPx) {
            // below the user's scoring size threshold: keep the detection but
            // skip the sharpness computation entirely
            fr.tooSmall = true;
            r.faces.push_back(std::move(fr));
            continue;
        }
        sharp::Score s = sharp::scoreFace(img, d.box, d.lm);
        fr.score = s.final0to100;
        fr.resFactor = s.resFactor;
        fr.metrics = s.raw;
        fr.components = FaceDetector::componentsOf(d, {img.cols, img.rows});
        fr.incomplete = !fr.components.all();
        fr.missing = FaceDetector::missingText(fr.components);
        r.faces.push_back(std::move(fr));
    }
    r.analyzeMs = msSince(t2);

    // Best face = sharpest among faces that are scoreable (big enough) and
    // complete; images whose faces are all incomplete or all too small to
    // score are graded D.
    if (!r.faces.empty()) {
        bool anyScoreable = false;
        for (size_t i = 0; i < r.faces.size(); ++i) {
            const FaceResult& f = r.faces[i];
            if (f.tooSmall || f.incomplete) continue;
            anyScoreable = true;
            if (r.bestIdx < 0 || f.score > r.faces[r.bestIdx].score) r.bestIdx = (int)i;
        }
        if (r.bestIdx >= 0) {
            r.bestScore = r.faces[r.bestIdx].score;
            r.status = "ok";
        } else if (anyScoreable) {
            r.bestScore = 0;
            r.status = "incomplete";
        } else {
            r.bestScore = 0;
            r.status = "too_small";
        }
    } else {
        r.status = "no_face";
    }

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

    std::unordered_map<std::string, CacheRec> cacheMap;
    const std::string cacheKey = cacheSettingsKey(opt);
    if (opt.useCache) loadResultCache(opt, cacheKey, cacheMap);

    std::atomic<int> nextIdx{0};
    std::mutex onResultMtx;
    std::vector<ImageResult> collected;

    // Wrap the caller's callback to aggregate summary stats (guarded by onResultMtx).
    ResultFn cb = [&](ImageResult&& r) {
        if (r.status == "ok") ++sum.ok;
        else if (r.status == "no_face") ++sum.noFace;
        else if (r.status == "incomplete") ++sum.incomplete;
        else if (r.status == "too_small") ++sum.smallOnly;
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

            // cache hit: same size + mtime + settings -> reuse the previous result
            if (!cacheMap.empty()) {
                std::error_code e3;
                fs::path relp = fs::relative(f, opt.folder, e3);
                const std::string relKey = e3 ? std::string() : platform::utf8str(relp);
                auto it = cacheMap.find(relKey);
                if (it != cacheMap.end()) {
                    const CacheRec& cr = it->second;
                    std::error_code e4;
                    const bool same =
                        !e4 && fs::file_size(f, e4) == cr.size && mtimeOf(f) == cr.mtime;
                    const bool usable =
                        !opt.generateReport || (!cr.r.thumbRel.empty() &&
                                                (cr.r.annotatedRel.empty() == !opt.exportAnnotated));
                    if (same && usable) {
                        ImageResult r = cr.r;
                        r.idx = i;
                        r.name = platform::utf8str(f.filename());
                        r.srcPath = platform::utf8str(f);
                        r.loadMs = r.detectMs = r.analyzeMs = r.totalMs = 0;
                        {
                            std::lock_guard<std::mutex> lk(onResultMtx);
                            cb(std::move(r));
                        }
                        done.fetch_add(1, std::memory_order_relaxed);
                        AF_TRACE("scan", "cache hit: " << r.relPath);
                        continue;
                    }
                }
            }

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
            const bool reject = (r.status == "no_face") || (r.status == "incomplete") ||
                                (r.status == "too_small") ||
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
            r.movedTo = platform::utf8str(fs::relative(dst, opt.folder, ec).generic_wstring());
            if (ec) { // fall back to the absolute path if relative() fails
                r.movedTo = platform::utf8str(dst.generic_wstring());
                ec.clear();
            }
            ++sum.movedCount;
            AF_INFO("scan", "moved grade-D reject: " << r.relPath << " -> " << r.movedTo);
            Logger::instance().event("move", "done",
                                     "\"file\":" + jsonStr(r.relPath) + ",\"to\":" + jsonStr(r.movedTo) +
                                         ",\"best\":" + std::to_string(r.bestScore) +
                                         ",\"status\":" + jsonStr(r.status));
            // manifest for "restore moved files" (binary mode: no \r translation)
            {
                std::ofstream mf(opt.folder / "_apexface_rejects" / "_apexface_moved.tsv",
                                 std::ios::app | std::ios::binary);
                if (mf) mf << r.movedTo << '\t' << r.relPath << '\n';
            }
        }
        AF_INFO("scan", "moved " << sum.movedCount << " reject file(s) to "
                                 << platform::utf8str(rejRoot));
    }

    // Near-duplicate (burst) grouping via 64-bit dHash. Greedy anchoring: each
    // image joins the first group whose ANCHOR hash is within 10 bits, so
    // similar-looking scenes cannot chain one long group together.
    {
        struct BurstGroup {
            std::uint64_t anchor = 0;
            std::vector<int> members;
        };
        std::vector<BurstGroup> groups;
        for (int i = 0; i < (int)collected.size(); ++i) {
            const std::uint64_t h = collected[i].dHash;
            if (!h) continue;
            int hit = -1;
            for (auto& g : groups) {
                if (std::popcount(h ^ g.anchor) <= 10) {
                    hit = (int)(&g - groups.data());
                    break;
                }
            }
            if (hit < 0) {
                groups.push_back({h, {}});
                hit = (int)groups.size() - 1;
            }
            groups[hit].members.push_back(i);
        }
        int gid = 0;
        for (const auto& g : groups) {
            if (g.members.size() < 2) continue;
            ++gid;
            int best = -1;
            for (int m : g.members) {
                collected[m].group = gid;
                collected[m].groupSize = (int)g.members.size();
                if (collected[m].status == "ok" &&
                    (best < 0 || collected[m].bestScore > collected[best].bestScore))
                    best = m;
            }
            if (best >= 0) collected[best].groupBest = true;
        }
        sum.burstGroups = gid;
        if (gid) AF_INFO("scan", "burst/near-duplicate groups: " << gid);
    }

    // Percentile of each ok image's best score within this run.
    {
        std::vector<int> okIdx;
        for (int i = 0; i < (int)collected.size(); ++i)
            if (collected[i].status == "ok") okIdx.push_back(i);
        std::sort(okIdx.begin(), okIdx.end(),
                  [&](int a, int b) { return collected[a].bestScore > collected[b].bestScore; });
        const double n = (double)okIdx.size();
        for (size_t rank = 0; rank < okIdx.size(); ++rank)
            collected[okIdx[rank]].percentile =
                n > 0 ? 100.0 * ((double)n - (double)rank) / n : -1.0;
    }

    if (sum.ok > 0) sum.avgBest /= sum.ok;
    sum.elapsedSec = msSince(runStart) / 1000.0;
    sum.results = std::move(collected);
    if (opt.useCache) saveResultCache(opt, cacheKey, sum.results);
    AF_INFO("scan", "analysis finished in " << sum.elapsedSec << "s (ok=" << sum.ok
                                            << " noface=" << sum.noFace << " err=" << sum.errors << ")");
    Logger::instance().event("run", "end",
                             "\"elapsed\":" + std::to_string(sum.elapsedSec) +
                                 ",\"canceled\":" + (sum.canceled ? "true" : "false") +
                                 ",\"files\":" + std::to_string(files.size()));
    return sum;
}

#include "ReportGenerator.h"
#include "Logger.h"
#include "Platform.h"
#include "Sharpness.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {

std::string hesc(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 16);
    for (unsigned char c : s) {
        switch (c) {
        case '&': o += "&amp;"; break;
        case '<': o += "&lt;"; break;
        case '>': o += "&gt;"; break;
        case '"': o += "&quot;"; break;
        default: o += (char)c;
        }
    }
    return o;
}

std::string f1(double v) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(1) << v;
    return os.str();
}
std::string f2(double v) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(2) << v;
    return os.str();
}
std::string f4(double v) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(4) << v;
    return os.str();
}

std::string jnum(double v) {
    if (std::isfinite(v)) {
        std::ostringstream os;
        os << std::fixed << std::setprecision(2) << v;
        return os.str();
    }
    return "null";
}

std::string jstr(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        switch (c) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        default:
            if (c < 0x20) {
                char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); o += b;
            } else o += (char)c;
        }
    }
    o += '"';
    return o;
}

const char* kCss = R"CSS(
:root{--bg:#0b0f17;--panel:#121826;--panel2:#0e1420;--line:#1f2937;--text:#e6eaf2;--muted:#8b95a7;
--acc:#4f8cff;--acc2:#9333ea;--emerald:#10b981;--green:#4ade80;--sky:#38bdf8;--amber:#f59e0b;--red:#ef4444}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--text);font-family:"Segoe UI",system-ui,-apple-system,Roboto,sans-serif;line-height:1.55}
.wrap{max-width:1280px;margin:0 auto;padding:0 20px 60px}
header.hero{background:linear-gradient(135deg,#111a2e 0%,#1b1233 60%,#0b0f17 100%);
border-bottom:1px solid var(--line);padding:42px 0 34px;margin-bottom:28px}
header.hero .wrap{padding-bottom:0}
h1{font-size:30px;font-weight:700;letter-spacing:.3px;display:flex;align-items:center;gap:12px}
h1 .logo{width:38px;height:38px;border-radius:11px;background:linear-gradient(135deg,var(--acc),var(--acc2));
display:inline-flex;align-items:center;justify-content:center;font-size:20px}
.sub{color:var(--muted);margin-top:6px;font-size:14px}
.chips{margin-top:14px;display:flex;flex-wrap:wrap;gap:8px}
.chip{background:var(--panel);border:1px solid var(--line);border-radius:999px;padding:4px 12px;font-size:12.5px;color:var(--muted)}
.chip b{color:var(--text);font-weight:600}
.stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:14px;margin:22px 0 30px}
.stat{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:16px 18px}
.stat .v{font-size:26px;font-weight:700}
.stat .k{color:var(--muted);font-size:12.5px;margin-top:2px;text-transform:uppercase;letter-spacing:.8px}
h2{font-size:20px;margin:34px 0 14px;display:flex;align-items:center;gap:10px}
h2::after{content:"";flex:1;height:1px;background:var(--line)}
.heroimg{display:grid;grid-template-columns:minmax(280px,440px) 1fr;gap:24px;background:var(--panel);
border:1px solid var(--line);border-radius:18px;overflow:hidden}
.heroimg img{width:100%;display:block}
.heroimg .info{padding:22px}
.scorebig{font-size:46px;font-weight:800;line-height:1}
.rating{display:inline-block;border-radius:8px;padding:2px 10px;font-weight:700;font-size:14px;color:#0b0f17}
table{border-collapse:collapse;width:100%;font-size:13.5px}
th{color:var(--muted);text-transform:uppercase;font-size:11.5px;letter-spacing:.6px;text-align:left;padding:8px 10px;border-bottom:1px solid var(--line)}
td{padding:8px 10px;border-bottom:1px solid var(--panel2)}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(240px,1fr));gap:16px;margin-top:14px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:14px;overflow:hidden;transition:transform .12s ease,border-color .12s ease}
.card:hover{transform:translateY(-3px);border-color:#334155}
.card .imgwrap{position:relative;display:block}
.card img{width:100%;aspect-ratio:3/2;object-fit:cover;display:block;background:#0a0e15}
.badge{position:absolute;top:10px;right:10px;border-radius:8px;padding:3px 9px;font-size:13px;font-weight:800;color:#0b0f17;
box-shadow:0 2px 8px rgba(0,0,0,.45)}
.meta{padding:10px 12px 12px}
.name{font-size:13.5px;font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.sub{color:var(--muted);font-size:12px;margin-top:3px}
.bA{background:var(--emerald)}.bA2{background:var(--green)}.bB{background:var(--sky)}.bC{background:var(--amber)}.bD{background:var(--red)}.bN{background:#64748b;color:#fff}
.err{border-color:#7f1d1d}
.err .sub{color:#fca5a5}
.bars{display:flex;flex-direction:column;gap:10px;margin-top:8px}
.bar{display:grid;grid-template-columns:110px 1fr 60px;align-items:center;gap:12px;font-size:13.5px}
.bar .track{background:var(--panel2);border-radius:999px;height:14px;overflow:hidden}
.bar .fill{height:100%;border-radius:999px}
.pager{display:flex;gap:8px;flex-wrap:wrap;margin:22px 0}
.pager a{background:var(--panel);border:1px solid var(--line);color:var(--text);border-radius:10px;padding:6px 14px;text-decoration:none;font-size:14px}
.pager a.cur{background:var(--acc);border-color:var(--acc);color:#fff;font-weight:700}
.pager a:hover{border-color:var(--acc)}
.filterbar{display:flex;gap:8px;flex-wrap:wrap;margin:16px 0 4px}
.filterbar button{background:var(--panel);border:1px solid var(--line);color:var(--text);border-radius:999px;
padding:5px 14px;font-size:13px;cursor:pointer}
.filterbar button.on{background:var(--acc);border-color:var(--acc);color:#fff;font-weight:700}
details{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:12px 16px;margin-top:10px}
details summary{cursor:pointer;font-weight:600}
footer{color:var(--muted);font-size:12.5px;margin-top:46px;text-align:center}
a{color:var(--sky)}
.top10{display:grid;grid-template-columns:repeat(auto-fill,minmax(180px,1fr));gap:12px}
.top10 .card img{aspect-ratio:1/1}
)CSS";

std::string badgeClass(double score) {
    if (score >= 85) return "bA";
    if (score >= 72) return "bA2";
    if (score >= 58) return "bB";
    if (score >= 42) return "bC";
    return "bD";
}

std::string cardHtml(const ImageResult& r) {
    std::ostringstream o;
    o << "<div class=\"card" << (r.status == "error" ? " err" : "") << "\"";
    if (!r.faces.empty()) o << " data-r=\"" << sharp::ratingLabel(r.bestScore) << "\"";
    else o << " data-r=\"N\"";
    o << ">";
    if (!r.thumbRel.empty() && r.status != "error") {
        if (!r.annotatedRel.empty()) {
            o << "<a class=\"imgwrap\" href=\"" << hesc(r.annotatedRel) << "\" target=\"_blank\" rel=\"noopener\">";
        } else {
            o << "<span class=\"imgwrap\">";
        }
        o << "<img loading=\"lazy\" src=\"" << hesc(r.thumbRel) << "\" alt=\"" << hesc(r.name) << "\">";
        if (!r.faces.empty()) {
            o << "<span class=\"badge " << badgeClass(r.bestScore) << "\">" << f1(r.bestScore) << "</span>";
        } else {
            o << "<span class=\"badge bN\">no face</span>";
        }
        o << (r.annotatedRel.empty() ? "</span>" : "</a>");
    }
    o << "<div class=\"meta\">";
    o << "<div class=\"name\" title=\"" << hesc(r.relPath) << "\">" << hesc(r.name) << "</div>";
    if (r.status == "ok") {
        o << "<div class=\"sub\">" << r.faces.size() << (r.faces.size() == 1 ? " face" : " faces")
          << " &middot; " << r.width << "&times;" << r.height << " &middot; " << (int)r.totalMs << " ms</div>";
    } else if (r.status == "no_face") {
        o << "<div class=\"sub\">no face &middot; " << r.width << "&times;" << r.height << "</div>";
    } else {
        o << "<div class=\"sub\">error: " << hesc(r.error) << "</div>";
    }
    o << "</div></div>";
    return o.str();
}

std::string pageHead(const std::string& title) {
    std::ostringstream o;
    o << "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">"
      << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      << "<title>" << hesc(title) << "</title><style>" << kCss << "</style></head><body>";
    return o.str();
}

bool writeFile(const fs::path& p, const std::string& content, std::string& err) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) {
        err = "cannot write " + platform::utf8str(p);
        return false;
    }
    f.write(content.data(), (std::streamsize)content.size());
    return (bool)f;
}

} // namespace

bool ReportGenerator::generate(const std::vector<ImageResult>& results, const RunSummary& sum,
                               const ReportOptions& opt, std::string& indexRelOut, std::string& errorOut) {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &tt);
    std::ostringstream dateOs;
    dateOs << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    std::string dateStr = dateOs.str();

    indexRelOut = "index.html";

    // best overall & top list
    std::vector<const ImageResult*> ranked;
    for (const auto& r : results)
        if (r.status == "ok") ranked.push_back(&r);
    std::sort(ranked.begin(), ranked.end(),
              [](const ImageResult* a, const ImageResult* b) { return a->bestScore > b->bestScore; });

    // rating distribution
    int dist[5] = {0, 0, 0, 0, 0}; // A+, A, B, C, D
    for (const auto* r : ranked) {
        double s = r->bestScore;
        dist[s >= 85 ? 0 : s >= 72 ? 1 : s >= 58 ? 2 : s >= 42 ? 3 : 4]++;
    }
    const char* distNames[5] = {"A+ (85-100)", "A (72-84)", "B (58-71)", "C (42-57)", "D (0-41)"};
    const char* distColors[5] = {"var(--emerald)", "var(--green)", "var(--sky)", "var(--amber)", "var(--red)"};

    // ------------------------------------------------------------------ index
    {
        std::ostringstream o;
        o << pageHead("ApexFace Report");
        o << "<header class=\"hero\"><div class=\"wrap\"><h1><span class=\"logo\">\xF0\x9F\x94\x8D</span>"
          << "ApexFace Report</h1>"
          << "<div class=\"sub\">Generated " << dateStr << (opt.canceled ? " &middot; <b>run canceled</b>" : "") << "</div>"
          << "<div class=\"chips\">"
          << "<span class=\"chip\">Folder <b>" << hesc(opt.sourceFolderUtf8) << "</b></span>"
          << "<span class=\"chip\">Backend <b>" << hesc(sum.backend) << "</b></span>"
          << "<span class=\"chip\">Workers <b>" << sum.workers << "</b></span>"
          << "<span class=\"chip\">Elapsed <b>" << f1(opt.elapsedSec) << "s</b></span>"
          << "<span class=\"chip\">ApexFace <b>v" << APEXFACE_VERSION << "</b></span>"
          << "</div></div></header><div class=\"wrap\">";

        o << "<div class=\"stats\">"
          << "<div class=\"stat\"><div class=\"v\">" << sum.total << "</div><div class=\"k\">Images</div></div>"
          << "<div class=\"stat\"><div class=\"v\" style=\"color:var(--emerald)\">" << sum.ok << "</div><div class=\"k\">With faces</div></div>"
          << "<div class=\"stat\"><div class=\"v\" style=\"color:var(--muted)\">" << sum.noFace << "</div><div class=\"k\">No face</div></div>"
          << "<div class=\"stat\"><div class=\"v\" style=\"color:" << (sum.errors ? "var(--red)" : "var(--text)") << "\">" << sum.errors << "</div><div class=\"k\">Errors</div></div>"
          << "<div class=\"stat\"><div class=\"v\">" << sum.totalFaces << "</div><div class=\"k\">Faces found</div></div>"
          << "<div class=\"stat\"><div class=\"v\" style=\"color:var(--acc)\">" << (ranked.empty() ? "-" : f1(ranked.front()->bestScore)) << "</div><div class=\"k\">Best score</div></div>"
          << "</div>";

        if (!ranked.empty()) {
            const ImageResult* best = ranked.front();
            o << "<h2>Sharpest face in the folder</h2><div class=\"heroimg\">";
            o << "<a href=\"" << hesc(best->annotatedRel.empty() ? best->thumbRel : best->annotatedRel)
              << "\" target=\"_blank\" rel=\"noopener\"><img src=\"" << hesc(best->thumbRel) << "\" alt=\"best\"></a>";
            o << "<div class=\"info\"><div class=\"scorebig\" style=\"color:" << sharp::ratingHex(best->bestScore)
              << "\">" << f1(best->bestScore) << "</div>"
              << "<span class=\"rating " << badgeClass(best->bestScore) << "\" style=\"background:"
              << sharp::ratingHex(best->bestScore) << "\">" << sharp::ratingLabel(best->bestScore) << "</span>"
              << "<p style=\"margin-top:14px;font-size:15px\"><b>" << hesc(best->relPath) << "</b></p>"
              << "<p class=\"sub\">" << best->faces.size() << " face(s) detected &middot; " << best->width
              << "&times;" << best->height << " px</p>"
              << "<table style=\"margin-top:14px\"><tr><th>Metric</th><th>Value</th></tr>"
              << "<tr><td>Laplacian variance</td><td>" << f2(best->faces[best->bestIdx].metrics.lapVar) << "</td></tr>"
              << "<tr><td>Tenengrad</td><td>" << f2(best->faces[best->bestIdx].metrics.tenengrad) << "</td></tr>"
              << "<tr><td>FFT high-freq ratio</td><td>" << f4(best->faces[best->bestIdx].metrics.fftHF) << "</td></tr>"
              << "<tr><td>RMS contrast</td><td>" << f2(best->faces[best->bestIdx].metrics.contrastRMS) << "</td></tr>"
              << "</table></div></div>";

            // Top 12 gallery
            o << "<h2>Top faces</h2><div class=\"top10\">";
            int n = 0;
            for (const auto* r : ranked) {
                if (n++ >= 12) break;
                o << cardHtml(*r);
            }
            o << "</div>";
        }

        // distribution
        o << "<h2>Score distribution (best face per image)</h2><div class=\"bars\">";
        int maxDist = 1;
        for (int d : dist) maxDist = std::max(maxDist, d);
        for (int i = 0; i < 5; ++i) {
            o << "<div class=\"bar\"><span>" << distNames[i] << "</span>"
              << "<div class=\"track\"><div class=\"fill\" style=\"width:" << (100.0 * dist[i] / maxDist)
              << "%;background:" << distColors[i] << "\"></div></div><span>" << dist[i] << "</span></div>";
        }
        o << "</div>";

        if (sum.errors > 0) {
            o << "<details><summary>Errors (" << sum.errors << ")</summary><table>";
            for (const auto& r : results) {
                if (r.status != "error") continue;
                o << "<tr><td>" << hesc(r.relPath) << "</td><td>" << hesc(r.error) << "</td></tr>";
            }
            o << "</table></details>";
        }

        // pagination links
        int pages = (int)std::ceil((double)results.size() / std::max(1, opt.cardsPerPage));
        if (pages > 0) {
            o << "<h2>All images (" << results.size() << ")</h2><div class=\"pager\">";
            for (int p = 0; p < pages; ++p) {
                char nm[32];
                snprintf(nm, sizeof(nm), "cards_%03d.html", p + 1);
                o << "<a href=\"" << nm << "\">Page " << (p + 1) << "</a>";
            }
            o << "</div>";
        }

        if (opt.exportCsv || opt.exportJson) {
            o << "<h2>Machine-readable data</h2><p class=\"sub\">";
            if (opt.exportCsv) o << "<a href=\"data.csv\">data.csv</a> &nbsp; ";
            if (opt.exportJson) o << "<a href=\"data.json\">data.json</a>";
            o << "</p>";
        }

        o << "<footer>Generated by ApexFace v" << APEXFACE_VERSION
          << " &middot; YuNet face detection &middot; multi-metric sharpness scoring</footer>"
          << "</div></body></html>";
        if (!writeFile(opt.outDir / "index.html", o.str(), errorOut)) return false;
    }

    // ------------------------------------------------------------ cards pages
    int pages = (int)std::ceil((double)results.size() / std::max(1, opt.cardsPerPage));
    for (int p = 0; p < pages; ++p) {
        std::ostringstream o;
        o << pageHead("ApexFace Report — Cards " + std::to_string(p + 1));
        o << "<div class=\"wrap\">";
        o << "<h1 style=\"font-size:22px;margin:26px 0 0\">All images — page " << (p + 1) << " / " << pages
          << " <a style=\"font-size:14px\" href=\"index.html\">&larr; summary</a></h1>";

        // filter bar + cards
        o << "<div class=\"filterbar\">"
          << "<button class=\"on\" data-f=\"all\">All</button>"
          << "<button data-f=\"A\">A+/A</button><button data-f=\"B\">B</button>"
          << "<button data-f=\"C\">C</button><button data-f=\"D\">D</button>"
          << "<button data-f=\"N\">No face</button></div>";
        o << "<div class=\"grid\" id=\"grid\">";
        size_t s = (size_t)p * opt.cardsPerPage;
        size_t e = std::min(results.size(), s + opt.cardsPerPage);
        for (size_t i = s; i < e; ++i) o << cardHtml(results[i]);
        o << "</div>";

        o << "<div class=\"pager\">";
        if (p > 0) o << "<a href=\"cards_" << std::setw(3) << std::setfill('0') << p << ".html\">&larr; Prev</a>";
        o << "<a href=\"index.html\">Summary</a>";
        if (p + 1 < pages)
            o << "<a href=\"cards_" << std::setw(3) << std::setfill('0') << (p + 2) << ".html\">Next &rarr;</a>";
        o << "</div>";

        o << "<script>document.querySelectorAll('.filterbar button').forEach(b=>{b.onclick=()=>{"
             "document.querySelectorAll('.filterbar button').forEach(x=>x.classList.remove('on'));"
             "b.classList.add('on');var f=b.dataset.f;"
             "document.querySelectorAll('#grid .card').forEach(c=>{"
             "var r=c.dataset.r;if(r&&r.startsWith('A'))r='A';"
             "c.style.display=(f==='all'||f===r)?'':'none';});};};</script>";
        o << "</div></body></html>";
        char nm[32];
        snprintf(nm, sizeof(nm), "cards_%03d.html", p + 1);
        if (!writeFile(opt.outDir / nm, o.str(), errorOut)) return false;
    }

    // ------------------------------------------------------------------- CSV
    if (opt.exportCsv) {
        std::ostringstream o;
        o << "idx,rel_path,status,width,height,faces,best_face_idx,best_score,rating,"
             "box_x,box_y,box_w,box_h,lap_var,tenengrad,fft_hf,contrast_rms,res_factor,"
             "conf,load_ms,detect_ms,analyze_ms,total_ms,backend,thumb,annotated\n";
        for (const auto& r : results) {
            const FaceResult* bf = r.bestIdx >= 0 ? &r.faces[r.bestIdx] : nullptr;
            o << r.idx << ",\"" << r.relPath << "\"," << r.status << "," << r.width << "," << r.height
              << "," << r.faces.size() << ",";
            if (bf) {
                o << r.bestIdx << "," << f2(bf->score) << "," << sharp::ratingLabel(bf->score) << ","
                  << bf->box.x << "," << bf->box.y << "," << bf->box.width << "," << bf->box.height << ","
                  << f2(bf->metrics.lapVar) << "," << f2(bf->metrics.tenengrad) << ","
                  << f4(bf->metrics.fftHF) << "," << f2(bf->metrics.contrastRMS) << ","
                  << f2(bf->resFactor) << "," << f2(bf->conf);
            } else {
                o << ",,,,,,,,";
            }
            o << "," << f1(r.loadMs) << "," << f1(r.detectMs) << "," << f1(r.analyzeMs) << ","
              << f1(r.totalMs) << "," << r.backend << ",\"" << r.thumbRel << "\",\"" << r.annotatedRel
              << "\"\n";
        }
        if (!writeFile(opt.outDir / "data.csv", o.str(), errorOut)) return false;
    }

    // ------------------------------------------------------------------ JSON
    if (opt.exportJson) {
        std::ostringstream o;
        o << std::fixed << std::setprecision(2);
        o << "{\"meta\":{\"app\":\"ApexFace\",\"version\":\"" << APEXFACE_VERSION
          << "\",\"generated\":" << jstr(dateStr) << ",\"source\":" << jstr(opt.sourceFolderUtf8)
          << ",\"backend\":" << jstr(sum.backend) << ",\"workers\":" << sum.workers
          << ",\"elapsed_sec\":" << jnum(opt.elapsedSec) << ",\"canceled\":" << (sum.canceled ? "true" : "false")
          << ",\"counts\":{\"total\":" << sum.total << ",\"ok\":" << sum.ok << ",\"no_face\":" << sum.noFace
          << ",\"errors\":" << sum.errors << ",\"faces\":" << sum.totalFaces << "}},\"images\":[";
        bool first = true;
        for (const auto& r : results) {
            if (!first) o << ',';
            first = false;
            o << "{\"idx\":" << r.idx << ",\"path\":" << jstr(r.relPath) << ",\"status\":" << jstr(r.status);
            if (!r.error.empty()) o << ",\"error\":" << jstr(r.error);
            o << ",\"width\":" << r.width << ",\"height\":" << r.height;
            o << ",\"faces\":[";
            for (size_t i = 0; i < r.faces.size(); ++i) {
                const FaceResult& fr = r.faces[i];
                if (i) o << ',';
                o << "{\"box\":[" << fr.box.x << "," << fr.box.y << "," << fr.box.width << ","
                  << fr.box.height << "],\"score\":" << jnum(fr.score) << ",\"rating\":"
                  << jstr(sharp::ratingLabel(fr.score)) << ",\"conf\":" << jnum(fr.conf)
                  << ",\"lap_var\":" << jnum(fr.metrics.lapVar) << ",\"tenengrad\":" << jnum(fr.metrics.tenengrad)
                  << ",\"fft_hf\":" << std::fixed << std::setprecision(4) << fr.metrics.fftHF
                  << std::fixed << std::setprecision(2) << ",\"contrast_rms\":" << jnum(fr.metrics.contrastRMS)
                  << ",\"res_factor\":" << jnum(fr.resFactor) << "}";
            }
            o << "]";
            if (r.bestIdx >= 0) o << ",\"best_index\":" << r.bestIdx << ",\"best_score\":" << jnum(r.bestScore);
            o << ",\"load_ms\":" << jnum(r.loadMs) << ",\"detect_ms\":" << jnum(r.detectMs)
              << ",\"total_ms\":" << jnum(r.totalMs) << ",\"thumb\":" << jstr(r.thumbRel)
              << ",\"annotated\":" << jstr(r.annotatedRel) << "}";
        }
        o << "]}";
        if (!writeFile(opt.outDir / "data.json", o.str(), errorOut)) return false;
    }

    // ------------------------------------------------------------------- logs
    if (!opt.logFileToCopy.empty() && fs::exists(opt.logFileToCopy)) {
        std::error_code ec;
        fs::create_directories(opt.outDir / "logs", ec);
        fs::copy_file(opt.logFileToCopy, opt.outDir / "logs" / opt.logFileToCopy.filename(),
                      fs::copy_options::overwrite_existing, ec);
    }
    AF_INFO("report", "report generated at " << platform::utf8str(opt.outDir / "index.html"));
    Logger::instance().event("report", "generated", "\"dir\":" + jstr(platform::utf8str(opt.outDir)));
    return true;
}

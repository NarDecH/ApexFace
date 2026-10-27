#include "Settings.h"
#include "Platform.h"

#include <opencv2/core.hpp>

#include <algorithm>

bool Settings::load(const fs::path& file) {
    try {
        cv::FileStorage f(file.string(), cv::FileStorage::READ);
        if (!f.isOpened()) return false;
        auto rs = [&](const char* k, std::string& dst, const char* def) {
            dst = f[k].empty() ? def : (std::string)f[k];
        };
        auto ri = [&](const char* k, int& dst, int def) { dst = f[k].empty() ? def : (int)f[k]; };
        auto rb = [&](const char* k, bool& dst, bool def) {
            dst = f[k].empty() ? def : ((int)f[k] != 0);
        };
        rs("lastFolder", lastFolder, "");
        if (!f["recentFolders"].empty()) {
            recentFolders.clear();
            for (auto n : f["recentFolders"]) recentFolders.emplace_back((std::string)n);
        }
        rs("outDirOverride", outDirOverride, "");
        rb("recursive", recursive, true);
        ri("minFacePx", minFacePx, 28);
        rs("backend", backend, "auto");
        ri("workers", workers, 0);
        rb("exportAnnotated", exportAnnotated, true);
        rb("exportCsv", exportCsv, true);
        rb("exportJson", exportJson, true);
        rb("generateReport", generateReport, true);
        rb("moveRejects", moveRejects, false);
        rb("useCache", useCache, true);
        ri("scoreMinPx", scoreMinPx, 0);
        {
            // grade boundaries; sanitize by sorting ascending -> c,b,a,aPlus
            double g[4] = {85, 72, 58, 42};
            if (!f["gradeAPlus"].empty()) g[0] = (double)f["gradeAPlus"];
            if (!f["gradeA"].empty()) g[1] = (double)f["gradeA"];
            if (!f["gradeB"].empty()) g[2] = (double)f["gradeB"];
            if (!f["gradeC"].empty()) g[3] = (double)f["gradeC"];
            std::sort(g, g + 4);
            gradeC = g[0];
            gradeB = g[1];
            gradeA = g[2];
            gradeAPlus = g[3];
        }
        ri("reportCardsPerPage", reportCardsPerPage, 200);
        rs("logLevel", logLevel, "trace");
        rs("lang", lang, "en");
        return true;
    } catch (...) {
        return false;
    }
}

bool Settings::save(const fs::path& file) const {
    try {
        std::error_code ec;
        fs::create_directories(file.parent_path(), ec);
        cv::FileStorage f(file.string(), cv::FileStorage::WRITE);
        if (!f.isOpened()) return false;
        f << "lastFolder" << lastFolder;
        f << "recentFolders" << "[";
        for (const auto& s : recentFolders) f << s;
        f << "]";
        f << "outDirOverride" << outDirOverride;
        f << "recursive" << recursive;
        f << "minFacePx" << minFacePx;
        f << "backend" << backend;
        f << "workers" << workers;
        f << "exportAnnotated" << exportAnnotated;
        f << "exportCsv" << exportCsv;
        f << "exportJson" << exportJson;
        f << "generateReport" << generateReport;
        f << "moveRejects" << moveRejects;
        f << "useCache" << useCache;
        f << "scoreMinPx" << scoreMinPx;
        f << "gradeAPlus" << gradeAPlus;
        f << "gradeA" << gradeA;
        f << "gradeB" << gradeB;
        f << "gradeC" << gradeC;
        f << "reportCardsPerPage" << reportCardsPerPage;
        f << "logLevel" << logLevel;
        f << "lang" << lang;
        return true;
    } catch (...) {
        return false;
    }
}

fs::path Settings::defaultPath() {
    return platform::exeDir() / "config" / "settings.yml";
}

void Settings::addRecentFolder(const std::string& utf8Folder) {
    if (utf8Folder.empty()) return;
    for (size_t i = 0; i < recentFolders.size(); ++i) {
        if (recentFolders[i] == utf8Folder) {
            recentFolders.erase(recentFolders.begin() + i);
            break;
        }
    }
    recentFolders.insert(recentFolders.begin(), utf8Folder);
    if (recentFolders.size() > 5) recentFolders.resize(5);
}

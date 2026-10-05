#include "Settings.h"
#include "Platform.h"

#include <opencv2/core.hpp>

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
        rs("outDirOverride", outDirOverride, "");
        rb("recursive", recursive, true);
        ri("minFacePx", minFacePx, 28);
        rs("backend", backend, "auto");
        ri("workers", workers, 0);
        rb("exportAnnotated", exportAnnotated, true);
        rb("exportCsv", exportCsv, true);
        rb("exportJson", exportJson, true);
        rb("moveRejects", moveRejects, false);
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
        f << "outDirOverride" << outDirOverride;
        f << "recursive" << recursive;
        f << "minFacePx" << minFacePx;
        f << "backend" << backend;
        f << "workers" << workers;
        f << "exportAnnotated" << exportAnnotated;
        f << "exportCsv" << exportCsv;
        f << "exportJson" << exportJson;
        f << "moveRejects" << moveRejects;
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

// ApexFace - persistent settings (YAML next to the executable)
#pragma once
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

struct Settings {
    std::string lastFolder;        // UTF-8
    std::string outDirOverride;    // empty = <folder>/_apexface_report
    bool recursive = true;
    int minFacePx = 28;
    std::string backend = "auto";  // auto | cpu | cuda
    int workers = 0;               // 0 = auto
    bool exportAnnotated = true;
    bool exportCsv = true;
    bool exportJson = true;
    bool generateReport = true; // false = scoring pass only: no thumbs/annotated/HTML/CSV/JSON
    bool moveRejects = false;   // move grade-D / no-face files to <folder>/_apexface_rejects
    int reportCardsPerPage = 200;
    std::string logLevel = "trace"; // trace | debug | info | warn | error
    std::string lang = "en";        // en | th

    bool load(const fs::path& file);
    bool save(const fs::path& file) const;
    static fs::path defaultPath(); // <exeDir>/config/settings.yml
};

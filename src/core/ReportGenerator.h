// ApexFace - HTML/CSV/JSON report generation
#pragma once
#include "Analyzer.h"

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct ReportOptions {
    fs::path outDir;
    std::string sourceFolderUtf8;
    bool exportCsv = true;
    bool exportJson = true;
    int cardsPerPage = 200;
    std::string backend;
    int workers = 0;
    double elapsedSec = 0;
    fs::path logFileToCopy; // session log copied into <outDir>/logs/
    bool canceled = false;
};

class ReportGenerator {
public:
    // Writes index.html + cards_NNN.html + data.csv + data.json.
    // Returns false with errorOut on failure. indexRelOut = "index.html".
    static bool generate(const std::vector<ImageResult>& results, const RunSummary& sum,
                         const ReportOptions& opt, std::string& indexRelOut, std::string& errorOut);
};

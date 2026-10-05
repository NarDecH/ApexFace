// ApexFace - main GUI application (Dear ImGui + GLFW + OpenGL)
#include "Theme.h"

#include "../core/Analyzer.h"
#include "../core/ImageIO.h"
#include "../core/Logger.h"
#include "../core/Platform.h"
#include "../core/ReportGenerator.h"
#include "../core/Settings.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <nfd.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

#ifndef GL_BGR
#define GL_BGR 0x80E0
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#include <opencv2/core.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <list>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace gui {
namespace {

struct Tex {
    unsigned id = 0;
    int w = 0, h = 0;
    bool valid() const { return id != 0; }
};

struct RunState {
    std::mutex mtx;
    std::vector<ImageResult> all;
    RunSummary summary;
    bool reportOk = false;
    std::string reportErr;
    std::atomic<int> done{0};
    std::atomic<int> total{0};
    std::atomic<bool> running{false};
    std::atomic<bool> cancel{false};
    std::atomic<bool> reportDone{false};
    CurrentFileReporter current;
};

struct I18n {
    std::string lang = "en";
    const char* operator()(const char* en, const char* th) const {
        return lang == "th" ? th : en;
    }
};

ImVec4 rgb(int r, int g, int b, int a = 255) {
    return ImVec4(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
}
const ImVec4& ratingColorVec(double score) {
    static ImVec4 cols[5] = {rgb(16, 185, 129), rgb(74, 222, 128), rgb(56, 189, 248),
                             rgb(245, 158, 11), rgb(239, 68, 68)};
    int i = score >= 85 ? 0 : score >= 72 ? 1 : score >= 58 ? 2 : score >= 42 ? 3 : 4;
    return cols[i];
}

std::string f1s(double v) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(1) << v;
    return os.str();
}

class App {
public:
    int run(const wchar_t* initialFolderWide);

private:
    // ------- UI panels
    void buildDockLayout(ImGuiID dockspaceId);
    void drawMenuBar();
    void drawControls();
    void drawResults();
    void drawPreview();
    void drawLogPanel();

    // ------- actions
    void startRun();
    void drainNewResults();
    void clearTextures();

    bool pickFolder(std::string& outUtf8);
    Tex getTex(const std::string& rel);

    // ------- members
    GLFWwindow* window_ = nullptr;
    Settings st_;
    I18n tr_;
    RunState run_;
    std::thread worker_;
    std::vector<ImageResult> ui_;
    size_t consumed_ = 0;
    int selected_ = -1;
    fs::path outDir_;
    std::string outDirU8_;
    std::chrono::steady_clock::time_point runStart_;
    std::string lastError_;
    std::string infoLine_;

    // incremental stats over ui_
    int okCount_ = 0, noFaceCount_ = 0, errCount_ = 0, faceCount_ = 0;
    double bestBest_ = 0;
    std::string bestFile_;

    std::map<std::string, Tex> texs_;
    std::list<std::string> lru_;

    float fontScale_ = 1.f;
    ImFont* headingFont_ = nullptr;
    bool dockInit_ = true;

    // log panel
    std::vector<Logger::Entry> logLines_;
    uint64_t logVer_ = 0;
    bool levelShown_[5] = {false, true, true, true, true};
    bool autoScroll_ = true;

};

// ---------------------------------------------------------------------------

int App::run(const wchar_t* initialFolderWide) {
    st_.load(Settings::defaultPath());
    if (initialFolderWide) {
        st_.lastFolder = platform::utf8str(fs::path(initialFolderWide));
    }
    tr_.lang = st_.lang;

    Logger::Config lc;
    lc.dir = platform::exeDir() / "logs";
    lc.sessionTag = "gui";
    lc.consoleEcho = false;
    lc.fileLevel = LogLevel::Trace;
    Logger::instance().start(lc);

    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    window_ = glfwCreateWindow(1460, 940, "ApexFace 1.0.0 — Sharpest Face Finder", nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // keep the app dir clean

    int fw = 0, fh = 0, ww = 0, wh = 0;
    glfwGetFramebufferSize(window_, &fw, &fh);
    glfwGetWindowSize(window_, &ww, &wh);
    fontScale_ = ww > 0 ? std::clamp((float)fw / (float)ww, 1.0f, 3.0f) : 1.f;

    theme::applyStyle();
    ImGui::GetStyle().ScaleAllSizes(fontScale_);
    theme::loadFonts(15.5f * fontScale_, nullptr);
    headingFont_ = ImGui::GetIO().Fonts->Fonts.size() > 1
                       ? ImGui::GetIO().Fonts->Fonts[1]
                       : ImGui::GetIO().Fonts->Fonts[0];

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    AF_INFO("gui", "ApexFace 1.0.0 GUI started (fontScale=" << fontScale_ << ", opencv " << CV_VERSION << ")");
    Logger::instance().event("app", "start", "\"ui\":\"gui\",\"opencv\":\"" CV_VERSION "\"");

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (dockInit_) {
            ImGuiID dsid = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                                                        ImGuiDockNodeFlags_None);
            buildDockLayout(dsid);
        } else {
            ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_None);
        }

        drawMenuBar();
        drainNewResults();
        drawControls();
        drawResults();
        drawPreview();
        drawLogPanel();

        ImGui::Render();
        int dw, dh;
        glfwGetFramebufferSize(window_, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(0.05f, 0.066f, 0.09f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window_);
    }

    run_.cancel = true;
    if (worker_.joinable()) worker_.join();
    st_.lang = tr_.lang;
    st_.save(Settings::defaultPath());
    AF_INFO("gui", "ApexFace exited");
    Logger::instance().stop();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window_);
    glfwTerminate();
    return 0;
}

void App::buildDockLayout(ImGuiID dsid) {
    dockInit_ = false;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::DockBuilderRemoveNode(dsid);
    ImGui::DockBuilderAddNode(dsid, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dsid, vp->WorkSize);

    ImGuiID dLeft, dRest;
    ImGui::DockBuilderSplitNode(dsid, ImGuiDir_Left, 0.26f, &dLeft, &dRest);
    ImGuiID dLogs, dCenter;
    ImGui::DockBuilderSplitNode(dRest, ImGuiDir_Down, 0.20f, &dLogs, &dCenter);
    ImGuiID dPreview, dTable;
    ImGui::DockBuilderSplitNode(dCenter, ImGuiDir_Right, 0.30f, &dPreview, &dTable);

    ImGui::DockBuilderDockWindow("Controls", dLeft);
    ImGui::DockBuilderDockWindow("Log", dLogs);
    ImGui::DockBuilderDockWindow("Preview", dPreview);
    ImGui::DockBuilderDockWindow("Results", dTable);
    ImGui::DockBuilderFinish(dsid);
}

void App::drawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu(tr_("File", "ไฟล์"))) {
        if (ImGui::MenuItem(tr_("Open Folder...", "เปิดโฟลเดอร์..."), "Ctrl+O")) {
            std::string picked;
            if (pickFolder(picked)) st_.lastFolder = picked;
        }
        ImGui::Separator();
        if (ImGui::MenuItem(tr_("Exit", "ออก"), "Alt+F4")) glfwSetWindowShouldClose(window_, 1);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr_("Language", "ภาษา"))) {
        if (ImGui::MenuItem("English", nullptr, tr_.lang == "en")) tr_.lang = "en";
        if (ImGui::MenuItem("ไทย", nullptr, tr_.lang == "th")) tr_.lang = "th";
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr_("Help", "ช่วยเหลือ"))) {
        if (ImGui::MenuItem(tr_("About ApexFace", "เกี่ยวกับ ApexFace"))) ImGui::OpenPopup("About");
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();

    ImGui::SetNextWindowSize({560 * fontScale_, 0});
    if (ImGui::BeginPopupModal("About")) {
        ImGui::Text("ApexFace 1.0.0");
        ImGui::Separator();
        ImGui::TextUnformatted(tr_(
            "Find the sharpest face in every photo of a folder.\n"
            "YuNet face detection + multi-metric sharpness scoring,\n"
            "with an interactive HTML report.\n\nBuilt with OpenCV 4 + Dear ImGui.",
            "ค้นหาใบหน้าที่คมชัดที่สุดจากทุกรูปในโฟลเดอร์\nตรวจจับด้วย YuNet + ให้คะแนนความคมชัดแบบหลายเมตริก\n"
            "พร้อมรายงาน HTML แบบโต้ตอบ\n\nสร้างด้วย OpenCV 4 + Dear ImGui"));
        ImGui::Spacing();
        if (ImGui::Button("OK", {120 * fontScale_, 0})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

bool App::pickFolder(std::string& outUtf8) {
    nfdu8char_t* out = nullptr;
    nfdresult_t res = NFD_PickFolderU8(&out, nullptr);
    if (res == NFD_OKAY && out) {
        outUtf8 = (const char*)out;
        NFD_FreePathU8(out);
        return true;
    }
    return false;
}

void App::clearTextures() {
    for (auto& [k, t] : texs_)
        if (t.id) glDeleteTextures(1, &t.id);
    texs_.clear();
    lru_.clear();
}

void App::drainNewResults() {
    std::lock_guard<std::mutex> lk(run_.mtx);
    if (consumed_ >= run_.all.size()) return;
    for (size_t i = consumed_; i < run_.all.size(); ++i) {
        const ImageResult& r = run_.all[i];
        ui_.push_back(r);
        if (r.status == "ok") {
            ++okCount_;
            faceCount_ += (int)r.faces.size();
            if (r.bestScore > bestBest_) {
                bestBest_ = r.bestScore;
                bestFile_ = r.relPath;
                if (selected_ < 0) selected_ = (int)ui_.size() - 1;
            }
        } else if (r.status == "no_face") ++noFaceCount_;
        else if (r.status == "error") ++errCount_;
    }
    consumed_ = run_.all.size();
}

void App::startRun() {
    lastError_.clear();
    infoLine_.clear();
    if (st_.lastFolder.empty()) {
        lastError_ = tr_("Select a folder first.", "ยังไม่ได้เลือกโฟลเดอร์");
        return;
    }
    fs::path folder = platform::utf8path(st_.lastFolder);
    std::error_code ec;
    if (!fs::exists(folder, ec) || !fs::is_directory(folder, ec)) {
        lastError_ = tr_("Folder does not exist.", "ไม่พบโฟลเดอร์นี้");
        return;
    }
    fs::path model = platform::findResource("models/face_detection_yunet_2023mar.onnx");
    if (model.empty()) {
        lastError_ = tr_("Face model not found (models/face_detection_yunet_2023mar.onnx).",
                         "ไม่พบโมเดลใบหน้า (models/face_detection_yunet_2023mar.onnx)");
        AF_ERROR("gui", lastError_);
        return;
    }
    outDir_ = st_.outDirOverride.empty() ? folder / "_apexface_report"
                                         : platform::utf8path(st_.outDirOverride);
    fs::create_directories(outDir_, ec);
    outDirU8_ = platform::utf8str(outDir_);

    AnalyzeOptions o;
    o.folder = folder;
    o.outDir = outDir_;
    o.recursive = st_.recursive;
    o.minFacePx = st_.minFacePx;
    o.backend = st_.backend == "cuda" ? Backend::CUDA : st_.backend == "cpu" ? Backend::CPU : Backend::Auto;
    o.workers = st_.workers;
    o.exportAnnotated = st_.exportAnnotated;
    o.modelPathUtf8 = platform::utf8str(model);

    {
        std::lock_guard<std::mutex> lk(run_.mtx);
        run_.all.clear();
        run_.summary = RunSummary{};
        run_.reportOk = false;
        run_.reportErr.clear();
    }
    ui_.clear();
    consumed_ = 0;
    selected_ = -1;
    okCount_ = noFaceCount_ = errCount_ = faceCount_ = 0;
    bestBest_ = 0;
    bestFile_.clear();
    clearTextures();
    run_.cancel = false;
    run_.reportDone = false;
    run_.running = true;
    runStart_ = std::chrono::steady_clock::now();

    st_.save(Settings::defaultPath());
    AF_INFO("gui", "run started: " << st_.lastFolder << " backend=" << st_.backend);

    worker_ = std::thread([this, o] {
        Analyzer an;
        RunSummary sum = an.run(
            o,
            [this](ImageResult&& r) {
                std::lock_guard<std::mutex> lk(run_.mtx);
                run_.all.push_back(std::move(r));
            },
            run_.done, run_.total, run_.cancel, run_.current);

        {
            std::lock_guard<std::mutex> lk(run_.mtx);
            run_.summary = sum;
        }
        if (sum.backend == "init_failed") {
            std::lock_guard<std::mutex> lk(run_.mtx);
            run_.reportDone = true;
            run_.running = false;
            return;
        }
        ReportOptions ro;
        ro.outDir = o.outDir;
        ro.sourceFolderUtf8 = platform::utf8str(o.folder);
        ro.exportCsv = st_.exportCsv;
        ro.exportJson = st_.exportJson;
        ro.cardsPerPage = st_.reportCardsPerPage;
        ro.backend = sum.backend;
        ro.workers = sum.workers;
        ro.elapsedSec = sum.elapsedSec;
        ro.logFileToCopy = Logger::instance().filePath();
        ro.canceled = sum.canceled;

        std::vector<ImageResult> copy;
        {
            std::lock_guard<std::mutex> lk(run_.mtx);
            copy = run_.all;
        }
        std::string err;
        std::string idxRel;
        bool ok = ReportGenerator::generate(copy, sum, ro, idxRel, err);
        std::lock_guard<std::mutex> lk(run_.mtx);
        run_.reportOk = ok;
        run_.reportErr = err;
        run_.reportDone = true;
        run_.running = false;
    });
}

void App::drawControls() {
    ImGui::SetNextWindowSize({360 * fontScale_, 600 * fontScale_}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Controls")) {
        ImGui::End();
        return;
    }
    auto section = [&](const char* label) {
        ImGui::Spacing();
        ImGui::PushFont(headingFont_, 0.0f);
        ImGui::TextColored(rgb(139, 149, 167), "%s", label);
        ImGui::PopFont();
        ImGui::Separator();
    };

    // language toggle top-right
    ImGui::TextUnformatted("ApexFace");
    ImGui::SameLine(ImGui::GetWindowWidth() - 110 * fontScale_);
    if (ImGui::SmallButton("EN")) tr_.lang = "en";
    ImGui::SameLine();
    if (ImGui::SmallButton("ไทย")) tr_.lang = "th";

    section(tr_("Source", "แหล่งรูป"));
    if (ImGui::Button(tr_("Browse Folder...", "เลือกโฟลเดอร์..."), {-1, 0})) {
        std::string picked;
        if (pickFolder(picked)) st_.lastFolder = picked;
    }
    ImGui::PushStyleColor(ImGuiCol_FrameBg, rgb(12, 16, 22));
    ImGui::BeginChild("srcpath", {0, ImGui::GetTextLineHeight() * 2.2f + 8}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::TextWrapped("%s", st_.lastFolder.empty() ? tr_("(no folder selected)", "(ยังไม่ได้เลือกโฟลเดอร์)")
                                                    : st_.lastFolder.c_str());
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Checkbox(tr_("Include subfolders", "รวมโฟลเดอร์ย่อย"), &st_.recursive);

    section(tr_("Detection", "การตรวจจับ"));
    int minFace = st_.minFacePx;
    if (ImGui::SliderInt(tr_("Min face (px)", "ใบหน้าต่ำสุด (px)"), &minFace, 16, 128, "%d px"))
        st_.minFacePx = minFace;
    const char* beItems[] = {"Auto", "CPU", "CUDA"};
    int be = st_.backend == "cuda" ? 2 : st_.backend == "cpu" ? 1 : 0;
    if (ImGui::Combo(tr_("Backend", "Backend"), &be, beItems, 3))
        st_.backend = be == 2 ? "cuda" : be == 1 ? "cpu" : "auto";
    ImGui::SliderInt(tr_("Workers (0=auto)", "เธรด (0=อัตโนมัติ)"), &st_.workers, 0, 16);

    section(tr_("Output", "ผลลัพธ์"));
    ImGui::BeginChild("outpath", {0, ImGui::GetTextLineHeight() * 2.2f + 8}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    std::string outShown = st_.outDirOverride.empty()
                               ? (st_.lastFolder + "/_apexface_report")
                               : st_.outDirOverride;
    ImGui::TextWrapped("%s", outShown.c_str());
    ImGui::EndChild();
    if (ImGui::Button(tr_("Choose Output Folder...", "เลือกโฟลเดอร์ผลลัพธ์..."))) {
        std::string picked;
        if (pickFolder(picked)) st_.outDirOverride = picked;
    }
    ImGui::SameLine();
    if (ImGui::Button(tr_("Reset", "ค่าเริ่มต้น")) && !st_.outDirOverride.empty()) {
        st_.outDirOverride.clear();
    }
    ImGui::Checkbox(tr_("Save annotated images", "บันทึกรูปพร้อมกรอบหน้า"), &st_.exportAnnotated);
    ImGui::Checkbox("CSV", &st_.exportCsv);
    ImGui::SameLine();
    ImGui::Checkbox("JSON", &st_.exportJson);

    section(tr_("Run", "การทำงาน"));
    if (!run_.running) {
        ImGui::PushStyleColor(ImGuiCol_Button, rgb(34, 197, 94));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgb(52, 211, 153));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgb(34, 197, 94));
        ImGui::PushStyleColor(ImGuiCol_Text, rgb(10, 20, 14));
        if (ImGui::Button(tr_("Start Analysis", "เริ่มวิเคราะห์"), {-1, 46 * fontScale_})) startRun();
        ImGui::PopStyleColor(4);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, rgb(239, 68, 68));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgb(248, 113, 113));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgb(239, 68, 68));
        ImGui::PushStyleColor(ImGuiCol_Text, rgb(28, 12, 12));
        if (ImGui::Button(tr_("Cancel", "ยกเลิก"), {-1, 46 * fontScale_})) run_.cancel = true;
        ImGui::PopStyleColor(4);
    }

    double total = run_.total.load();
    double done = run_.done.load();
    if (run_.running || (done > 0 && !ui_.empty())) {
        double frac = total > 0 ? done / total : 0.0;
        char overlay[64];
        snprintf(overlay, sizeof(overlay), "%d / %d", (int)done, (int)total);
        ImGui::ProgressBar((float)frac, {-1, 0}, overlay);
        std::string cur = run_.current.get();
        if (run_.running && !cur.empty()) {
            ImGui::TextDisabled("%s", cur.c_str());
        }
        double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - runStart_).count();
        double eta = done > 0 ? elapsed / done * (total - done) : 0.0;
        ImGui::TextDisabled("%s %.0fs · %s ~%.0fs", tr_("Elapsed", "ใช้เวลา"), elapsed,
                            tr_("ETA", "อีกประมาณ"), eta);
    } else {
        ImGui::TextDisabled("%s", tr_("Ready. Pick a folder and start.", "พร้อมทำงาน — เลือกโฟลเดอร์แล้วกดเริ่ม"));
    }

    if (!lastError_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, rgb(248, 113, 113));
        ImGui::TextWrapped("%s", lastError_.c_str());
        ImGui::PopStyleColor();
    }

    if (run_.reportDone) {
        if (run_.summary.backend == "init_failed") {
            ImGui::PushStyleColor(ImGuiCol_Text, rgb(248, 113, 113));
            ImGui::TextWrapped("%s", tr_("Detector init failed — see the Log panel.",
                                         "เริ่มตัวตรวจจับไม่สำเร็จ — ดูแผงบันทึกการทำงาน"));
            ImGui::PopStyleColor();
        } else if (run_.reportOk) {
            ImGui::PushStyleColor(ImGuiCol_Text, rgb(74, 222, 128));
            ImGui::TextUnformatted(tr_("Report ready.", "รายงานพร้อมแล้ว"));
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Button, rgb(79, 140, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgb(106, 160, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgb(62, 118, 226));
            if (ImGui::Button(tr_("Open Report", "เปิดรายงาน"), {-1, 0})) {
                platform::openFileOrFolder(outDir_ / "index.html");
            }
            ImGui::PopStyleColor(3);
            if (ImGui::Button(tr_("Open Output Folder", "เปิดโฟลเดอร์ผลลัพธ์"), {-1, 0})) {
                platform::openFileOrFolder(outDir_);
            }
        } else if (!run_.reportErr.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, rgb(248, 113, 113));
            ImGui::TextWrapped("report error: %s", run_.reportErr.c_str());
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();
}

void App::drawResults() {
    if (!ImGui::Begin("Results")) {
        ImGui::End();
        return;
    }

    // stat strip
    ImGuiStyle& sty = ImGui::GetStyle();
    auto statChip = [&](const char* k, const std::string& v, const ImVec4& col) {
        ImGui::BeginGroup();
        ImGui::PushFont(headingFont_, 0.0f);
        ImGui::TextColored(col, "%s", v.c_str());
        ImGui::PopFont();
        ImGui::TextDisabled("%s", k);
        ImGui::EndGroup();
        ImGui::SameLine(0, 26 * fontScale_);
    };
    statChip(tr_("images", "รูป"), std::to_string(ui_.size()), rgb(230, 234, 242));
    statChip(tr_("with faces", "มีใบหน้า"), std::to_string(okCount_), rgb(74, 222, 128));
    statChip(tr_("no face", "ไม่พบใบหน้า"), std::to_string(noFaceCount_), rgb(139, 149, 167));
    statChip(tr_("errors", "ผิดพลาด"), std::to_string(errCount_), errCount_ ? rgb(239, 68, 68) : rgb(230, 234, 242));
    statChip(tr_("faces", "ใบหน้า"), std::to_string(faceCount_), rgb(56, 189, 248));
    statChip(tr_("best", "ดีที่สุด"), okCount_ ? f1s(bestBest_) : "-", rgb(79, 140, 255));
    ImGui::NewLine();

    static ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                   ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
                                   ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV;
    if (ImGui::BeginTable("results", 8, flags, {-1, -1})) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 46 * fontScale_);
        ImGui::TableSetupColumn(tr_("File", "ไฟล์"), ImGuiTableColumnFlags_WidthStretch, 5);
        ImGui::TableSetupColumn(tr_("Size", "ขนาด"), ImGuiTableColumnFlags_WidthFixed, 90 * fontScale_);
        ImGui::TableSetupColumn(tr_("Faces", "ใบหน้า"), ImGuiTableColumnFlags_WidthFixed, 56 * fontScale_);
        ImGui::TableSetupColumn(tr_("Best", "คะแนน"), ImGuiTableColumnFlags_WidthFixed, 62 * fontScale_);
        ImGui::TableSetupColumn(tr_("Rating", "ระดับ"), ImGuiTableColumnFlags_WidthFixed, 56 * fontScale_);
        ImGui::TableSetupColumn(tr_("Status", "สถานะ"), ImGuiTableColumnFlags_WidthFixed, 74 * fontScale_);
        ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthFixed, 62 * fontScale_);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin((int)ui_.size());
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const ImageResult& r = ui_[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                char idx[16];
                snprintf(idx, sizeof(idx), "%d", r.idx + 1);
                ImGui::Selectable(idx, selected_ == i,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
                if (ImGui::IsItemClicked()) selected_ = i;
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(r.name.c_str());
                if (ImGui::IsItemHovered() && r.relPath != r.name) {
                    ImGui::SetTooltip("%s", r.relPath.c_str());
                }
                ImGui::TableNextColumn();
                if (r.width) ImGui::Text("%dx%d", r.width, r.height);
                else ImGui::TextUnformatted("-");
                ImGui::TableNextColumn();
                ImGui::Text("%d", (int)r.faces.size());
                ImGui::TableNextColumn();
                if (r.bestIdx >= 0) {
                    ImGui::TextColored(ratingColorVec(r.bestScore), "%.1f", r.bestScore);
                } else ImGui::TextUnformatted("-");
                ImGui::TableNextColumn();
                if (r.bestIdx >= 0) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ratingColorVec(r.bestScore));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ratingColorVec(r.bestScore));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ratingColorVec(r.bestScore));
                    ImGui::PushStyleColor(ImGuiCol_Text, rgb(10, 14, 20));
                    ImGui::SmallButton(sharp::ratingLabel(r.bestScore));
                    ImGui::PopStyleColor(4);
                } else ImGui::TextUnformatted("-");
                ImGui::TableNextColumn();
                if (r.status == "ok") ImGui::TextColored(rgb(74, 222, 128), "%s", tr_("ok", "สำเร็จ"));
                else if (r.status == "no_face") ImGui::TextColored(rgb(139, 149, 167), "%s", tr_("no face", "ไม่พบใบหน้า"));
                else ImGui::TextColored(rgb(239, 68, 68), "%s", tr_("error", "ผิดพลาด"));
                ImGui::TableNextColumn();
                ImGui::Text("%.0f", r.totalMs);
            }
        }
        ImGui::EndTable();
    }
    (void)sty;
    ImGui::End();
}

Tex App::getTex(const std::string& rel) {
    auto it = texs_.find(rel);
    if (it != texs_.end()) {
        lru_.remove(rel);
        lru_.push_front(rel);
        return it->second;
    }
    Tex t;
    if (outDirU8_.empty() || rel.empty()) return t;
    fs::path p = outDir_ / platform::utf8path(rel);
    std::string err;
    cv::Mat m = imageio::loadBgr(p, err);
    if (m.empty()) return t;

    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m.cols, m.rows, 0, GL_BGR, GL_UNSIGNED_BYTE, m.data);
    t = Tex{id, m.cols, m.rows};
    texs_[rel] = t;
    lru_.push_front(rel);
    if (lru_.size() > 120) {
        std::string old = lru_.back();
        lru_.pop_back();
        auto oi = texs_.find(old);
        if (oi != texs_.end()) {
            if (oi->second.id) glDeleteTextures(1, &oi->second.id);
            texs_.erase(oi);
        }
    }
    return t;
}

void App::drawPreview() {
    if (!ImGui::Begin("Preview")) {
        ImGui::End();
        return;
    }
    if (selected_ < 0 || selected_ >= (int)ui_.size()) {
        ImGui::TextDisabled("%s", tr_("Select a row in Results.", "เลือกแถวในผลลัพธ์เพื่อดูรูป"));
        ImGui::End();
        return;
    }
    const ImageResult& r = ui_[selected_];
    ImGui::PushFont(headingFont_, 0.0f);
    ImGui::TextUnformatted(r.name.c_str());
    ImGui::PopFont();
    ImGui::TextDisabled("%s", r.relPath.c_str());

    if (r.status == "error") {
        ImGui::PushStyleColor(ImGuiCol_Text, rgb(248, 113, 113));
        ImGui::TextWrapped("%s", r.error.c_str());
        ImGui::PopStyleColor();
        ImGui::End();
        return;
    }

    Tex tex = getTex(r.thumbRel);
    if (tex.valid()) {
        float avail = ImGui::GetContentRegionAvail().x;
        float h = avail * (float)tex.h / (float)std::max(1, tex.w);
        ImGui::Image((ImTextureID)(ImU64)tex.id, {avail, h}, {0, 0}, {1, 1});
        if (ImGui::IsItemHovered() && r.bestIdx >= 0) {
            ImGui::SetTooltip("%s %.1f", tr_("Best face score:", "คะแนนใบหน้าดีที่สุด:"), r.bestScore);
        }
    } else {
        ImGui::TextDisabled("(%s)", tr_("thumbnail unavailable", "ไม่มีภาพย่อ"));
    }

    ImGui::Spacing();
    if (r.bestIdx >= 0) {
        const FaceResult& bf = r.faces[r.bestIdx];
        ImGui::PushFont(headingFont_, 0.0f);
        ImGui::TextColored(ratingColorVec(bf.score), "%s %.1f  (%s)", tr_("Sharpest:", "คมชัดสุด:"),
                           bf.score, sharp::ratingLabel(bf.score));
        ImGui::PopFont();
    }
    ImGui::Separator();
    for (size_t i = 0; i < r.faces.size(); ++i) {
        const FaceResult& f = r.faces[i];
        bool best = (int)i == r.bestIdx;
        ImGui::PushStyleColor(ImGuiCol_Text, ratingColorVec(f.score));
        ImGui::BulletText("%s %zu: %.1f", tr_("Face", "ใบหน้า"), i + 1, f.score);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextDisabled("[%d,%d %dx%d]%s", f.box.x, f.box.y, f.box.width, f.box.height,
                            best ? " \xe2\x98\x85#1" : "");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("lapVar=%.1f\ntenengrad=%.1f\nfftHF=%.3f\ncontrast=%.1f\nconf=%.2f",
                              f.metrics.lapVar, f.metrics.tenengrad, f.metrics.fftHF,
                              f.metrics.contrastRMS, f.conf);
        }
    }

    ImGui::Spacing();
    float w = ImGui::GetContentRegionAvail().x;
    float bw = (w - 8 * fontScale_) * 0.5f;
    if (!r.annotatedRel.empty()) {
        if (ImGui::Button(tr_("Open Full Image", "เปิดรูปเต็ม"), {bw, 0}))
            platform::openFileOrFolder(outDir_ / platform::utf8path(r.annotatedRel));
        ImGui::SameLine();
    }
    if (ImGui::Button(tr_("Show In Folder", "แสดงในโฟลเดอร์"), {bw, 0}))
        platform::openFileOrFolder(outDir_ / "thumbs");
    ImGui::End();
}

void App::drawLogPanel() {
    if (!ImGui::Begin("Log")) {
        ImGui::End();
        return;
    }
    static const char* names[5] = {"Trace", "Debug", "Info", "Warn", "Error"};
    static const ImVec4 cols[5] = {rgb(107, 114, 128), rgb(139, 149, 167), rgb(230, 234, 242),
                                   rgb(245, 158, 11), rgb(239, 68, 68)};
    for (int i = 0; i < 5; ++i) {
        ImGui::Checkbox(names[i], &levelShown_[i]);
        ImGui::SameLine(0, 14 * fontScale_);
    }
    ImGui::Checkbox(tr_("Auto-scroll", "เลื่อนอัตโนมัติ"), &autoScroll_);
    ImGui::SameLine();
    if (ImGui::SmallButton(tr_("Open log folder", "เปิดโฟลเดอร์ log"))) {
        platform::openFileOrFolder(platform::exeDir() / "logs");
    }
    ImGui::Separator();

    uint64_t ver = 0;
    logLines_ = Logger::instance().snapshotGui(ver);

    ImGui::BeginChild("logscroll", {0, 0}, ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& e : logLines_) {
        if (!levelShown_[(int)e.lv]) continue;
        ImGui::TextColored(cols[(int)e.lv], "%s [%s] %s", e.stamp.c_str(), e.cat.c_str(), e.msg.c_str());
    }
    if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4)
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    ImGui::End();
}

} // namespace

int RunApexFaceGui(const wchar_t* initialFolderWide) {
    App app;
    return app.run(initialFolderWide);
}

} // namespace gui

// ApexFace - GUI entry point (WinMain); delegates to gui::RunApexFaceGui()
// usage: ApexFace.exe [folder] [--autorun]   (--autorun starts the analysis immediately)
#include <windows.h>
#include <shellapi.h>

namespace gui {
int RunApexFaceGui(const wchar_t* initialFolderWide, bool autoStart);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    const wchar_t* folder = nullptr;
    bool autoStart = false;
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            if (wcscmp(argv[i], L"--autorun") == 0) autoStart = true;
            else if (!folder) folder = argv[i];
        }
    }
    int rc = gui::RunApexFaceGui(folder, autoStart);
    if (argv) LocalFree(argv);
    return rc;
}

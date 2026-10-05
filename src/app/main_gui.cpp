// ApexFace - GUI entry point (WinMain); delegates to gui::RunApexFaceGui()
#include <windows.h>
#include <shellapi.h>

namespace gui {
int RunApexFaceGui(const wchar_t* initialFolderWide);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR lpCmdLine, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    const wchar_t* folder = (argv && argc > 1) ? argv[1] : nullptr;
    int rc = gui::RunApexFaceGui(folder);
    if (argv) LocalFree(argv);
    return rc;
}

#include "Platform.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

namespace platform {

fs::path exeDir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    fs::path p = fs::path(std::wstring(buf, n)).parent_path();
    return p;
}

fs::path utf8path(const std::string& s) {
    if (s.empty()) return {};
    int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), wlen);
    return fs::path(w);
}

std::string utf8str(const fs::path& p) {
    std::wstring w = p.wstring();
    if (w.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), len, nullptr, nullptr);
    return s;
}

void openFileOrFolder(const fs::path& p) {
    if (p.empty()) return;
    ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

bool isDirInside(const fs::path& dir, const fs::path& candidate) {
    auto d = fs::weakly_canonical(dir);
    auto c = fs::weakly_canonical(candidate);
    auto di = d.begin();
    auto ci = c.begin();
    for (; di != d.end(); ++di, ++ci) {
        if (ci == c.end() || *di != *ci) return false;
    }
    return ci != c.end();
}

fs::path findResource(const std::string& relUtf8) {
    fs::path base = exeDir();
    for (int i = 0; i < 4; ++i) {
        fs::path candidate = base / utf8path(relUtf8);
        std::error_code ec;
        if (fs::exists(candidate, ec)) return candidate;
        base = base.parent_path();
    }
    fs::path cwd = fs::current_path() / utf8path(relUtf8);
    std::error_code ec;
    if (fs::exists(cwd, ec)) return cwd;
    return {};
}

} // namespace platform

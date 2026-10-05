// ApexFace - platform helpers: UTF-8 <-> wide paths, exe location, shell open
#pragma once
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace platform {

fs::path exeDir();
fs::path utf8path(const std::string& s);   // UTF-8 -> fs::path (wide on Windows)
std::string utf8str(const fs::path& p);    // fs::path -> UTF-8
void openFileOrFolder(const fs::path& p);  // ShellExecute "open"
bool isDirInside(const fs::path& dir, const fs::path& candidate); // candidate inside dir?

// Locate a resource next to the exe, walking up a few levels (dev tree vs packaged layout).
fs::path findResource(const std::string& relUtf8);

} // namespace platform

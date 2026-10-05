// ApexFace - ImGui dark theme + font loading (Thai-capable Sarabun)
#pragma once
#include <string>

namespace theme {

void applyStyle();
// Returns true if the Thai-capable font was loaded (else ImGui falls back to default).
bool loadFonts(float pixelSize, float* headingSizeOut);

} // namespace theme

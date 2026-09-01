#pragma once

#include <string>

// Resolve Scheme/Windows font name to a font file path.
// Search order (Linux):
// 1) CSRETRO_UI_FONTS env dir
// 2) $XASH3D_RODIR/platform/resource/linux_fonts/
// 3) relative gamedata paths if set
// 4) optional system fallback (known dirs) — NEVER hardcode only
//    /usr/share/fonts/TTF/DejaVuSans.ttf as sole path
// Map: Tahoma/Verdana/Arial/Trebuchet MS → DejaVuSans.ttf or LiberationSans-Regular.ttf
// Bold weight (>=600) → DejaVuSans-Bold.ttf / LiberationSans-Bold.ttf
std::string Csretro_ResolveFontFile(const char *familyOrFile, int weight);
void Csretro_AddFontSearchDir(const char *dir);

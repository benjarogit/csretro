#pragma once

#include <string>

// Resolve Scheme/Windows font name to a font file path.
// CS Retro ships Noto Sans (SIL OFL 1.1) as the only UI family; Steam fonts are not used.
// Search order:
// 1) CSRETRO_UI_FONTS env dir
// 2) $XASH3D_RODIR/platform/resource/csretro_fonts/
// 3) relative gamedata paths if set
// 4) system font dirs — only as a safety net, never a single hardcoded path
// Map: Tahoma/Verdana/Arial/Trebuchet MS/… → NotoSans-Regular.ttf, bold (>=600) → NotoSans-Bold.ttf
//      Courier/Consolas/Lucida Console     → NotoSansMono-Regular.ttf
// Marlett is drawn geometrically (vgui_symbols.cpp), never resolved to a file.
std::string Csretro_ResolveFontFile(const char *familyOrFile, int weight);
void Csretro_AddFontSearchDir(const char *dir);

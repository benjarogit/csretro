#pragma once

#include <cstdint>

// Platform-neutral VGUI2 symbol fonts (Windows Marlett semantics without Marlett.ttf).
// Used by Frame close/min/max, ComboBox/ScrollBar arrows, Menu cascades, etc.
namespace CsretroVguiSymbols
{
bool IsSymbolFontName(const char *windowsFontName);

// Draw one Marlett-codepoint glyph into the active surface coordinate space.
// fill(x0,y0,x1,y1) and line(x0,y0,x1,y1) must already respect draw color.
// Returns true if the codepoint was handled as a symbol (caller should not FreeType it).
bool PaintCodepoint(int x, int y, int tall, uint32_t codepoint,
	void (*fill)(int x0, int y0, int x1, int y1),
	void (*line)(int x0, int y0, int x1, int y1));

int AdvanceForTall(int tall);
}

#include "vgui_symbols.h"

#include <cstring>
#include <strings.h>

namespace CsretroVguiSymbols
{
bool IsSymbolFontName(const char *windowsFontName)
{
	if (!windowsFontName || !*windowsFontName)
		return false;
	// TrackerScheme / vgui_controls: "Marlett", "MarlettSmall"
	return !strcasecmp(windowsFontName, "Marlett") || !strcasecmp(windowsFontName, "MarlettSmall");
}

int AdvanceForTall(int tall)
{
	return tall > 0 ? tall : 12;
}

namespace
{
void TriDown(int x, int y, int s, void (*fill)(int, int, int, int), void (*line)(int, int, int, int))
{
	(void)fill;
	const int mid = x + s / 2;
	const int top = y + s / 3;
	const int bot = y + (2 * s) / 3;
	line(x + 2, top, mid, bot);
	line(mid, bot, x + s - 2, top);
	line(x + 2, top, x + s - 2, top);
}

void TriUp(int x, int y, int s, void (*fill)(int, int, int, int), void (*line)(int, int, int, int))
{
	(void)fill;
	const int mid = x + s / 2;
	const int top = y + s / 3;
	const int bot = y + (2 * s) / 3;
	line(x + 2, bot, mid, top);
	line(mid, top, x + s - 2, bot);
	line(x + 2, bot, x + s - 2, bot);
}

void TriRight(int x, int y, int s, void (*fill)(int, int, int, int), void (*line)(int, int, int, int))
{
	(void)fill;
	const int mid = y + s / 2;
	const int left = x + s / 3;
	const int right = x + (2 * s) / 3;
	line(left, y + 2, right, mid);
	line(right, mid, left, y + s - 2);
	line(left, y + 2, left, y + s - 2);
}

void TriLeft(int x, int y, int s, void (*fill)(int, int, int, int), void (*line)(int, int, int, int))
{
	(void)fill;
	const int mid = y + s / 2;
	const int left = x + s / 3;
	const int right = x + (2 * s) / 3;
	line(right, y + 2, left, mid);
	line(left, mid, right, y + s - 2);
	line(right, y + 2, right, y + s - 2);
}

void Cross(int x, int y, int s, void (*line)(int, int, int, int))
{
	// 2px-thick X so small title buttons stay readable.
	line(x + 2, y + 2, x + s - 2, y + s - 2);
	line(x + 3, y + 2, x + s - 1, y + s - 2);
	line(x + s - 2, y + 2, x + 2, y + s - 2);
	line(x + s - 1, y + 2, x + 3, y + s - 2);
}

void CheckMark(int x, int y, int s, void (*line)(int, int, int, int))
{
	const int x0 = x + s / 5;
	const int y0 = y + s / 2;
	const int x1 = x + s / 2 - 1;
	const int y1 = y + s - 3;
	const int x2 = x + s - 3;
	const int y2 = y + 3;
	line(x0, y0, x1, y1);
	line(x1, y1, x2, y2);
}

void GripLines(int x, int y, int s, bool shadow, void (*line)(int, int, int, int))
{
	// Three crisp diagonal pairs, matching the classic Windows/VGUI resize
	// handle without relying on a platform-specific Marlett bitmap.
	for (int inset = 4; inset <= s - 1; inset += 4)
	{
		const int offset = shadow ? 1 : 0;
		line(x + s - inset + offset, y + s - 2 + offset,
			x + s - 2 + offset, y + s - inset + offset);
	}
}
} // namespace

bool PaintCodepoint(int x, int y, int tall, uint32_t codepoint,
	void (*fill)(int x0, int y0, int x1, int y1),
	void (*line)(int x0, int y0, int x1, int y1))
{
	if (!fill || !line)
		return false;
	const int s = AdvanceForTall(tall);
	const int ch = static_cast<int>(codepoint);

	switch (ch)
	{
	case 'r': // close
	case 'R':
		Cross(x, y, s, line);
		return true;
	case '0': // minimize
		fill(x + 2, y + s - 4, x + s - 2, y + s - 2);
		return true;
	case '1': // maximize
		fill(x + 2, y + 2, x + s - 2, y + 4);
		fill(x + 2, y + 2, x + 4, y + s - 2);
		fill(x + s - 4, y + 2, x + s - 2, y + s - 2);
		fill(x + 2, y + s - 4, x + s - 2, y + s - 2);
		return true;
	case '2': // restore
		fill(x + 4, y + 2, x + s - 2, y + 4);
		fill(x + s - 4, y + 2, x + s - 2, y + s - 4);
		fill(x + 2, y + 5, x + s - 5, y + 7);
		fill(x + 2, y + 5, x + 4, y + s - 2);
		fill(x + s - 7, y + 5, x + s - 5, y + s - 2);
		fill(x + 2, y + s - 4, x + s - 5, y + s - 2);
		return true;
	case 'u': // down
	case 'U':
		TriDown(x, y, s, fill, line);
		return true;
	case 't': // up
	case 'T':
		TriUp(x, y, s, fill, line);
		return true;
	case '4': // right (menu cascade / expand)
		TriRight(x, y, s, fill, line);
		return true;
	case '3': // left
		TriLeft(x, y, s, fill, line);
		return true;
	case '6': // down (expand selected) — same as u, slightly lower
		TriDown(x, y, s, fill, line);
		return true;
	case 'a': // menu check
	case 'b': // checkbox check (Win Marlett)
		CheckMark(x, y, s, line);
		return true;
	case 'o': // grip shadow
		GripLines(x, y, s, true, line);
		return true;
	case 'p': // grip highlight
		GripLines(x, y, s, false, line);
		return true;
	// Radio / checkbox border glyphs — approximate with box so letter never shows.
	case 'g':
	case 'e':
	case 'f':
	case 'n':
	case 'j':
	case 'k':
		fill(x + 1, y + 1, x + s - 1, y + 2);
		fill(x + 1, y + 1, x + 2, y + s - 1);
		fill(x + s - 2, y + 1, x + s - 1, y + s - 1);
		fill(x + 1, y + s - 2, x + s - 1, y + s - 1);
		return true;
	case 'h': // radio selected dot
		fill(x + s / 3, y + s / 3, x + (2 * s) / 3, y + (2 * s) / 3);
		return true;
	default:
		// Unknown Marlett codepoint: still consume as empty advance (never fall to Latin TTF).
		return true;
	}
}
} // namespace CsretroVguiSymbols

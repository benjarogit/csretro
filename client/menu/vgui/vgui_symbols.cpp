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
using FillFn = void (*)(int, int, int, int);
using LineFn = void (*)(int, int, int, int);

int ClampPad(int s, int denom)
{
	const int pad = s / denom;
	return pad < 2 ? 2 : pad;
}

// 2x2-SSAA: Pixel bleibt, wenn mindestens zwei der vier Subsamples im Kreis liegen.
bool DiskHit(int dx, int dy, int r)
{
	const int r2 = (2 * r) * (2 * r);
	int hits = 0;
	for (int sy = 0; sy < 2; ++sy)
	{
		for (int sx = 0; sx < 2; ++sx)
		{
			const int sdx = 2 * dx + sx;
			const int sdy = 2 * dy + sy;
			if (sdx * sdx + sdy * sdy <= r2)
				++hits;
		}
	}
	return hits >= 2;
}

void FillDisk(int cx, int cy, int r, FillFn fill)
{
	if (!fill || r < 1)
		return;
	for (int dy = -r; dy <= r; ++dy)
	{
		int x0 = 0;
		bool run = false;
		for (int dx = -r; dx <= r; ++dx)
		{
			if (DiskHit(dx, dy, r))
			{
				if (!run)
				{
					x0 = cx + dx;
					run = true;
				}
			}
			else if (run)
			{
				fill(x0, cy + dy, cx + dx, cy + dy + 1);
				run = false;
			}
		}
		if (run)
			fill(x0, cy + dy, cx + r + 1, cy + dy + 1);
	}
}

void FillRingBevel(int cx, int cy, int rOuter, int rInner, bool dark, FillFn fill)
{
	if (!fill || rOuter < 1)
		return;
	if (rInner < 0)
		rInner = 0;
	for (int dy = -rOuter; dy <= rOuter; ++dy)
	{
		int x0 = 0;
		bool run = false;
		for (int dx = -rOuter; dx <= rOuter; ++dx)
		{
			const bool inOuter = DiskHit(dx, dy, rOuter);
			const bool inInner = rInner > 0 && DiskHit(dx, dy, rInner);
			const bool upperLeft = (dx + dy) <= 0;
			const bool keep = inOuter && !inInner && (dark == upperLeft);
			if (keep)
			{
				if (!run)
				{
					x0 = cx + dx;
					run = true;
				}
			}
			else if (run)
			{
				fill(x0, cy + dy, cx + dx, cy + dy + 1);
				run = false;
			}
		}
		if (run)
			fill(x0, cy + dy, cx + rOuter + 1, cy + dy + 1);
	}
}

void FillTriDown(int x, int y, int s, FillFn fill)
{
	if (!fill)
		return;
	const int pad = ClampPad(s, 5);
	const int top = y + pad;
	const int bot = y + s - pad - 1;
	const int mid = x + s / 2;
	const int half = (s - 2 * pad) / 2;
	const int h = bot - top;
	if (h <= 0 || half <= 0)
		return;
	for (int row = top; row <= bot; ++row)
	{
		const int w = half * (bot - row) / h;
		fill(mid - w, row, mid + w + 1, row + 1);
	}
}

void FillTriUp(int x, int y, int s, FillFn fill)
{
	if (!fill)
		return;
	const int pad = ClampPad(s, 5);
	const int top = y + pad;
	const int bot = y + s - pad - 1;
	const int mid = x + s / 2;
	const int half = (s - 2 * pad) / 2;
	const int h = bot - top;
	if (h <= 0 || half <= 0)
		return;
	for (int row = top; row <= bot; ++row)
	{
		const int w = half * (row - top) / h;
		fill(mid - w, row, mid + w + 1, row + 1);
	}
}

void FillTriRight(int x, int y, int s, FillFn fill)
{
	if (!fill)
		return;
	const int pad = ClampPad(s, 5);
	const int left = x + pad;
	const int right = x + s - pad - 1;
	const int mid = y + s / 2;
	const int half = (s - 2 * pad) / 2;
	const int w = right - left;
	if (w <= 0 || half <= 0)
		return;
	for (int col = left; col <= right; ++col)
	{
		const int h = half * (right - col) / w;
		fill(col, mid - h, col + 1, mid + h + 1);
	}
}

void FillTriLeft(int x, int y, int s, FillFn fill)
{
	if (!fill)
		return;
	const int pad = ClampPad(s, 5);
	const int left = x + pad;
	const int right = x + s - pad - 1;
	const int mid = y + s / 2;
	const int half = (s - 2 * pad) / 2;
	const int w = right - left;
	if (w <= 0 || half <= 0)
		return;
	for (int col = left; col <= right; ++col)
	{
		const int h = half * (col - left) / w;
		fill(col, mid - h, col + 1, mid + h + 1);
	}
}

void Cross(int x, int y, int s, LineFn line)
{
	if (!line)
		return;
	// 3px-X, zentriert in der Zelle — Titel-Close muss gegen Glass lesbar bleiben.
	const int inset = s >= 12 ? 3 : 2;
	const int x0 = x + inset;
	const int y0 = y + inset;
	const int x1 = x + s - inset - 1;
	const int y1 = y + s - inset - 1;
	for (int o = -1; o <= 1; ++o)
	{
		line(x0 + o, y0, x1 + o, y1);
		line(x1 + o, y0, x0 + o, y1);
	}
}

void CheckMark(int x, int y, int s, LineFn line)
{
	if (!line)
		return;
	// Haken in der inneren Box, 2px Strich, nicht oben links versetzt.
	const int inset = ClampPad(s, 6);
	const int x0 = x + inset;
	const int y0 = y + s / 2;
	const int x1 = x + s / 2;
	const int y1 = y + s - inset - 1;
	const int x2 = x + s - inset;
	const int y2 = y + inset + 1;
	line(x0, y0, x1, y1);
	line(x0, y0 + 1, x1, y1 + 1);
	line(x1, y1, x2, y2);
	line(x1, y1 + 1, x2, y2 + 1);
}

void GripLines(int x, int y, int s, bool shadow, LineFn line)
{
	if (!line)
		return;
	for (int inset = 4; inset <= s - 1; inset += 4)
	{
		const int offset = shadow ? 1 : 0;
		line(x + s - inset + offset, y + s - 2 + offset, x + s - 2 + offset, y + s - inset + offset);
	}
}

void RadioCenter(int x, int y, int s, int &cx, int &cy, int &r)
{
	cx = x + s / 2;
	cy = y + s / 2;
	r = s / 2 - 1;
	if (r < 3)
		r = 3;
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
	case '6':
		FillTriDown(x, y, s, fill);
		return true;
	case 't': // up
	case 'T':
		FillTriUp(x, y, s, fill);
		return true;
	case '4': // right
	case 'w': // horizontal scroll (scheme uses w; treat as left-pointing sibling of 4)
		if (ch == 'w')
			FillTriLeft(x, y, s, fill);
		else
			FillTriRight(x, y, s, fill);
		return true;
	case '3': // left
		FillTriLeft(x, y, s, fill);
		return true;
	case 'a': // menu check
	case 'b': // checkbox check
		CheckMark(x, y, s, line);
		return true;
	case 'o':
		GripLines(x, y, s, true, line);
		return true;
	case 'p':
		GripLines(x, y, s, false, line);
		return true;
	case 'g': // checkbox fill
		fill(x + 1, y + 1, x + s - 1, y + s - 1);
		return true;
	case 'e': // checkbox dark bevel
		fill(x + 1, y + 1, x + s - 1, y + 2);
		fill(x + 1, y + 1, x + 2, y + s - 1);
		return true;
	case 'f': // checkbox bright bevel
		fill(x + s - 2, y + 1, x + s - 1, y + s - 1);
		fill(x + 1, y + s - 2, x + s - 1, y + s - 1);
		return true;
	case 'n': // radio fill
	{
		int cx = 0, cy = 0, r = 0;
		RadioCenter(x, y, s, cx, cy, r);
		FillDisk(cx, cy, r - 1, fill);
		return true;
	}
	case 'j': // radio dark bevel
	case 'k': // radio bright bevel
	{
		int cx = 0, cy = 0, r = 0;
		RadioCenter(x, y, s, cx, cy, r);
		const int rInner = r > 2 ? r - 2 : 0;
		FillRingBevel(cx, cy, r, rInner, ch == 'j', fill);
		return true;
	}
	case 'h': // radio selected disk — Kreis, gleiche Mitte wie der Ring
	{
		int cx = 0, cy = 0, r = 0;
		RadioCenter(x, y, s, cx, cy, r);
		int dot = r / 3;
		if (dot < 2)
			dot = 2;
		FillDisk(cx, cy, dot, fill);
		return true;
	}
	default:
		// Unknown Marlett codepoint: still consume as empty advance (never fall to Latin TTF).
		return true;
	}
}
} // namespace CsretroVguiSymbols

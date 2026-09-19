#pragma once

#include <Color.h>
#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>

#include <algorithm>
#include <cmath>

// CS Retro in-game UI chrome. Buy, Team, Class and Radio share this palette.
// The toolkit underneath is an implementation detail, not the product name.
namespace InGameUi
{
inline Color Overlay() { return Color(0, 0, 0, 208); }
inline Color OverlayBg() { return Overlay(); }
inline Color Card() { return Color(18, 19, 22, 236); }
inline Color CardArmed() { return Color(36, 38, 42, 248); }
inline Color Text() { return Color(240, 240, 240, 255); }
inline Color TextDim() { return Color(210, 210, 212, 255); }
inline Color Terror() { return Color(232, 196, 52, 255); }
inline Color CT() { return Color(90, 170, 230, 255); }
inline Color Gold() { return Color(232, 196, 52, 255); }
inline Color BuyGold() { return Gold(); }
inline Color BuyCell() { return Color(28, 30, 34, 250); }
inline Color BuyCellArmed() { return Color(72, 76, 84, 255); }
inline Color BuyCellDim() { return Color(22, 23, 26, 250); }
inline Color BuyPlate() { return Color(10, 10, 12, 0); }
inline Color BuyOverlay() { return Color(0, 0, 0, 208); }
inline Color BuyColumn(int index)
{
	const int lift = (index % 2) ? 0 : 10;
	return Color(14 + lift, 15 + lift, 18 + lift, 158);
}

// In-game UI grows up to a comfortable 1440x810 workspace, then stays centered.
// This is deliberately not a fixed 16:9 letterbox: 4:3 and ultrawide keep all
// available height, while large desktop resolutions no longer inflate controls.
inline void ContentCanvas(int viewW, int viewH, int &x, int &y, int &w, int &h)
{
	const int marginX = std::max(8, viewW * 3 / 100);
	const int marginY = std::max(8, viewH * 3 / 100);
	w = std::max(320, std::min(1440, viewW - marginX * 2));
	h = std::max(260, std::min(810, viewH - marginY * 2));
	w = std::min(w, viewW);
	h = std::min(h, viewH);
	x = (viewW - w) / 2;
	y = (viewH - h) / 2;
}

inline void StyleCardButton(vgui2::Button *btn, Color accent)
{
	if (!btn)
		return;
	btn->SetPaintBackgroundEnabled(true);
	btn->SetPaintBorderEnabled(false);
	btn->SetBorder(nullptr);
	btn->SetDefaultColor(accent, Card());
	btn->SetArmedColor(Text(), CardArmed());
	btn->SetDepressedColor(Text(), CardArmed());
	btn->SetDefaultBorder(nullptr);
	btn->SetDepressedBorder(nullptr);
	btn->SetKeyFocusBorder(nullptr);
	btn->SetButtonActivationType(vgui2::Button::ACTIVATE_ONPRESSED);
}

inline void StyleFooterButton(vgui2::Button *btn, Color accent)
{
	if (!btn)
		return;
	btn->SetPaintBackgroundEnabled(false);
	btn->SetPaintBorderEnabled(false);
	btn->SetBorder(nullptr);
	btn->SetDefaultColor(accent, Color(0, 0, 0, 0));
	btn->SetArmedColor(Text(), Color(0, 0, 0, 0));
	btn->SetDepressedColor(Text(), Color(0, 0, 0, 0));
	btn->SetDefaultBorder(nullptr);
	btn->SetDepressedBorder(nullptr);
	btn->SetKeyFocusBorder(nullptr);
	btn->SetContentAlignment(vgui2::Label::a_center);
	btn->SetButtonActivationType(vgui2::Button::ACTIVATE_ONPRESSED);
	btn->SetTextInset(8, 0);
}

inline void StyleTitle(vgui2::Label *lab)
{
	if (!lab)
		return;
	lab->SetTextColorState(vgui2::Label::CS_NORMAL);
	lab->SetFgColor(Text());
	lab->SetPaintBackgroundEnabled(false);
}

// VGUI1's Xash surface does not implement textured polygons, so rounded UI
// geometry is built from a center rectangle and a handful of horizontal edge
// spans.  With a 4-8 px radius this stays cheaper than introducing bitmap
// corners and scales cleanly at every viewport size.
inline void PaintRoundedRect(int x0, int y0, int x1, int y1, int radius, Color color)
{
	if (!vgui2::surface() || x1 <= x0 || y1 <= y0)
		return;
	const int w = x1 - x0;
	const int h = y1 - y0;
	radius = std::max(0, std::min(radius, std::min(w, h) / 2));
	vgui2::surface()->DrawSetColor(color);
	if (radius == 0)
	{
		vgui2::surface()->DrawFilledRect(x0, y0, x1, y1);
		return;
	}
	vgui2::surface()->DrawFilledRect(x0, y0 + radius, x1, y1 - radius);
	vgui2::surface()->DrawFilledRect(x0 + radius, y0, x1 - radius, y1);
	for (int row = 0; row < radius; ++row)
	{
		const float dy = static_cast<float>(radius - row) - 0.5f;
		const int span = static_cast<int>(std::sqrt(
			std::max(0.0f, static_cast<float>(radius * radius) - dy * dy)));
		const int inset = std::max(0, radius - span);
		vgui2::surface()->DrawFilledRect(x0 + inset, y0 + row, x1 - inset, y0 + row + 1);
		vgui2::surface()->DrawFilledRect(x0 + inset, y1 - row - 1, x1 - inset, y1 - row);
	}
}

inline void PaintOverlay(int w, int h)
{
	if (!vgui2::surface() || w < 1 || h < 1)
		return;
	const Color veil = Overlay();
	vgui2::surface()->DrawSetColor(veil.r(), veil.g(), veil.b(), veil.a());
	vgui2::surface()->DrawFilledRect(0, 0, w, h);
}

// Team/Class: map stays readable. Buy keeps PaintOverlay.
inline void PaintSelectVeil(int w, int h)
{
	if (!vgui2::surface() || w < 1 || h < 1)
		return;
	vgui2::surface()->DrawSetColor(0, 0, 0, 82);
	vgui2::surface()->DrawFilledRect(0, 0, w, h);
}

inline void PaintBuyPlate(int w, int h)
{
	PaintRoundedRect(0, 0, w, h, std::min(8, h / 8), BuyPlate());
}

inline void PaintBuyHeader(int w, int h, Color accent = BuyGold())
{
	if (!vgui2::surface() || w < 2 || h < 2)
		return;
	const int y = std::max(0, h - 2);
	vgui2::surface()->DrawSetColor(accent.r(), accent.g(), accent.b(), 210);
	vgui2::surface()->DrawFilledRect(0, y, w, h);
}

inline void PaintBuyColumn(int w, int h, int index)
{
	if (!vgui2::surface() || w < 2 || h < 2)
		return;
	PaintRoundedRect(0, 0, w, h, 4, BuyColumn(index));
}

inline void PaintBuyCell(int w, int h, bool armed, bool dim = false)
{
	if (!vgui2::surface() || w < 2 || h < 2)
		return;
	PaintRoundedRect(0, 0, w, h, 3,
		armed ? BuyCellArmed() : (dim ? BuyCellDim() : BuyCell()));
}

inline void PaintBuyFooter(int w, int h, Color accent, bool armed)
{
	if (!vgui2::surface() || w < 2 || h < 2)
		return;
	PaintRoundedRect(0, 0, w, h, 3, armed ? Color(36, 38, 42, 230) : Color(18, 19, 22, 210));
	vgui2::surface()->DrawSetColor(accent.r(), accent.g(), accent.b(), armed ? 255 : 220);
	vgui2::surface()->DrawFilledRect(6, h - 3, w - 6, h - 1);
}

inline void DrawFilledTriangle(int x0, int y0, int x1, int y1, int x2, int y2, Color color);

inline void PaintCardBackground(int w, int h, Color accent, bool armed)
{
	if (!vgui2::surface() || w < 2 || h < 2)
		return;
	PaintRoundedRect(0, 0, w, h, 3, armed ? CardArmed() : Card());
	vgui2::surface()->DrawSetColor(accent.r(), accent.g(), accent.b(), armed ? 230 : 170);
	vgui2::surface()->DrawFilledRect(0, 0, 3, h);
	vgui2::surface()->DrawFilledRect(6, h - 3, w - 6, h - 1);
}

inline void TeamStage(int viewW, int viewH, int &x, int &y, int &w, int &h)
{
	w = std::min(viewW, viewH * 16 / 9);
	h = viewH;
	x = (viewW - w) / 2;
	y = 0;
}

// Charakter-Slot: ganze Teamspalte unter den Titeln.
inline void TeamCharacterSlot(int sideW, int sideH, int &x, int &y, int &w, int &h)
{
	x = 0;
	y = sideH * 18 / 100;
	w = sideW;
	h = std::max(32, sideH - y);
}

inline void TeamEmblemMetrics(int sideW, int sideH, int &cx, int &cy, int &radius)
{
	int sx = 0, sy = 0, sw = 0, sh = 0;
	TeamCharacterSlot(sideW, sideH, sx, sy, sw, sh);
	cx = sideW / 2;
	cy = sideH * 56 / 100;
	radius = std::max(32, std::min(sideW * 44 / 100, sideH * 24 / 100));
}

// Quadrat um das Emblem, nicht die ganze Spalte.
inline void TeamModelViewport(int sideW, int sideH, int &x, int &y, int &w, int &h)
{
	int cx = 0, cy = 0, radius = 0;
	TeamEmblemMetrics(sideW, sideH, cx, cy, radius);
	w = sideW;
	h = sideH * 64 / 100;
	x = cx - w / 2;
	y = cy - h / 2;
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (x + w > sideW)
		x = std::max(0, sideW - w);
	if (y + h > sideH)
		y = std::max(0, sideH - h);
}

inline bool PointInTriangle(float px, float py,
	float ax, float ay, float bx, float by, float cx, float cy)
{
	const float d0 = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
	const float d1 = (cx - bx) * (py - by) - (cy - by) * (px - bx);
	const float d2 = (ax - cx) * (py - cy) - (ay - cy) * (px - cx);
	const bool hasNeg = (d0 < 0.0f) || (d1 < 0.0f) || (d2 < 0.0f);
	const bool hasPos = (d0 > 0.0f) || (d1 > 0.0f) || (d2 > 0.0f);
	return !(hasNeg && hasPos);
}

inline void DrawFilledTriangle(int x0, int y0, int x1, int y1, int x2, int y2, Color color)
{
	auto *surf = vgui2::surface();
	if (!surf)
		return;
	const int minX = std::min(x0, std::min(x1, x2));
	const int maxX = std::max(x0, std::max(x1, x2));
	const int minY = std::min(y0, std::min(y1, y2));
	const int maxY = std::max(y0, std::max(y1, y2));
	const float ax = static_cast<float>(x0);
	const float ay = static_cast<float>(y0);
	const float bx = static_cast<float>(x1);
	const float by = static_cast<float>(y1);
	const float cx = static_cast<float>(x2);
	const float cy = static_cast<float>(y2);
	for (int y = minY; y <= maxY; ++y)
	{
		int runStart = -1;
		int runCover = -1;
		auto flush = [&](int xEnd) {
			if (runStart < 0)
				return;
			surf->DrawSetColor(color.r(), color.g(), color.b(), (color.a() * runCover + 2) / 4);
			surf->DrawFilledRect(runStart, y, xEnd, y + 1);
			runStart = -1;
			runCover = -1;
		};
		for (int x = minX; x <= maxX; ++x)
		{
			int cover = 0;
			for (int sy = 0; sy < 2; ++sy)
			{
				for (int sx = 0; sx < 2; ++sx)
				{
					if (PointInTriangle(static_cast<float>(x) + (static_cast<float>(sx) + 0.5f) * 0.5f,
						static_cast<float>(y) + (static_cast<float>(sy) + 0.5f) * 0.5f,
						ax, ay, bx, by, cx, cy))
						++cover;
				}
			}
			if (cover == 0)
			{
				flush(x);
				continue;
			}
			if (runStart >= 0 && cover == runCover)
				continue;
			flush(x);
			runStart = x;
			runCover = cover;
		}
		flush(maxX + 1);
	}
}

inline void PaintTeamGlyph(int team, int cx, int cy, int radius, Color accent, bool armed)
{
	const int a = armed ? 220 : 155;
	if (team == 1)
	{
		// Broad angular phoenix mark: deliberately native to CS Retro, while
		// retaining the large, dark-on-gold hierarchy of the reference.
		const Color ink(24, 20, 14, a);
		const int r = radius * 62 / 100;
		const int core = radius * 18 / 100;
		DrawFilledTriangle(cx, cy - r, cx - core, cy + core, cx + core, cy + core, ink);
		DrawFilledTriangle(cx - r, cy - r / 3, cx - core, cy - core, cx - core, cy + core, ink);
		DrawFilledTriangle(cx + r, cy - r / 3, cx + core, cy - core, cx + core, cy + core, ink);
		DrawFilledTriangle(cx - r * 4 / 5, cy + r / 2, cx - core, cy, cx, cy + core, ink);
		DrawFilledTriangle(cx + r * 4 / 5, cy + r / 2, cx + core, cy, cx, cy + core, ink);
		DrawFilledTriangle(cx, cy + r, cx - core, cy, cx + core, cy, ink);
		return;
	}
	if (team == 2)
	{
		// Filled mirrored wings around a shield read at low resolution too.
		const Color ink(accent.r(), accent.g(), accent.b(), a);
		const int r = radius * 68 / 100;
		for (int i = 0; i < 3; ++i)
		{
			const int y = cy - r / 2 + i * r / 3;
			const int inner = r / 7 + i * r / 18;
			const int outer = r * (9 - i) / 10;
			DrawFilledTriangle(cx - inner, y, cx - outer, y - r / 5, cx - inner, y + r / 4, ink);
			DrawFilledTriangle(cx + inner, y, cx + outer, y - r / 5, cx + inner, y + r / 4, ink);
		}
		DrawFilledTriangle(cx, cy - r * 3 / 5, cx - r / 4, cy, cx, cy + r * 3 / 5, ink);
		DrawFilledTriangle(cx, cy - r * 3 / 5, cx + r / 4, cy, cx, cy + r * 3 / 5, ink);
	}
}

inline void PaintEmblem(int team, int cx, int cy, int radius, Color accent, bool armed, int ringWidth = 2)
{
	auto *surf = vgui2::surface();
	if (!surf || radius < 12)
		return;
	const int fillA = armed ? 140 : 112;
	const int ringA = armed ? 230 : 180;
	for (int y = -radius; y <= radius; ++y)
	{
		const int span = static_cast<int>(std::sqrt(static_cast<double>(radius * radius - y * y)));
		surf->DrawSetColor(7, 9, 12, armed ? 225 : 205);
		surf->DrawFilledRect(cx - span, cy + y, cx + span + 1, cy + y + 1);
		if (team == 1)
		{
			surf->DrawSetColor(accent.r(), accent.g(), accent.b(), fillA);
			surf->DrawFilledRect(cx - span, cy + y, cx + span + 1, cy + y + 1);
		}
	}
	const int inner = std::max(1, radius - ringWidth);
	for (int y = -radius; y <= radius; ++y)
	{
		const int outer = static_cast<int>(std::sqrt(static_cast<double>(radius * radius - y * y)));
		int hole = 0;
		if (y >= -inner && y <= inner)
			hole = static_cast<int>(std::sqrt(static_cast<double>(inner * inner - y * y)));
		if (team == 1 && ringWidth > 2)
			surf->DrawSetColor(28, 27, 22, 235);
		else
			surf->DrawSetColor(accent.r(), accent.g(), accent.b(), ringA);
		surf->DrawFilledRect(cx - outer, cy + y, cx - hole, cy + y + 1);
		surf->DrawFilledRect(cx + hole, cy + y, cx + outer + 1, cy + y + 1);
	}
	if (ringWidth <= 2)
	{
		PaintTeamGlyph(team, cx, cy, radius, accent, armed);
		return;
	}
	// Selection badges: broad wings for T, a feathered wreath for CT.
	// Native geometry, not a stretched star or an imported reference bitmap.
	const Color ink = team == 1 ? Color(28, 26, 20, 230) : Color(accent.r(), accent.g(), accent.b(), 200);
	const int r = radius * 78 / 100;
	for (int side : {-1, 1})
	{
		if (team == 1)
		{
			DrawFilledTriangle(cx, cy + r / 3, cx + side * r, cy - r / 4,
				cx + side * r * 3 / 4, cy + r / 3, ink);
			for (int i = 0; i < 5; ++i)
				DrawFilledTriangle(cx + side * r / 5, cy + r / 3,
					cx + side * r * (8 - i) / 10, cy + r * (3 + i) / 10,
					cx + side * r / 5, cy + r * 3 / 4, ink);
		}
		else
		{
			for (int i = 0; i < 10; ++i)
			{
				const float angle = (-65.0f + i * 13.0f) * 3.14159265f / 180.0f;
				const int x = cx + side * static_cast<int>(std::cos(angle) * r * .65f);
				const int y = cy + static_cast<int>(std::sin(angle) * r);
				DrawFilledTriangle(x, y, x + side * r / 4, y - r / 5,
					x + side * r / 8, y + r / 6, ink);
			}
		}
	}
	if (team == 1)
	{
		DrawFilledTriangle(cx, cy - r / 2, cx - r / 5, cy + r / 3, cx + r / 5, cy + r / 3, ink);
		DrawFilledTriangle(cx, cy + r, cx - r / 5, cy + r / 3, cx + r / 5, cy + r / 3, ink);
	}
	else
		DrawFilledTriangle(cx, cy + r, cx - r / 4, cy + r / 2, cx + r / 4, cy + r / 2, ink);
}

inline void PaintTeamBackdrop(int w, int h, bool blurred)
{
	if (!vgui2::surface() || w < 1 || h < 1)
		return;
	vgui2::surface()->DrawSetColor(0, 0, 0, blurred ? 88 : 120);
	vgui2::surface()->DrawFilledRect(0, 0, w, h);
	const int fade = std::max(36, h * 16 / 100);
	for (int i = 0; i < fade; ++i)
	{
		const int a = (i * (blurred ? 90 : 120)) / fade;
		vgui2::surface()->DrawSetColor(0, 0, 0, a);
		vgui2::surface()->DrawFilledRect(0, h - fade + i, w, h - fade + i + 1);
	}
}

inline void PaintSplitBackdrop(int w, int h)
{
	PaintTeamBackdrop(w, h, false);
}
} // namespace InGameUi

namespace InGameViewportLook = InGameUi;

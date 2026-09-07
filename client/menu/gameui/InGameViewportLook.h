#pragma once

#include <Color.h>
#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>

#include <algorithm>
#include <cmath>

// Team/Class/Buy: eine Familie. Kein GameUI-Frame, kein Radio-Klon.
// Team-Wahl: CS:GO-Layout (Titel, Emblem, Modell, mittige Listen).
namespace InGameViewportLook
{
inline Color OverlayBg() { return Color(0, 0, 0, 40); }
inline Color Card() { return Color(16, 16, 18, 220); }
inline Color CardArmed() { return Color(36, 36, 40, 240); }
inline Color Text() { return Color(240, 240, 240, 255); }
inline Color TextDim() { return Color(190, 190, 190, 255); }
inline Color Terror() { return Color(210, 170, 70, 255); }
inline Color CT() { return Color(90, 170, 230, 255); }
inline Color BuyGold() { return Color(232, 196, 52, 255); }
inline Color BuyCell() { return Color(14, 14, 16, 185); }
inline Color BuyCellArmed() { return Color(36, 32, 18, 220); }
inline Color BuyPlate() { return Color(8, 8, 10, 205); }

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
	btn->SetContentAlignment(vgui2::Label::a_east);
	btn->SetButtonActivationType(vgui2::Button::ACTIVATE_ONPRESSED);
}

inline void StyleTitle(vgui2::Label *lab)
{
	if (!lab)
		return;
	lab->SetTextColorState(vgui2::Label::CS_NORMAL);
	lab->SetFgColor(Text());
	lab->SetPaintBackgroundEnabled(false);
}

inline void PaintBuyCell(int w, int h, bool armed)
{
	if (!vgui2::surface() || w < 2 || h < 2)
		return;
	vgui2::surface()->DrawSetColor(armed ? BuyCellArmed() : BuyCell());
	vgui2::surface()->DrawFilledRect(0, 0, w, h);
	if (armed)
	{
		vgui2::surface()->DrawSetColor(BuyGold().r(), BuyGold().g(), BuyGold().b(), 220);
		vgui2::surface()->DrawOutlinedRect(0, 0, w, h);
	}
	else
	{
		vgui2::surface()->DrawSetColor(255, 255, 255, 16);
		vgui2::surface()->DrawFilledRect(0, 0, w, 1);
	}
}

inline void PaintCardBackground(int w, int h, Color accent, bool armed)
{
	if (!vgui2::surface() || w < 2 || h < 2)
		return;
	const Color fill = armed ? CardArmed() : Card();
	vgui2::surface()->DrawSetColor(fill);
	vgui2::surface()->DrawFilledRect(0, 0, w, h);
	vgui2::surface()->DrawSetColor(accent.r(), accent.g(), accent.b(), armed ? 230 : 150);
	vgui2::surface()->DrawFilledRect(0, 0, 3, h);
	vgui2::surface()->DrawSetColor(255, 255, 255, armed ? 36 : 18);
	vgui2::surface()->DrawFilledRect(3, 0, w, 1);
	vgui2::surface()->DrawFilledRect(3, h - 1, w, h);
}

// Fast volle Fläche auf kleinen Viewports; auf großen Displays begrenzte Bühne.
inline void TeamStage(int viewW, int viewH, int &x, int &y, int &w, int &h)
{
	w = std::max(320, std::min(1440, viewW * 94 / 100));
	h = std::max(260, viewH * 86 / 100);
	w = std::min(w, viewW);
	h = std::min(h, viewH);
	x = (viewW - w) / 2;
	y = viewH * 6 / 100;
}

// Charakter-Slot: ganze Teamspalte unter den Titeln.
inline void TeamCharacterSlot(int sideW, int sideH, int &x, int &y, int &w, int &h)
{
	x = 0;
	y = sideH * 16 / 100;
	w = sideW;
	h = std::max(32, sideH - y);
}

inline void TeamEmblemMetrics(int sideW, int sideH, int &cx, int &cy, int &radius)
{
	int sx = 0, sy = 0, sw = 0, sh = 0;
	TeamCharacterSlot(sideW, sideH, sx, sy, sw, sh);
	cx = sx + sw / 2;
	cy = sy + sh * 48 / 100;
	radius = std::max(44, std::min(sw, sh) * 36 / 100);
}

// Ganzkörper vor dem Kreis, nicht als Avatar im Ring.
inline void TeamModelViewport(int sideW, int sideH, int &x, int &y, int &w, int &h)
{
	x = 0;
	y = sideH * 12 / 100;
	w = sideW;
	h = std::max(64, sideH - y);
}

inline void DrawFilledTriangle(int x0, int y0, int x1, int y1, int x2, int y2, Color color)
{
	auto *surf = vgui2::surface();
	if (!surf)
		return;
	const int minY = std::min(y0, std::min(y1, y2));
	const int maxY = std::max(y0, std::max(y1, y2));
	surf->DrawSetColor(color);
	for (int y = minY; y <= maxY; ++y)
	{
		int hits[3] = {};
		int count = 0;
		auto hit = [&](int ax, int ay, int bx, int by) {
			if (ay == by || y < std::min(ay, by) || y > std::max(ay, by) || count >= 3)
				return;
			hits[count++] = ax + (bx - ax) * (y - ay) / (by - ay);
		};
		hit(x0, y0, x1, y1);
		hit(x1, y1, x2, y2);
		hit(x2, y2, x0, y0);
		if (count >= 2)
		{
			if (hits[0] > hits[1])
				std::swap(hits[0], hits[1]);
			surf->DrawFilledRect(hits[0], y, hits[1] + 1, y + 1);
		}
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

inline void PaintEmblem(int team, int cx, int cy, int radius, Color accent, bool armed)
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
	const int inner = std::max(1, radius - 2);
	for (int y = -radius; y <= radius; ++y)
	{
		const int outer = static_cast<int>(std::sqrt(static_cast<double>(radius * radius - y * y)));
		int hole = 0;
		if (y >= -inner && y <= inner)
			hole = static_cast<int>(std::sqrt(static_cast<double>(inner * inner - y * y)));
		surf->DrawSetColor(accent.r(), accent.g(), accent.b(), ringA);
		surf->DrawFilledRect(cx - outer, cy + y, cx - hole, cy + y + 1);
		surf->DrawFilledRect(cx + hole, cy + y, cx + outer + 1, cy + y + 1);
	}
	PaintTeamGlyph(team, cx, cy, radius, accent, armed);
}

inline void PaintTeamBackdrop(int w, int h, bool blurred)
{
	if (!vgui2::surface() || w < 1 || h < 1)
		return;
	vgui2::surface()->DrawSetColor(0, 0, 0, blurred ? 28 : 80);
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
} // namespace InGameViewportLook

#pragma once

#include <Color.h>
#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Label.h>

// Spectator/Scoreboard: eigene Familie. Kein Team-Viewport, kein Buy-Raster.
// Anreiz: dunkler Rahmen, Daten am Rand — kein Faceit-1:1.
namespace HudFrameLook
{
inline Color Bar() { return Color(8, 8, 10, 220); }
inline Color Card() { return Color(12, 12, 14, 190); }
inline Color Edge() { return Color(8, 8, 10, 200); }
inline Color Text() { return Color(240, 240, 240, 255); }
inline Color TextDim() { return Color(180, 180, 180, 255); }
inline Color Terror() { return Color(210, 170, 70, 255); }
inline Color CT() { return Color(90, 170, 230, 255); }
inline Color Dead() { return Color(140, 140, 140, 255); }

inline void ViewportSize(int &w, int &h, vgui2::Panel *parent)
{
	w = h = 0;
	if (vgui2::surface())
		vgui2::surface()->GetScreenSize(w, h);
	if ((w < 1 || h < 1) && parent)
		parent->GetSize(w, h);
	if (w < 1)
		w = 640;
	if (h < 1)
		h = 480;
}

inline void StyleHudLabel(vgui2::Label *lab, Color fg)
{
	if (!lab)
		return;
	lab->SetTextColorState(vgui2::Label::CS_NORMAL);
	lab->SetFgColor(fg);
	lab->SetPaintBackgroundEnabled(false);
}

inline void PaintBroadcastFrame(int w, int h, int topH, int botH, int edge)
{
	if (!vgui2::surface() || w < 1 || h < 1)
		return;
	if (edge < 4)
		edge = 4;
	vgui2::surface()->DrawSetColor(Edge());
	vgui2::surface()->DrawFilledRect(0, 0, edge, h);
	vgui2::surface()->DrawFilledRect(w - edge, 0, w, h);
	(void)topH;
	(void)botH;
}

inline void PaintTeamAccent(int w, int h)
{
	if (!vgui2::surface() || w < 2 || h < 2)
		return;
	vgui2::surface()->DrawSetColor(Terror());
	vgui2::surface()->DrawFilledRect(0, h - 2, w / 2, h);
	vgui2::surface()->DrawSetColor(CT());
	vgui2::surface()->DrawFilledRect(w / 2, h - 2, w, h);
}
} // namespace HudFrameLook

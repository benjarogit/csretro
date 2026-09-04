#pragma once

#include <Color.h>
#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>

// Team/Class/Buy: eine Familie. Kein GameUI-Frame, kein Radio-Klon.
// Anreiz: dunkle Karten, T/CT-Farbe, Welt durchscheinend — kein CS2-1:1.
namespace InGameViewportLook
{
inline Color OverlayBg() { return Color(0, 0, 0, 120); }
inline Color Card() { return Color(16, 16, 18, 220); }
inline Color CardArmed() { return Color(36, 36, 40, 240); }
inline Color Text() { return Color(240, 240, 240, 255); }
inline Color TextDim() { return Color(190, 190, 190, 255); }
inline Color Terror() { return Color(210, 170, 70, 255); }
inline Color CT() { return Color(90, 170, 230, 255); }

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

inline void StyleTitle(vgui2::Label *lab)
{
	if (!lab)
		return;
	lab->SetTextColorState(vgui2::Label::CS_NORMAL);
	lab->SetFgColor(Text());
	lab->SetPaintBackgroundEnabled(false);
}

inline void PaintSplitBackdrop(int w, int h)
{
	if (!vgui2::surface() || w < 1 || h < 1)
		return;
	vgui2::surface()->DrawSetColor(0, 0, 0, 120);
	vgui2::surface()->DrawFilledRect(0, 0, w, h);
	vgui2::surface()->DrawSetColor(210, 170, 70, 48);
	vgui2::surface()->DrawFilledRect(0, 0, w / 2, h);
	vgui2::surface()->DrawSetColor(90, 170, 230, 48);
	vgui2::surface()->DrawFilledRect(w / 2, 0, w, h);
}
} // namespace InGameViewportLook

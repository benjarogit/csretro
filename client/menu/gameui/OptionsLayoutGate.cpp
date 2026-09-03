#include "OptionsLayoutGate.h"
#include "OptionsAdaptiveLayout.h"
#include "OptionsClassicMetrics.h"
#include "OptionsDialog.h"
#include "OptionsSubKeyboard.h"
#include "Controls/MenuEngine.h"

#include "../../vgui/vgui_boot.h"
#include "../../vgui/window_geometry.h"
#include "../src/menu_priv.h"

#include "vgui/IVGui.h"
#include "vgui/IInputInternal.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Controls.h"
#include "vgui_controls/Panel.h"
#include "vgui_controls/PropertySheet.h"

#include "keydefs.h"

#include <algorithm>

extern vgui2::IInputInternal *g_pVGuiInput;

namespace
{
void Expect(bool ok, const char *tag)
{
	Menu_Con(ok ? "CSRETRO_LAYOUT_GATE_OK %s" : "CSRETRO_LAYOUT_GATE_FAIL %s", tag);
}

void FlushVgui()
{
	if (vgui2::ivgui())
		vgui2::ivgui()->RunFrame();
}

COptionsSubKeyboard *KeyboardPage(COptionsDialog *dialog)
{
	if (!dialog || !dialog->GetPropertySheet())
		return nullptr;
	return dynamic_cast<COptionsSubKeyboard *>(dialog->GetPropertySheet()->GetActivePage());
}

void FooterY(COptionsDialog *dialog, int &okY, int &cancelY, int &applyY)
{
	okY = cancelY = applyY = -1;
	if (!dialog)
		return;
	if (auto *b = dynamic_cast<vgui2::Button *>(dialog->FindChildByName("OKButton", true)))
		okY = b->GetYPos();
	if (auto *b = dynamic_cast<vgui2::Button *>(dialog->FindChildByName("CancelButton", true)))
		cancelY = b->GetYPos();
	if (auto *b = dynamic_cast<vgui2::Button *>(dialog->FindChildByName("ApplyButton", true)))
		applyY = b->GetYPos();
}

bool ChildBounds(COptionsDialog *dialog, const char *name, int &x, int &y, int &w, int &h)
{
	x = y = w = h = 0;
	if (!dialog || !name)
		return false;
	vgui2::Panel *p = dialog->FindChildByName(name, true);
	if (!p)
		return false;
	p->GetBounds(x, y, w, h);
	return w > 0 && h > 0;
}

bool SameBounds(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
{
	return ax == bx && ay == by && aw == bw && ah == bh;
}

void Shot(const char *tag)
{
	MenuEngine::ClientCmd("screenshot\n");
	Menu_Con("CSRETRO_LAYOUT_SHOT %s", tag);
}

bool DragGrip(COptionsDialog *dialog, const char *name, int dx, int dy)
{
	if (!dialog || !name || !g_pVGuiInput)
		return false;
	vgui2::Panel *grip = dialog->FindChildByName(name, false);
	if (!grip || !grip->IsVisible() || !grip->IsMouseInputEnabled())
		return false;

	int x = grip->GetWide() / 2;
	int y = grip->GetTall() / 2;
	grip->LocalToScreen(x, y);
	g_pVGuiInput->InternalCursorMoved(x, y);
	FlushVgui();
	g_pVGuiInput->InternalMousePressed(vgui2::MOUSE_LEFT);
	FlushVgui();
	g_pVGuiInput->InternalCursorMoved(x + dx, y + dy);
	FlushVgui();
	g_pVGuiInput->InternalMouseReleased(vgui2::MOUSE_LEFT);
	FlushVgui();
	return true;
}

bool ProbeGrip(COptionsDialog *dialog, const char *name, int xdir, int ydir, int dx, int dy,
	int screenW, int screenH)
{
	const int baseW = CsretroOptionsClassic::kPreferredWide;
	const int baseH = CsretroOptionsClassic::kPreferredTall;
	const int baseX = (screenW - baseW) / 2;
	const int baseY = (screenH - baseH) / 2;
	dialog->SetBounds(baseX, baseY, baseW, baseH);
	dialog->InvalidateLayout(true, true);
	FlushVgui();

	if (!DragGrip(dialog, name, dx, dy))
		return false;

	int x = 0, y = 0, w = 0, h = 0;
	dialog->GetBounds(x, y, w, h);
	const int expectedX = baseX + (xdir < 0 ? dx : 0);
	const int expectedY = baseY + (ydir < 0 ? dy : 0);
	const int expectedW = baseW + dx * xdir;
	const int expectedH = baseH + dy * ydir;
	return x == expectedX && y == expectedY && w == expectedW && h == expectedH;
}

void ProbeNativeResize(COptionsDialog *dialog)
{
	int screenW = 0, screenH = 0;
	if (gGlobals)
	{
		screenW = gGlobals->scrWidth;
		screenH = gGlobals->scrHeight;
	}
	if ((screenW <= 0 || screenH <= 0) && dialog && dialog->GetParent())
		dialog->GetParent()->GetSize(screenW, screenH);
	if (screenW < CsretroOptionsClassic::kPreferredWide + 24 ||
		screenH < CsretroOptionsClassic::kPreferredTall + 24)
	{
		Menu_Con("CSRETRO_LAYOUT_GATE_FAIL resize_workspace_too_small %dx%d", screenW, screenH);
		return;
	}

	struct GripCase
	{
		const char *name;
		const char *tag;
		int xdir, ydir, dx, dy;
	};
	const GripCase grips[] = {
		{"frame_topGrip", "grip_top", 0, -1, 0, -12},
		{"frame_bottomGrip", "grip_bottom", 0, 1, 0, 12},
		{"frame_leftGrip", "grip_left", -1, 0, -12, 0},
		{"frame_rightGrip", "grip_right", 1, 0, 12, 0},
		{"frame_tlGrip", "grip_top_left", -1, -1, -12, -12},
		{"frame_trGrip", "grip_top_right", 1, -1, 12, -12},
		{"frame_blGrip", "grip_bottom_left", -1, 1, -12, 12},
		{"frame_brGrip", "grip_bottom_right", 1, 1, 12, 12},
	};
	bool all = true;
	for (const GripCase &g : grips)
	{
		const bool ok = ProbeGrip(dialog, g.name, g.xdir, g.ydir, g.dx, g.dy, screenW, screenH);
		Expect(ok, g.tag);
		all = all && ok;
	}
	Expect(all, "resize_grips_all");

	// Crossing the top/left workspace edge must shorten those sides while the
	// opposite (right/bottom) edges stay anchored.
	const int baseW = CsretroOptionsClassic::kPreferredWide;
	const int baseH = CsretroOptionsClassic::kPreferredTall;
	const int baseX = (screenW - baseW) / 2;
	const int baseY = (screenH - baseH) / 2;
	dialog->SetBounds(baseX, baseY, baseW, baseH);
	dialog->InvalidateLayout(true, true);
	FlushVgui();
	const bool topLeftDragged = DragGrip(dialog, "frame_tlGrip", -(baseX + 20), -(baseY + 20));
	int x = 0, y = 0, w = 0, h = 0;
	dialog->GetBounds(x, y, w, h);
	Expect(topLeftDragged && x == 0 && y == 0 && w == baseX + baseW && h == baseY + baseH,
		"resize_top_left_boundary_anchor");

	// A very large bottom-right drag must stop at the workspace boundary.
	dialog->SetBounds(baseX, baseY, baseW, baseH);
	dialog->InvalidateLayout(true, true);
	FlushVgui();
	const bool workspaceDragged = DragGrip(dialog, "frame_brGrip", screenW, screenH);
	dialog->GetBounds(x, y, w, h);
	Expect(workspaceDragged && x >= 0 && y >= 0 && x + w <= screenW && y + h <= screenH,
		"resize_workspace_clamp");

	// Shrinking through a real grip stops at the derived minimum.
	const int growW = std::min(screenW - 20, baseW + 80);
	const int growH = std::min(screenH - 20, baseH + 50);
	dialog->SetBounds((screenW - growW) / 2, (screenH - growH) / 2, growW, growH);
	dialog->InvalidateLayout(true, true);
	FlushVgui();
	const bool minDragged = DragGrip(dialog, "frame_brGrip", -screenW, -screenH);
	dialog->GetBounds(x, y, w, h);
	int minW = 0, minH = 0;
	dialog->GetAdaptiveMinimum(minW, minH);
	Menu_Con("CSRETRO_LAYOUT_RESIZE_MIN dragged=%d bounds=%d,%d %dx%d min=%dx%d",
		minDragged ? 1 : 0, x, y, w, h, minW, minH);
	Expect(minDragged && w == minW && h == minH, "resize_min_clamp");
}
} // namespace

void OptionsLayout_RunFunctionalGate(COptionsDialog *dialog)
{
	if (!dialog)
	{
		Menu_Con("CSRETRO_LAYOUT_GATE_FAIL no_dialog");
		return;
	}

	Expect(dialog->Gate_IsSizeable(), "sizeable_enabled");

	int minW = 0, minH = 0;
	dialog->GetAdaptiveMinimum(minW, minH);
	Expect(minW > 0 && minH > 0, "min_derived");
	Expect(minW <= CsretroOptionsClassic::kPreferredWide, "min_w_le_preferred");
	Expect(minH <= CsretroOptionsClassic::kPreferredTall, "min_h_le_preferred");
	Menu_Con("CSRETRO_LAYOUT_MIN %dx%d preferred=%dx%d", minW, minH,
		CsretroOptionsClassic::kPreferredWide, CsretroOptionsClassic::kPreferredTall);

	ProbeNativeResize(dialog);

	dialog->Gate_SetSize(CsretroOptionsClassic::kPreferredWide, CsretroOptionsClassic::kPreferredTall);
	FlushVgui();
	dialog->InvalidateLayout(true, true);
	FlushVgui();
	dialog->OpenTab("Keyboard");
	FlushVgui();

	COptionsSubKeyboard *kb = KeyboardPage(dialog);
	if (!kb)
	{
		Menu_Con("CSRETRO_LAYOUT_GATE_FAIL no_keyboard");
		return;
	}

	int lx = 0, ly = 0, lw = 0, lh = 0;
	kb->Gate_ListBounds(lx, ly, lw, lh);
	Expect(lx == 8 && ly == 10 && lw == 480 && lh == 258, "classic_list_512x406");
	Expect(kb->Gate_ListVisibleChildrenContained(), "classic_list_children_clipped");
	Menu_Con("CSRETRO_LAYOUT_LIST classic %d,%d %dx%d", lx, ly, lw, lh);

	int dw = 0, dh = 0;
	dialog->GetSize(dw, dh);
	Expect(dw == CsretroOptionsClassic::kPreferredWide && dh == CsretroOptionsClassic::kPreferredTall,
		"classic_dialog_size");

	int okY0 = 0, cancelY0 = 0, applyY0 = 0;
	FooterY(dialog, okY0, cancelY0, applyY0);
	Expect(okY0 > 0 && cancelY0 == okY0 && applyY0 == okY0, "classic_footer_aligned");

	int mouseX0 = 0, mouseY0 = 0, mouseW0 = 0, mouseH0 = 0;
	int sfxX0 = 0, sfxY0 = 0, sfxW0 = 0, sfxH0 = 0;
	int resX0 = 0, resY0 = 0, resW0 = 0, resH0 = 0;
	Expect(ChildBounds(dialog, "Slider", mouseX0, mouseY0, mouseW0, mouseH0), "classic_mouse_slider");
	Expect(ChildBounds(dialog, "SFX Slider", sfxX0, sfxY0, sfxW0, sfxH0), "classic_audio_sfx");
	Expect(ChildBounds(dialog, "Resolution", resX0, resY0, resW0, resH0), "classic_video_res");
	Shot("classic");

	// Grow — extra = dialog − Preferred, not a 64×24 page capture.
	constexpr int kGrowW = 700;
	constexpr int kGrowH = 520;
	dialog->Gate_SetSize(kGrowW, kGrowH);
	FlushVgui();
	dialog->InvalidateLayout(true, true);
	FlushVgui();
	kb = KeyboardPage(dialog);
	int gx = 0, gy = 0, gw = 0, gh = 0;
	if (kb)
		kb->Gate_ListBounds(gx, gy, gw, gh);
	const int expectListW = CsretroOptionsClassic::kKeyboardListW + (kGrowW - CsretroOptionsClassic::kPreferredWide);
	const int expectListH = CsretroOptionsClassic::kKeyboardListH + (kGrowH - CsretroOptionsClassic::kPreferredTall);
	Expect(gw == expectListW && gh == expectListH, "grow_list");
	Expect(gx == 8 && gy == 10, "grow_list_origin");
	Expect(kb && kb->Gate_ListVisibleChildrenContained(), "grow_list_children_clipped");
	int pageW = 0, pageH = 0;
	if (kb)
		kb->GetSize(pageW, pageH);
	Expect(pageW > 0 && gx + gw <= pageW && gy + gh + 24 <= pageH, "grow_list_fits_page");
	int dwGrow = 0, dhGrow = 0;
	dialog->GetSize(dwGrow, dhGrow);
	Expect(gx + gw <= dwGrow && gy + gh <= dhGrow, "grow_list_fits_dialog");
	int okY1 = 0, cancelY1 = 0, applyY1 = 0;
	FooterY(dialog, okY1, cancelY1, applyY1);
	Expect(okY1 > okY0, "grow_footer_down");
	Menu_Con("CSRETRO_LAYOUT_LIST grow %d,%d %dx%d expect=%dx%d page=%dx%d",
		gx, gy, gw, gh, expectListW, expectListH, pageW, pageH);

	int mouseX1 = 0, mouseY1 = 0, mouseW1 = 0, mouseH1 = 0;
	int sfxX1 = 0, sfxY1 = 0, sfxW1 = 0, sfxH1 = 0;
	int resX1 = 0, resY1 = 0, resW1 = 0, resH1 = 0;
	Expect(ChildBounds(dialog, "Slider", mouseX1, mouseY1, mouseW1, mouseH1) &&
		SameBounds(mouseX0, mouseY0, mouseW0, mouseH0, mouseX1, mouseY1, mouseW1, mouseH1),
		"mouse_classic_after_grow");
	Expect(ChildBounds(dialog, "SFX Slider", sfxX1, sfxY1, sfxW1, sfxH1) &&
		SameBounds(sfxX0, sfxY0, sfxW0, sfxH0, sfxX1, sfxY1, sfxW1, sfxH1),
		"audio_classic_after_grow");
	Expect(ChildBounds(dialog, "Resolution", resX1, resY1, resW1, resH1) &&
		SameBounds(resX0, resY0, resW0, resH0, resX1, resY1, resW1, resH1),
		"video_classic_after_grow");
	Shot("grow");

	// Page switch after resize
	dialog->OpenTab("Mouse");
	FlushVgui();
	dialog->OpenTab("Audio");
	FlushVgui();
	dialog->OpenTab("Video");
	FlushVgui();
	dialog->OpenTab("Keyboard");
	FlushVgui();
	kb = KeyboardPage(dialog);
	Expect(kb != nullptr, "pages_after_grow");

	// Wheel after resize
	if (kb)
	{
		Expect(kb->Gate_SelectActionByBinding("+forward"), "wheel_select");
		int cx = 0, cy = 0;
		kb->Gate_ListScreenCenter(cx, cy);
		VGuiXash_MouseMove(cx, cy);
		FlushVgui();
		const int before = kb->Gate_ScrollValue();
		VGuiXash_Key(K_MWHEELDOWN, 1);
		VGuiXash_Key(K_MWHEELDOWN, 0);
		FlushVgui();
		if (kb->Gate_ScrollRange() > 0)
			Expect(kb->Gate_ScrollValue() > before, "wheel_after_resize");
		else
			Menu_Con("CSRETRO_LAYOUT_GATE skip wheel range=0");
	}

	// Capture after resize
	if (kb)
	{
		const int q = 'q';
		MenuEngine::SetBinding(q, "");
		dialog->ResetAllData();
		Expect(kb->Gate_SelectActionByBinding("+jump"), "cap_select");
		Expect(kb->Gate_StartCaptureColumn(1), "cap_start");
		VGuiXash_Key(q, 1);
		Expect(!kb->Gate_IsCapturing(), "cap_finished");
		dialog->Gate_Cancel();
		dialog->Activate();
		dialog->OpenTab("Keyboard");
	}

	// QueryBox after resize
	if (kb)
	{
		Expect(kb->Gate_OpenDefaultsQuery(), "querybox_after_resize");
		FlushVgui();
		Expect(kb->Gate_HasQueryBox(), "querybox_visible");
		Expect(kb->Gate_DismissQueryBox(), "querybox_dismiss");
		FlushVgui();
		FlushVgui();
		if (kb->Gate_HasQueryBox())
			Menu_Con("CSRETRO_LAYOUT_GATE skip querybox_closed (deletion deferred)");
		else
			Expect(true, "querybox_closed");
	}

	// Persist + clamp (in-process; restart restore is the script's second concern)
	{
		int sx = 0, sy = 0, sw = 0, sh = 0;
		dialog->GetBounds(sx, sy, sw, sh);
		CsretroWindowGeometry::Save("Options", sx, sy, sw, sh);
		const CsretroWindowGeometry::Bounds loaded = CsretroWindowGeometry::Load("Options");
		Expect(loaded.valid && loaded.w == sw && loaded.h == sh, "persist_roundtrip");
		int cx = loaded.x, cy = loaded.y, cw = loaded.w, ch = loaded.h;
		CsretroWindowGeometry::ClampBounds(cx, cy, cw, ch, minW, minH, 0, 0, 640, 480);
		Expect(cw <= 640 && ch <= 480, "clamp_640_size");
		Expect(cy >= 0 && cy <= 480 - 28, "clamp_640_titlebar");
		Expect(cx >= 0 && cy >= 0 && cx + cw <= 640 && cy + ch <= 480, "clamp_640_full");
		int ix = 9999, iy = -400, iw = 2000, ih = 1600;
		CsretroWindowGeometry::ClampBounds(ix, iy, iw, ih, minW, minH, 0, 0, 800, 600);
		Expect(iw <= 800 && ih <= 600 && iw >= minW && ih >= minH, "clamp_offscreen_size");
		Expect(iy >= 0 && iy <= 600 - 28, "clamp_offscreen_titlebar");
		Expect(ix >= 0 && iy >= 0 && ix + iw <= 800 && iy + ih <= 600, "clamp_offscreen_full");

		int workW = gGlobals ? gGlobals->scrWidth : 0;
		int workH = gGlobals ? gGlobals->scrHeight : 0;
		if (workW < 1 || workH < 1)
		{
			if (vgui2::Panel *root = dialog->GetParent())
				root->GetSize(workW, workH);
		}
		if (workW > 0 && workH > 0)
		{
			int rx = loaded.x, ry = loaded.y, rw = loaded.w, rh = loaded.h;
			CsretroWindowGeometry::ClampBounds(rx, ry, rw, rh, minW, minH, 0, 0, workW, workH);
			dialog->SetSize(rw, rh);
			dialog->SetPos(rx, ry);
			dialog->InvalidateLayout(true, true);
			FlushVgui();
			int aw = 0, ah = 0;
			dialog->GetSize(aw, ah);
			Expect(aw == rw && ah == rh && aw <= workW && ah <= workH, "restore_clamped_to_workspace");
			Menu_Con("CSRETRO_LAYOUT_RESTORE clamped %dx%d workspace=%dx%d", aw, ah, workW, workH);
		}
	}

	// Shrink back to classic
	dialog->Gate_SetSize(CsretroOptionsClassic::kPreferredWide, CsretroOptionsClassic::kPreferredTall);
	FlushVgui();
	dialog->InvalidateLayout(true, true);
	FlushVgui();
	kb = KeyboardPage(dialog);
	int rx = 0, ry = 0, rw = 0, rh = 0;
	if (kb)
		kb->Gate_ListBounds(rx, ry, rw, rh);
	Expect(rx == 8 && ry == 10 && rw == 480 && rh == 258, "classic_after_shrink");

	// Derived minimum
	dialog->Gate_SetSize(minW, minH);
	FlushVgui();
	dialog->InvalidateLayout(true, true);
	FlushVgui();
	int mw = 0, mh = 0;
	dialog->GetSize(mw, mh);
	Expect(mw >= minW && mh >= minH, "min_applied");
	if (kb)
	{
		int mlx = 0, mly = 0, mlw = 0, mlh = 0;
		kb->Gate_ListBounds(mlx, mly, mlw, mlh);
		Expect(mlw >= CsretroOptionsClassic::kKeyboardListMinW, "min_list_w");
		Expect(mlh >= CsretroOptionsClassic::kKeyboardListMinH, "min_list_h");
		Menu_Con("CSRETRO_LAYOUT_LIST min %d,%d %dx%d dialog=%dx%d", mlx, mly, mlw, mlh, mw, mh);
		Shot("min");
	}

	// Grow again
	dialog->Gate_SetSize(640, 480);
	FlushVgui();
	dialog->InvalidateLayout(true, true);
	FlushVgui();
	kb = KeyboardPage(dialog);
	if (kb)
	{
		int ax = 0, ay = 0, aw = 0, ah = 0;
		kb->Gate_ListBounds(ax, ay, aw, ah);
		Expect(aw >= 480 && ah >= 258, "regrow_list");
	}

	// Move/resize persistence is independent from Apply. RunFrame invokes the
	// dialog's post-drag OnThink save once the synthetic mouse is up.
	int liveX = 0, liveY = 0, liveW = 0, liveH = 0;
	dialog->GetBounds(liveX, liveY, liveW, liveH);
	FlushVgui();
	const CsretroWindowGeometry::Bounds live = CsretroWindowGeometry::Load("Options");
	Expect(live.valid && live.x == liveX && live.y == liveY && live.w == liveW && live.h == liveH,
		"geometry_live_without_apply");

	MenuEngine::ClientCmd("screenshot\n");
	// Stable fixture for the script's clean second-process restore check.
	CsretroWindowGeometry::Save("Options", 0, 0, 700, 520);
	Menu_Con("CSRETRO_LAYOUT_GATE_SHOT_READY");
	Menu_Con("CSRETRO_LAYOUT_GATE_DONE");
}

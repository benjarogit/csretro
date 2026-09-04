#include "OptionsVideoGate.h"
#include "OptionsDialog.h"
#include "OptionsSubVideo.h"
#include "OptionsMenuComboGate.h"
#include "Controls/MenuEngine.h"

#include "vgui_controls/PropertySheet.h"

#include "../src/menu_priv.h"

#include <cmath>

namespace
{
bool Near(float a, float b, float eps = 0.05f)
{
	return std::fabs(a - b) <= eps;
}

void Expect(bool ok, const char *tag)
{
	Menu_Con(ok ? "CSRETRO_VIDEO_GATE_OK %s" : "CSRETRO_VIDEO_GATE_FAIL %s", tag);
}

COptionsSubVideo *VideoPage(COptionsDialog *dialog)
{
	if (!dialog || !dialog->GetPropertySheet())
		return nullptr;
	return dynamic_cast<COptionsSubVideo *>(dialog->GetPropertySheet()->GetActivePage());
}
} // namespace

void OptionsVideo_RunFunctionalGate(COptionsDialog *dialog)
{
	if (!dialog)
	{
		Menu_Con("CSRETRO_VIDEO_GATE_FAIL no_dialog");
		return;
	}

	dialog->OpenTab("Video");
	COptionsSubVideo *video = VideoPage(dialog);
	if (!video)
	{
		Menu_Con("CSRETRO_VIDEO_GATE_FAIL no_video_page");
		return;
	}

	// Safe baseline (no resolution/display mode mutation in automated gate).
	MenuEngine::CvarSetValue("brightness", 0.5f);
	MenuEngine::CvarSetValue("gamma", 2.2f);
	MenuEngine::CvarSetValue("gl_vsync", 1.0f);
	MenuEngine::CvarSetValue("r_detailtextures", 1.0f);

	dialog->ResetAllData();
	Expect(Near(video->Gate_GetBrightnessPending(), 0.5f), "open_reads_brightness");
	Expect(Near(video->Gate_GetGammaPending(), 2.2f), "open_reads_gamma");
	Expect(video->Gate_GetVSyncPending(), "open_reads_vsync_on");
	Expect(video->Gate_GetResolutionWide() >= 640, "open_reads_resolution_w");
	Expect(video->Gate_GetResolutionTall() >= 480, "open_reads_resolution_h");
	Expect(video->Gate_GetDisplayModePending() >= 0 && video->Gate_GetDisplayModePending() <= 2,
		"open_reads_display_mode");

	video->Gate_SetBrightnessPending(1.1f);
	video->Gate_SetGammaPending(2.6f);
	video->Gate_SetVSyncPending(false);
	video->OnApplyChanges();
	Expect(Near(MenuEngine::GetCvarFloat("brightness"), 1.1f), "apply_brightness");
	Expect(Near(MenuEngine::GetCvarFloat("gamma"), 2.6f), "apply_gamma");
	Expect(MenuEngine::GetCvarFloat("gl_vsync") == 0.0f, "apply_vsync_off");
	Expect(dialog->IsVisible(), "apply_keeps_open");

	// Cancel discards pending live cvars
	MenuEngine::CvarSetValue("brightness", 0.4f);
	MenuEngine::CvarSetValue("gamma", 2.1f);
	dialog->ResetAllData();
	video->Gate_SetBrightnessPending(1.8f);
	video->Gate_SetGammaPending(2.9f);
	video->Gate_PreviewGammaBrightness();
	Expect(Near(MenuEngine::GetCvarFloat("brightness"), 1.8f), "live_preview_brightness");
	Expect(Near(MenuEngine::GetCvarFloat("gamma"), 2.9f), "live_preview_gamma");
	dialog->Gate_Cancel();
	Expect(Near(MenuEngine::GetCvarFloat("brightness"), 0.4f), "cancel_discards");
	Expect(Near(MenuEngine::GetCvarFloat("gamma"), 2.1f), "cancel_restores_gamma_preview");

	// OK applies + closes
	dialog->Activate();
	dialog->OpenTab("Video");
	video = VideoPage(dialog);
	Expect(video != nullptr, "reopen_video");
	if (video)
	{
		dialog->ResetAllData();
		video->Gate_SetBrightnessPending(0.7f);
		video->Gate_SetVSyncPending(true);
		dialog->Gate_OK();
		Expect(Near(MenuEngine::GetCvarFloat("brightness"), 0.7f), "ok_applies_brightness");
		Expect(MenuEngine::GetCvarFloat("gl_vsync") != 0.0f, "ok_applies_vsync");
		Expect(!dialog->IsVisible(), "ok_closes");
	}

	dialog->Activate();
	dialog->OpenTab("Video");
	video = VideoPage(dialog);
	if (video)
	{
		dialog->ResetAllData();
		Expect(Near(video->Gate_GetBrightnessPending(), MenuEngine::GetCvarFloat("brightness")),
			"reopen_shows_applied");
		Expect(video->Gate_GetVSyncPending() == (MenuEngine::GetCvarFloat("gl_vsync") != 0.0f),
			"reset_from_runtime_vsync");
	}

	Menu_Con("CSRETRO_VIDEO_CVAR brightness=%g", MenuEngine::GetCvarFloat("brightness"));
	Menu_Con("CSRETRO_VIDEO_CVAR gamma=%g", MenuEngine::GetCvarFloat("gamma"));
	Menu_Con("CSRETRO_VIDEO_CVAR gl_vsync=%g", MenuEngine::GetCvarFloat("gl_vsync"));
	Menu_Con("CSRETRO_VIDEO_CVAR width=%g", MenuEngine::GetCvarFloat("width"));
	Menu_Con("CSRETRO_VIDEO_CVAR height=%g", MenuEngine::GetCvarFloat("height"));
	Menu_Con("CSRETRO_VIDEO_CVAR fullscreen=%g", MenuEngine::GetCvarFloat("fullscreen"));
	Menu_Con("CSRETRO_VIDEO_CVAR r_refdll_loaded=%s", MenuEngine::GetCvarString("r_refdll_loaded"));

	// Core ComboBox/Menu VPANEL path — must not crash (blocks Video acceptance).
	OptionsMenuCombo_RunGate(dialog);

	MenuEngine::ClientCmd("host_writeconfig\n");
	Menu_Con("CSRETRO_VIDEO_GATE_WRITECONFIG");
	Menu_Con("CSRETRO_VIDEO_GATE_SHOT_READY");
	Menu_Con("CSRETRO_VIDEO_GATE_DONE");
}

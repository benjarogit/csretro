#include "OptionsVideoModeSafetyGate.h"
#include "OptionsDialog.h"
#include "OptionsSubVideo.h"
#include "OptionsMenuComboGate.h"
#include "OptionsMetricsDump.h"
#include "Controls/MenuEngine.h"

#include "vgui_controls/PropertySheet.h"

#include "../src/menu_priv.h"

#include <cstdio>
#include <cstdlib>

namespace
{
enum WallclockPhase : int
{
	kWcIdle = 0,
	kWcFsConfirmPaint,
	kWcFsAfterKeepPaint,
	kWcRestoreWindowed,
	kWcTimeoutConfirmPaint,
	kWcWaitingTimeout,
	kWcAfterTimeoutPaint,
	kWcFinishing,
	kWcDone,
};

WallclockPhase g_wcPhase = kWcIdle;
int g_wcBaseW = 0;
int g_wcBaseH = 0;
int g_wcBaseFs = 0;
int g_wcAltW = 0;
int g_wcAltH = 0;
int g_wcFrames = 0;
bool g_wcAllowFs = false;

void Expect(bool ok, const char *tag)
{
	Menu_Con(ok ? "CSRETRO_MODE_SAFETY_OK %s" : "CSRETRO_MODE_SAFETY_FAIL %s", tag);
}

bool WallclockMode()
{
	const char *v = std::getenv("CSRETRO_MODE_SAFETY_WALLCLOCK");
	return v && v[0] == '1';
}

COptionsSubVideo *VideoPage(COptionsDialog *dialog)
{
	if (!dialog || !dialog->GetPropertySheet())
		return nullptr;
	return dynamic_cast<COptionsSubVideo *>(dialog->GetPropertySheet()->GetActivePage());
}

void LogBaseline(const char *tag)
{
	Menu_Con("CSRETRO_MODE_SAFETY_BASELINE %s width=%g height=%g fullscreen=%g vid_width=%g vid_height=%g "
		"r_refdll_loaded=%s gl_vsync=%g",
		tag,
		MenuEngine::GetCvarFloat("width"),
		MenuEngine::GetCvarFloat("height"),
		MenuEngine::GetCvarFloat("fullscreen"),
		MenuEngine::GetCvarFloat("vid_width"),
		MenuEngine::GetCvarFloat("vid_height"),
		MenuEngine::GetCvarString("r_refdll_loaded"),
		MenuEngine::GetCvarFloat("gl_vsync"));
}

bool PickAlternateResolution(COptionsSubVideo *video, int baseW, int baseH, int &outW, int &outH)
{
	static const int kCandidates[][2] = {
		{800, 600}, {1024, 768}, {1280, 720}, {1366, 768}, {640, 480},
	};
	for (const auto &c : kCandidates)
	{
		if (c[0] == baseW && c[1] == baseH)
			continue;
		if (video->Gate_SelectResolution(c[0], c[1]))
		{
			outW = c[0];
			outH = c[1];
			return true;
		}
	}
	return false;
}

void AfterReinitComboSmoke(COptionsDialog *dialog)
{
	OptionsMenuCombo_RunGate(dialog);
}

void Shot(const char *tag)
{
	// Engine captures the *next* rendered frame — caller must wait several frames first.
	// Named path so the shell gate can collect distinct shots (default auto-index collapses under burst).
	char cmd[192];
	std::snprintf(cmd, sizeof(cmd), "screenshot scrshots/mode_safety_%s.png\n", tag);
	MenuEngine::ClientCmd(cmd);
	Menu_Con("CSRETRO_MODE_SAFETY_SHOT tag=%s path=scrshots/mode_safety_%s.png", tag, tag);
}

void FinishAndQuit()
{
	Menu_Con("CSRETRO_MODE_SAFETY_DONE");
	MenuEngine::ClientCmd("quit\n");
	g_wcPhase = kWcDone;
}

void RunFastPath(COptionsDialog *dialog, COptionsSubVideo *video, int baseW, int baseH, int baseFs, int altW, int altH)
{
	// Test E — Resolution Change → Cancel
	video->Gate_SetDisplayModePending(0);
	video->Gate_SelectResolution(altW, altH);
	video->OnApplyChanges();
	Expect(video->Gate_IsConfirmOpen(), "e_confirm_open");
	video->Gate_ConfirmRevert();
	Expect(!video->Gate_IsConfirmOpen(), "e_confirm_closed");
	Expect(static_cast<int>(MenuEngine::GetCvarFloat("width")) == baseW, "e_width_restored");
	Expect(static_cast<int>(MenuEngine::GetCvarFloat("height")) == baseH, "e_height_restored");
	Expect(static_cast<int>(MenuEngine::GetCvarFloat("fullscreen")) == baseFs, "e_fullscreen_restored");
	Expect(video->Gate_GetResolutionWide() == baseW && video->Gate_GetResolutionTall() == baseH,
		"e_ui_resolution_restored");
	Expect(video->Gate_GetDisplayModePending() == baseFs, "e_ui_displaymode_restored");
	LogBaseline("after_e_cancel");
	AfterReinitComboSmoke(dialog);

	if (!WallclockMode())
	{
		// Test F — Timeout path via OnTick (deadline forced past)
		dialog->OpenTab("Video");
		video = VideoPage(dialog);
		if (!video)
		{
			Menu_Con("CSRETRO_MODE_SAFETY_FAIL reopen_after_e");
			FinishAndQuit();
			return;
		}
		dialog->ResetAllData();
		video->Gate_SetDisplayModePending(0);
		video->Gate_SelectResolution(altW, altH);
		video->OnApplyChanges();
		Expect(video->Gate_IsConfirmOpen(), "f_confirm_open");
		video->Gate_ExpireConfirmNow();
		Expect(!video->Gate_IsConfirmOpen(), "f_timeout_closed");
		Expect(static_cast<int>(MenuEngine::GetCvarFloat("width")) == baseW, "f_width_restored");
		Expect(static_cast<int>(MenuEngine::GetCvarFloat("height")) == baseH, "f_height_restored");
		Expect(static_cast<int>(MenuEngine::GetCvarFloat("fullscreen")) == baseFs, "f_fullscreen_restored");
		Expect(video->Gate_GetResolutionWide() == baseW && video->Gate_GetResolutionTall() == baseH,
			"f_ui_resolution_restored");
		LogBaseline("after_f_timeout");
		Menu_Con("CSRETRO_MODE_SAFETY_NOTE f_forced_expire (use CSRETRO_MODE_SAFETY_WALLCLOCK=1 for real 10s)");
		AfterReinitComboSmoke(dialog);
	}

	const char *allowFs = std::getenv("CSRETRO_MODE_SAFETY_ALLOW_FS");
	if (allowFs && allowFs[0] == '1' && !WallclockMode())
	{
		dialog->OpenTab("Video");
		video = VideoPage(dialog);
		if (video)
		{
			dialog->ResetAllData();
			video->Gate_SelectResolution(baseW, baseH);
			video->Gate_SetDisplayModePending(1);
			video->OnApplyChanges();
			Expect(video->Gate_IsConfirmOpen(), "a_fs_confirm_open");
			video->Gate_ConfirmKeep();
			Expect(!video->Gate_IsConfirmOpen(), "a_fs_confirm_closed");
			Expect(static_cast<int>(MenuEngine::GetCvarFloat("fullscreen")) == 1, "a_fs_applied");
			LogBaseline("after_a_fs_keep");
			AfterReinitComboSmoke(dialog);

			dialog->OpenTab("Video");
			video = VideoPage(dialog);
			if (video)
			{
				dialog->ResetAllData();
				video->Gate_SelectResolution(baseW, baseH);
				video->Gate_SetDisplayModePending(0);
				video->OnApplyChanges();
				if (video->Gate_IsConfirmOpen())
					video->Gate_ConfirmKeep();
				Expect(static_cast<int>(MenuEngine::GetCvarFloat("fullscreen")) == 0, "b_windowed_applied");
				LogBaseline("after_b_windowed_keep");
			}

			dialog->OpenTab("Video");
			video = VideoPage(dialog);
			if (video)
			{
				dialog->ResetAllData();
				video->Gate_SelectResolution(baseW, baseH);
				video->Gate_SetDisplayModePending(2);
				video->OnApplyChanges();
				Expect(video->Gate_IsConfirmOpen(), "c_borderless_confirm");
				video->Gate_ConfirmKeep();
				Expect(static_cast<int>(MenuEngine::GetCvarFloat("fullscreen")) == 2, "c_borderless_applied");
				Expect(video->Gate_GetDisplayModePending() == 2, "c_ui_borderless");
				LogBaseline("after_c_borderless_keep");
				AfterReinitComboSmoke(dialog);
			}

			dialog->OpenTab("Video");
			video = VideoPage(dialog);
			if (video)
			{
				dialog->ResetAllData();
				video->Gate_SetDisplayModePending(0);
				video->OnApplyChanges();
				if (video->Gate_IsConfirmOpen())
					video->Gate_ConfirmKeep();
				Expect(static_cast<int>(MenuEngine::GetCvarFloat("fullscreen")) == 0, "d_windowed_applied");
				LogBaseline("after_d_windowed");
			}

			dialog->OpenTab("Video");
			video = VideoPage(dialog);
			if (video)
			{
				const int chain[] = {1, 0, 2, 0};
				for (int mode : chain)
				{
					dialog->ResetAllData();
					video->Gate_SelectResolution(baseW, baseH);
					video->Gate_SetDisplayModePending(mode);
					video->OnApplyChanges();
					if (video->Gate_IsConfirmOpen())
						video->Gate_ConfirmKeep();
					Expect(static_cast<int>(MenuEngine::GetCvarFloat("fullscreen")) == mode, "g_chain_mode");
				}
				LogBaseline("after_g_chain");
				AfterReinitComboSmoke(dialog);
			}
		}
	}
	else if (!(allowFs && allowFs[0] == '1'))
	{
		Menu_Con("CSRETRO_MODE_SAFETY_SKIP fs_borderless (set CSRETRO_MODE_SAFETY_ALLOW_FS=1 on native desktop)");
	}
}

bool BeginFsConfirm(COptionsDialog *dialog)
{
	dialog->OpenTab("Video");
	COptionsSubVideo *video = VideoPage(dialog);
	if (!video)
	{
		Menu_Con("CSRETRO_MODE_SAFETY_FAIL wallclock_fs_no_video");
		return false;
	}
	dialog->ResetAllData();
	video->Gate_SelectResolution(g_wcBaseW, g_wcBaseH);
	video->Gate_SetDisplayModePending(1);
	video->OnApplyChanges();
	Expect(video->Gate_IsConfirmOpen(), "a_fs_confirm_open");
	return video->Gate_IsConfirmOpen();
}

bool BeginTimeoutConfirm(COptionsDialog *dialog)
{
	dialog->OpenTab("Video");
	COptionsSubVideo *video = VideoPage(dialog);
	if (!video)
	{
		Menu_Con("CSRETRO_MODE_SAFETY_FAIL wallclock_no_video");
		return false;
	}
	dialog->ResetAllData();
	video->Gate_SetDisplayModePending(0);
	video->Gate_SelectResolution(g_wcAltW, g_wcAltH);
	video->OnApplyChanges();
	Expect(video->Gate_IsConfirmOpen(), "f_wallclock_confirm_open");
	return video->Gate_IsConfirmOpen();
}

void CompleteWallclockTimeout(COptionsDialog *dialog)
{
	COptionsSubVideo *video = VideoPage(dialog);
	Expect(video != nullptr, "f_wallclock_video_page");
	Expect(video && !video->Gate_IsConfirmOpen(), "f_wallclock_timeout_closed");
	Expect(static_cast<int>(MenuEngine::GetCvarFloat("width")) == g_wcBaseW, "f_wallclock_width_restored");
	Expect(static_cast<int>(MenuEngine::GetCvarFloat("height")) == g_wcBaseH, "f_wallclock_height_restored");
	Expect(static_cast<int>(MenuEngine::GetCvarFloat("fullscreen")) == g_wcBaseFs, "f_wallclock_fullscreen_restored");
	if (video)
	{
		Expect(video->Gate_GetResolutionWide() == g_wcBaseW && video->Gate_GetResolutionTall() == g_wcBaseH,
			"f_wallclock_ui_resolution_restored");
		Expect(video->Gate_GetDisplayModePending() == g_wcBaseFs, "f_wallclock_ui_displaymode_restored");
	}
	LogBaseline("after_f_wallclock_timeout");
	OptionsMetrics_DumpTree(dialog);
	AfterReinitComboSmoke(dialog);
	FinishAndQuit();
}

void EnterTimeoutPhase(COptionsDialog *dialog)
{
	if (!BeginTimeoutConfirm(dialog))
	{
		FinishAndQuit();
		return;
	}
	Menu_Con("CSRETRO_MODE_SAFETY_WALLCLOCK waiting_real_10s (OnTick deadline; no ExpireConfirmNow)");
	g_wcFrames = 0;
	g_wcPhase = kWcTimeoutConfirmPaint;
}
} // namespace

void OptionsVideoModeSafety_RunGate(COptionsDialog *dialog)
{
	g_wcPhase = kWcIdle;
	g_wcFrames = 0;
	if (!dialog)
	{
		Menu_Con("CSRETRO_MODE_SAFETY_FAIL no_dialog");
		return;
	}

	dialog->OpenTab("Video");
	COptionsSubVideo *video = VideoPage(dialog);
	if (!video)
	{
		Menu_Con("CSRETRO_MODE_SAFETY_FAIL no_video_page");
		return;
	}

	dialog->ResetAllData();
	LogBaseline("start");
	if (WallclockMode())
		Menu_Con("CSRETRO_MODE_SAFETY_WALLCLOCK enabled");

	g_wcBaseW = static_cast<int>(MenuEngine::GetCvarFloat("width"));
	g_wcBaseH = static_cast<int>(MenuEngine::GetCvarFloat("height"));
	g_wcBaseFs = static_cast<int>(MenuEngine::GetCvarFloat("fullscreen"));
	Expect(g_wcBaseW >= 640 && g_wcBaseH >= 480, "baseline_resolution");
	Expect(g_wcBaseFs == 0, "baseline_windowed");

	Expect(PickAlternateResolution(video, g_wcBaseW, g_wcBaseH, g_wcAltW, g_wcAltH), "pick_alternate_resolution");
	if (g_wcAltW <= 0)
	{
		FinishAndQuit();
		return;
	}

	const char *allowFs = std::getenv("CSRETRO_MODE_SAFETY_ALLOW_FS");
	g_wcAllowFs = allowFs && allowFs[0] == '1';

	RunFastPath(dialog, video, g_wcBaseW, g_wcBaseH, g_wcBaseFs, g_wcAltW, g_wcAltH);

	if (WallclockMode())
	{
		if (g_wcAllowFs)
		{
			if (!BeginFsConfirm(dialog))
			{
				FinishAndQuit();
				return;
			}
			g_wcFrames = 0;
			g_wcPhase = kWcFsConfirmPaint;
			return;
		}
		EnterTimeoutPhase(dialog);
		return;
	}

	FinishAndQuit();
}

bool OptionsVideoModeSafety_IsWaitingWallclock()
{
	return g_wcPhase != kWcIdle && g_wcPhase != kWcDone;
}

bool OptionsVideoModeSafety_Poll(COptionsDialog *dialog)
{
	if (g_wcPhase == kWcIdle || g_wcPhase == kWcDone)
		return true;
	if (!dialog)
		return true;

	++g_wcFrames;
	COptionsSubVideo *video = VideoPage(dialog);

	switch (g_wcPhase)
	{
	case kWcFsConfirmPaint:
		// Let Confirm + FS reinit paint before capturing.
		if (g_wcFrames == 45)
			Shot("confirm_after_mode");
		if (g_wcFrames < 60)
			return false;
		if (video)
			video->Gate_ConfirmKeep();
		Expect(video && !video->Gate_IsConfirmOpen(), "a_fs_confirm_closed");
		Expect(static_cast<int>(MenuEngine::GetCvarFloat("fullscreen")) == 1, "a_fs_applied");
		LogBaseline("after_a_fs_keep");
		g_wcFrames = 0;
		g_wcPhase = kWcFsAfterKeepPaint;
		return false;

	case kWcFsAfterKeepPaint:
		// MarkForDeletion leaves QueryBox in the tree for several frames — wait it out.
		if (g_wcFrames < 90)
			return false;
		if (g_wcFrames == 90)
		{
			if (video && video->Gate_IsConfirmOpen())
			{
				Menu_Con("CSRETRO_MODE_SAFETY_FAIL a_fs_confirm_still_open_after_keep");
				FinishAndQuit();
				return true;
			}
			dialog->OpenTab("Video");
			Shot("options_after_keep");
		}
		if (g_wcFrames < 110)
			return false;
		OptionsMetrics_DumpTree(dialog);
		AfterReinitComboSmoke(dialog);
		g_wcFrames = 0;
		g_wcPhase = kWcRestoreWindowed;
		return false;

	case kWcRestoreWindowed:
	{
		if (g_wcFrames == 1)
		{
			dialog->OpenTab("Video");
			video = VideoPage(dialog);
			if (video)
			{
				dialog->ResetAllData();
				video->Gate_SelectResolution(g_wcBaseW, g_wcBaseH);
				video->Gate_SetDisplayModePending(0);
				video->OnApplyChanges();
				if (video->Gate_IsConfirmOpen())
					video->Gate_ConfirmKeep();
				Expect(static_cast<int>(MenuEngine::GetCvarFloat("fullscreen")) == 0, "b_windowed_applied");
				LogBaseline("after_b_windowed_keep");
			}
		}
		if (g_wcFrames < 45)
			return false;
		EnterTimeoutPhase(dialog);
		return false;
	}

	case kWcTimeoutConfirmPaint:
		if (g_wcFrames == 45)
			Shot("confirm_wallclock");
		if (g_wcFrames < 60)
			return false;
		g_wcFrames = 0;
		g_wcPhase = kWcWaitingTimeout;
		return false;

	case kWcWaitingTimeout:
		if (video && video->Gate_IsConfirmOpen())
			return false;
		g_wcFrames = 0;
		g_wcPhase = kWcAfterTimeoutPaint;
		return false;

	case kWcAfterTimeoutPaint:
		if (g_wcFrames < 90)
			return false;
		if (g_wcFrames == 90)
		{
			dialog->OpenTab("Video");
			Shot("options_after_timeout");
		}
		if (g_wcFrames < 110)
			return false;
		g_wcPhase = kWcFinishing;
		CompleteWallclockTimeout(dialog);
		return true;

	case kWcFinishing:
		return true;

	default:
		return true;
	}
}

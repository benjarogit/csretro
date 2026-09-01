#include "OptionsMouseGate.h"
#include "OptionsDialog.h"
#include "OptionsSubMouse.h"
#include "Controls/MenuEngine.h"

#include "vgui_controls/PropertySheet.h"

#include "../src/menu_priv.h"

#include <cmath>
#include <cstring>

namespace
{
bool Near(float a, float b, float eps = 0.05f)
{
	return std::fabs(a - b) <= eps;
}

void Expect(bool ok, const char *tag)
{
	Menu_Con(ok ? "CSRETRO_MOUSE_GATE_OK %s" : "CSRETRO_MOUSE_GATE_FAIL %s", tag);
}

COptionsSubMouse *MousePage(COptionsDialog *dialog)
{
	if (!dialog || !dialog->GetPropertySheet())
		return nullptr;
	return dynamic_cast<COptionsSubMouse *>(dialog->GetPropertySheet()->GetActivePage());
}
} // namespace

void OptionsMouse_RunFunctionalGate(COptionsDialog *dialog)
{
	if (!dialog)
	{
		Menu_Con("CSRETRO_MOUSE_GATE_FAIL no_dialog");
		return;
	}

	dialog->OpenTab("Mouse");
	COptionsSubMouse *mouse = MousePage(dialog);
	if (!mouse)
	{
		Menu_Con("CSRETRO_MOUSE_GATE_FAIL no_mouse_page");
		return;
	}

	// Known baseline
	MenuEngine::CvarSetValue("sensitivity", 3.0f);
	MenuEngine::CvarSetValue("look_filter", 0.0f);
	MenuEngine::CvarSetValue("m_rawinput", 1.0f);
	MenuEngine::CvarSetValue("joystick", 0.0f);
	MenuEngine::CvarSetValue("sv_aim", 0.0f);
	float mp = std::fabs(MenuEngine::GetCvarFloat("m_pitch"));
	if (mp < 0.00001f)
		mp = 0.022f;
	MenuEngine::CvarSetValue("m_pitch", mp);

	dialog->ResetAllData();
	Menu_Con("CSRETRO_MOUSE_GATE_DBG sens_cvar=%g pending=%g", MenuEngine::GetCvarFloat("sensitivity"),
		mouse->Gate_GetSensitivityPending());
	Expect(Near(mouse->Gate_GetSensitivityPending(), 3.0f), "open_reads_sensitivity");
	Expect(!mouse->Gate_GetFilterPending(), "open_reads_look_filter_off");

	// Edit → Apply stays open and writes
	mouse->Gate_SetSensitivityPending(6.25f);
	mouse->Gate_SetFilterPending(true);
	mouse->Gate_SetRawInputPending(false);
	mouse->OnApplyChanges();
	Expect(Near(MenuEngine::GetCvarFloat("sensitivity"), 6.25f), "apply_sensitivity");
	Expect(MenuEngine::GetCvarFloat("look_filter") != 0.0f, "apply_look_filter");
	Expect(MenuEngine::GetCvarFloat("m_rawinput") == 0.0f, "apply_m_rawinput");
	dialog->Gate_Apply(); // PropertyDialog Apply — Dialog bleibt offen
	Expect(dialog->IsVisible(), "apply_keeps_open");

	// Slider ↔ TextEntry sync helpers
	mouse->Gate_SetSensitivityPending(8.0f);
	mouse->Gate_SyncLabelFromSlider();
	Expect(Near(mouse->Gate_GetSensitivityPending(), 8.0f), "slider_pending");

	// Clamp out-of-range
	mouse->Gate_SetSensitivityPending(99.0f);
	mouse->OnApplyChanges(); // BoundSensitivityValue inside
	Expect(mouse->Gate_GetSensitivityPending() <= 20.0f + 0.01f, "sensitivity_clamp_hi");
	mouse->Gate_SetSensitivityPending(0.01f);
	mouse->OnApplyChanges();
	Expect(mouse->Gate_GetSensitivityPending() >= 0.2f - 0.01f, "sensitivity_clamp_lo");

	// Reverse mouse
	mouse->Gate_SetReverseMousePending(true);
	dialog->Gate_Apply();
	Expect(MenuEngine::GetCvarFloat("m_pitch") < 0, "apply_reverse_mouse");

	// Cancel discards pending
	MenuEngine::CvarSetValue("sensitivity", 5.0f);
	dialog->ResetAllData();
	mouse->Gate_SetSensitivityPending(11.0f);
	dialog->Gate_Cancel();
	Expect(Near(MenuEngine::GetCvarFloat("sensitivity"), 5.0f), "cancel_discards");

	// OK applies + closes
	dialog->Activate();
	dialog->OpenTab("Mouse");
	mouse = MousePage(dialog);
	Expect(mouse != nullptr, "reopen_mouse");
	if (mouse)
	{
		dialog->ResetAllData();
		mouse->Gate_SetSensitivityPending(4.5f);
		dialog->Gate_OK();
		Expect(Near(MenuEngine::GetCvarFloat("sensitivity"), 4.5f), "ok_applies");
		Expect(!dialog->IsVisible(), "ok_closes");
	}

	// Reopen shows applied
	dialog->Activate();
	dialog->OpenTab("Mouse");
	mouse = MousePage(dialog);
	if (mouse)
	{
		dialog->ResetAllData();
		Expect(Near(mouse->Gate_GetSensitivityPending(), 4.5f), "reopen_shows_applied");
	}

	// Reset reloads runtime (engine changed externally)
	MenuEngine::CvarSetValue("sensitivity", 2.2f);
	dialog->ResetAllData();
	if (mouse)
		Expect(Near(mouse->Gate_GetSensitivityPending(), 2.2f), "reset_from_runtime");

	// Persist into BASEDIR config (not Steam)
	MenuEngine::ClientCmd("host_writeconfig\n");
	Menu_Con("CSRETRO_MOUSE_GATE_WRITECONFIG");

	Menu_Con("CSRETRO_MOUSE_CVAR sensitivity=%g", MenuEngine::GetCvarFloat("sensitivity"));
	Menu_Con("CSRETRO_MOUSE_CVAR look_filter=%g", MenuEngine::GetCvarFloat("look_filter"));
	Menu_Con("CSRETRO_MOUSE_CVAR m_rawinput=%g", MenuEngine::GetCvarFloat("m_rawinput"));
	Menu_Con("CSRETRO_MOUSE_CVAR m_pitch=%g", MenuEngine::GetCvarFloat("m_pitch"));
	Menu_Con("CSRETRO_MOUSE_CVAR joystick=%g", MenuEngine::GetCvarFloat("joystick"));
	Menu_Con("CSRETRO_MOUSE_CVAR joy_enable=%g", MenuEngine::GetCvarFloat("joy_enable"));
	Menu_Con("CSRETRO_MOUSE_CVAR sv_aim=%g", MenuEngine::GetCvarFloat("sv_aim"));
	Menu_Con("CSRETRO_MOUSE_CVAR m_filter_absent=%d", MenuEngine::GetCvarString("m_filter")[0] ? 0 : 1);

	// Leave dialog open and request engine screenshot (works under headless GL).
	dialog->Activate();
	dialog->OpenTab("Mouse");
	MenuEngine::ClientCmd("screenshot\n");
	Menu_Con("CSRETRO_MOUSE_GATE_SHOT_READY");
	Menu_Con("CSRETRO_MOUSE_GATE_DONE");
}

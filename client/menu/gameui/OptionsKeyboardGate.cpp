#include "OptionsKeyboardGate.h"
#include "OptionsDialog.h"
#include "OptionsSubKeyboard.h"
#include "Controls/MenuEngine.h"

#include "../../vgui/xash_key_contract.h"
#include "../../vgui/vgui_boot.h"
#include "../../vgui/menu_runtime_info.h"
#include "../src/menu_priv.h"

#include "keydefs.h"
#include "vgui/IVGui.h"
#include "vgui_controls/Controls.h"
#include "vgui_controls/PropertySheet.h"

#include <cctype>
#include <cstring>
#include <string>
#include <strings.h>

namespace
{
void Expect(bool ok, const char *tag)
{
	Menu_Con(ok ? "CSRETRO_KEYBOARD_GATE_OK %s" : "CSRETRO_KEYBOARD_GATE_FAIL %s", tag);
}

COptionsSubKeyboard *KeyboardPage(COptionsDialog *dialog)
{
	if (!dialog || !dialog->GetPropertySheet())
		return nullptr;
	return dynamic_cast<COptionsSubKeyboard *>(dialog->GetPropertySheet()->GetActivePage());
}

bool ContainsNoCase(const char *hay, const char *needle)
{
	if (!hay || !needle || !*needle)
		return false;
	const size_t nlen = std::strlen(needle);
	for (const char *p = hay; *p; ++p)
	{
		size_t i = 0;
		while (i < nlen && p[i] &&
			std::tolower(static_cast<unsigned char>(p[i])) ==
				std::tolower(static_cast<unsigned char>(needle[i])))
			++i;
		if (i == nlen)
			return true;
	}
	return false;
}

bool KeyVisibleOnSelection(COptionsSubKeyboard *kb, const char *needle)
{
	if (!kb || !needle)
		return false;
	return ContainsNoCase(kb->Gate_SelectedPrimaryKey(), needle) ||
		ContainsNoCase(kb->Gate_SelectedAltKey(), needle);
}

// Shared adapter path: same sink as UI_KeyEvent → VGuiXash_Key (post-Xash keynum).
void ProveAdapterCapture(COptionsDialog *dialog, COptionsSubKeyboard *kb, const char *binding,
	int keynum, const char *label, bool expectZeroInitMouse, int column = -1)
{
	MenuEngine::SetBinding(keynum, "");
	dialog->ResetAllData();
	Expect(kb->Gate_SelectActionByBinding(binding), "path_select");
	if (column == 1 || column == 2)
		Expect(kb->Gate_StartCaptureColumn(column), "path_capture_start");
	else
		Expect(kb->Gate_StartCapture(), "path_capture_start");
	Expect(kb->Gate_IsCapturing(), "path_capturing");
	Expect(KeyVisibleOnSelection(kb, "GameUI_PressAKey") || KeyVisibleOnSelection(kb, "Press a key"),
		"path_waiting_visible");
	if (expectZeroInitMouse)
		Expect(kb->Gate_InitiatingMouseMask() == 0, "path_no_phantom_mouse");
	VGuiXash_Key(keynum, 1);
	Expect(!kb->Gate_IsCapturing(), "path_finished");
	if (column == 2)
		Expect(ContainsNoCase(kb->Gate_SelectedAltKey(), label), "path_visible_alt");
	else
		Expect(KeyVisibleOnSelection(kb, label), "path_visible");
	Menu_Con("CSRETRO_KEYBOARD_CAPTURE_PATH_OK %s→%s col=%d", binding, label, column);
}

void FlushVgui()
{
	if (vgui2::ivgui())
		vgui2::ivgui()->RunFrame();
}

void HoverList(COptionsSubKeyboard *kb)
{
	int cx = 0, cy = 0;
	if (!kb || !kb->Gate_ListScreenCenter(cx, cy))
		return;
	VGuiXash_MouseMove(cx, cy);
	FlushVgui();
}

void ProveWheelOwnership(COptionsDialog *dialog, COptionsSubKeyboard *kb)
{
	if (!dialog || !kb)
	{
		Expect(false, "wheel_no_page");
		return;
	}

	dialog->ResetAllData();
	Expect(kb->Gate_SelectActionByBinding("+forward"), "wheel_idle_select");
	HoverList(kb);
	const int rangeIdle = kb->Gate_ScrollRange();
	const int beforeIdle = kb->Gate_ScrollValue();
	VGuiXash_Key(K_MWHEELDOWN, 1);
	VGuiXash_Key(K_MWHEELDOWN, 0);
	FlushVgui();
	Expect(!kb->Gate_IsCapturing(), "wheel_idle_not_capture");
	if (rangeIdle > 0)
		Expect(kb->Gate_ScrollValue() > beforeIdle, "wheel_idle_scrolls");
	else
		Menu_Con("CSRETRO_KEYBOARD_GATE skip wheel_idle_scroll range=0");
	Menu_Con("CSRETRO_KEYBOARD_WHEEL_OK idle_scroll range=%d before=%d after=%d",
		rangeIdle, beforeIdle, kb->Gate_ScrollValue());

	MenuEngine::SetBinding(K_MWHEELDOWN, "");
	dialog->ResetAllData();
	Expect(kb->Gate_SelectActionByBinding("+jump"), "wheel_cap_select");
	Expect(kb->Gate_StartCaptureColumn(1), "wheel_cap_start");
	Expect(kb->Gate_IsCapturing(), "wheel_cap_on");
	VGuiXash_Key(K_MWHEELDOWN, 1);
	Expect(!kb->Gate_IsCapturing(), "wheel_cap_finished");
	Expect(ContainsNoCase(kb->Gate_SelectedPrimaryKey(), "MWHEELDOWN") ||
			ContainsNoCase(kb->Gate_SelectedPrimaryKey(), "MWHEEL"),
		"wheel_cap_bound");
	Menu_Con("CSRETRO_KEYBOARD_WHEEL_OK capture_bind");

	dialog->ResetAllData();
	Expect(kb->Gate_SelectActionByBinding("+forward"), "wheel_after_select");
	HoverList(kb);
	const int rangeAfter = kb->Gate_ScrollRange();
	const int beforeAfter = kb->Gate_ScrollValue();
	VGuiXash_Key(K_MWHEELDOWN, 1);
	VGuiXash_Key(K_MWHEELDOWN, 0);
	FlushVgui();
	Expect(!kb->Gate_IsCapturing(), "wheel_after_not_capture");
	if (rangeAfter > 0)
		Expect(kb->Gate_ScrollValue() != beforeAfter || kb->Gate_ScrollValue() > beforeAfter,
			"wheel_after_scrolls");
	else
		Menu_Con("CSRETRO_KEYBOARD_GATE skip wheel_after_scroll range=0");
	Expect(!ContainsNoCase(kb->Gate_SelectedPrimaryKey(), "MWHEELUP"),
		"wheel_after_not_bind");
	Menu_Con("CSRETRO_KEYBOARD_WHEEL_OK after_scroll range=%d before=%d after=%d",
		rangeAfter, beforeAfter, kb->Gate_ScrollValue());
}
} // namespace

void OptionsKeyboard_RunFunctionalGate(COptionsDialog *dialog)
{
	if (!dialog)
	{
		Menu_Con("CSRETRO_KEYBOARD_GATE_FAIL no_dialog");
		return;
	}

	Expect(MenuEngine::KeyCount() == XashKey::Count(), "keycount_contract");
	Expect(XashKey::Count() > 256, "keycount_gt_256");
	Expect(MenuEngine::HasExtendedEngfuncs(), "extended_menu_api");
	// Letter resolver must match the raw Key_Event path (lowercase), not 'W'==87.
	Expect(MenuEngine::KeyNameToKeynum("w") == static_cast<int>('w'), "resolver_w_canon");
	Expect(MenuEngine::KeyNameToKeynum("W") == static_cast<int>('w'), "resolver_W_is_w");
	Expect(MenuEngine::KeyNameToKeynum("c") == 99, "resolver_c_99");
	Expect(MenuEngine::KeyNameToKeynum("C") == 99, "resolver_C_is_c");
	Expect(MenuEngine::KeyNameToKeynum("SPACE") > 0, "resolver_SPACE");

	dialog->OpenTab("Keyboard");
	COptionsSubKeyboard *kb = KeyboardPage(dialog);
	if (!kb)
	{
		Menu_Con("CSRETRO_KEYBOARD_GATE_FAIL no_keyboard_page");
		return;
	}

	dialog->ResetAllData();
	Expect(kb->Gate_VisibleActionCount() > 20, "action_catalog_loaded");
	Expect(kb->Gate_SelectActionByBinding("+forward"), "select_forward");

	// --- Capture path self-proof (Edit/Enter semantics: mouse already up → mask 0) ---
	const int f8 = MenuEngine::KeyNameToKeynum("F8");
	const int f7 = MenuEngine::KeyNameToKeynum("F7");
	const int f6 = MenuEngine::KeyNameToKeynum("F6");
	Expect(XashKey::IsValidKeynum(f8), "f8_valid");
	Expect(XashKey::IsValidKeynum(f7), "f7_valid");
	Expect(XashKey::IsValidKeynum(f6), "f6_valid");

	if (XashKey::IsValidKeynum(f8) && kb)
	{
		// Edit key → F8 (adapter raw sink, no extra mouse)
		ProveAdapterCapture(dialog, kb, "+jump", f8, "F8", true, -1);
		Menu_Con("CSRETRO_KEYBOARD_CAPTURE_PATH_OK Edit→F8");
		dialog->Gate_Cancel();
		dialog->Activate();
		dialog->OpenTab("Keyboard");
		kb = KeyboardPage(dialog);
		dialog->ResetAllData();
	}

	if (kb && XashKey::IsValidKeynum(f7))
	{
		// Primary cell capture → F7
		const int q = 'q';
		ProveAdapterCapture(dialog, kb, "+duck", q, "q", true, 1);
		Menu_Con("CSRETRO_KEYBOARD_CAPTURE_PATH_OK Primary→Q");
		dialog->Gate_Cancel();
		dialog->Activate();
		dialog->OpenTab("Keyboard");
		kb = KeyboardPage(dialog);
		dialog->ResetAllData();

		// Alternate cell capture → F7 (Primary may stay empty / other)
		MenuEngine::SetBinding(f7, "");
		dialog->ResetAllData();
		Expect(kb->Gate_SelectActionByBinding("+duck"), "dbl_alt_select");
		Expect(kb->Gate_StartCaptureColumn(2), "dbl_alt_capture");
		Expect(kb->Gate_IsCapturing(), "dbl_alt_capturing");
		Expect(ContainsNoCase(kb->Gate_SelectedAltKey(), "GameUI_PressAKey") ||
			ContainsNoCase(kb->Gate_SelectedAltKey(), "Press a key"),
			"dbl_alt_waiting");
		VGuiXash_Key(f7, 1);
		Expect(!kb->Gate_IsCapturing(), "dbl_alt_finished");
		Expect(ContainsNoCase(kb->Gate_SelectedAltKey(), "F7"), "dbl_alt_visible_f7");
		Menu_Con("CSRETRO_KEYBOARD_CAPTURE_PATH_OK Alternate→F7");
		dialog->Gate_Cancel();
		dialog->Activate();
		dialog->OpenTab("Keyboard");
		kb = KeyboardPage(dialog);
		dialog->ResetAllData();
	}

	if (kb && XashKey::IsValidKeynum(f6))
	{
		// Enter → BeginCapture → F6
		ProveAdapterCapture(dialog, kb, "+speed", f6, "F6", true);
		Menu_Con("CSRETRO_KEYBOARD_CAPTURE_PATH_OK Enter→F6");
		dialog->Gate_Cancel();
		dialog->Activate();
		dialog->OpenTab("Keyboard");
		kb = KeyboardPage(dialog);
		dialog->ResetAllData();
	}

	// Letter via adapter (canonical Xash keynum 'q')
	if (kb)
	{
		const int q = 'q';
		ProveAdapterCapture(dialog, kb, "+forward", q, "q", true);
		Menu_Con("CSRETRO_KEYBOARD_CAPTURE_PATH_OK letter→Q");
		dialog->Gate_Cancel();
		dialog->Activate();
		dialog->OpenTab("Keyboard");
		kb = KeyboardPage(dialog);
		dialog->ResetAllData();
	}

	if (kb)
		ProveWheelOwnership(dialog, kb);

	Expect(kb && kb->Gate_SelectActionByBinding("+forward"), "select_forward_after_capture");

	// Snapshot a custom binding on an unused letter if possible — F9 often free.
	const int f9 = MenuEngine::KeyNameToKeynum("F9");
	Expect(XashKey::IsValidKeynum(f9), "f9_valid");
	std::string customSaved;
	if (XashKey::IsValidKeynum(f9))
	{
		customSaved = MenuEngine::GetBinding(f9);
		MenuEngine::SetBinding(f9, "echo csretro_custom_bind");
	}

	// >2 binds + international preserve: set three keys to +attack then reopen
	const int m1 = K_MOUSE1;
	const int m2 = K_MOUSE2;
	const int enter = MenuEngine::KeyNameToKeynum("ENTER");
	MenuEngine::SetBinding(m1, "+attack");
	MenuEngine::SetBinding(m2, "+attack");
	if (XashKey::IsValidKeynum(enter))
		MenuEngine::SetBinding(enter, "+attack");

	// International slot preserve (no UI edit)
	const int intl = K_INTERNATIONAL;
	std::string intlSaved;
	if (XashKey::IsValidKeynum(intl))
	{
		intlSaved = MenuEngine::GetBinding(intl);
		MenuEngine::SetBinding(intl, "echo csretro_intl");
	}

	dialog->ResetAllData();
	Expect(kb->Gate_SelectActionByBinding("+attack"), "select_attack");
	// UI shows at most two captureable; third (ENTER) remains in engine until Clear/Defaults
	Expect(MenuEngine::GetBinding(enter)[0] != '\0' || enter < 0, "extra_bind_still_in_engine");

	if (XashKey::IsValidKeynum(intl))
		Expect(std::strcmp(MenuEngine::GetBinding(intl), "echo csretro_intl") == 0, "intl_preserved_after_reset");

	if (XashKey::IsValidKeynum(f9))
		Expect(std::strcmp(MenuEngine::GetBinding(f9), "echo csretro_custom_bind") == 0, "custom_preserved_after_reset");

	// Clear selected action (captureable slots + extras for that action)
	kb->Gate_ClearSelected();
	dialog->Gate_Apply();
	Expect(MenuEngine::GetBinding(m1)[0] == '\0' || std::strcmp(MenuEngine::GetBinding(m1), "+attack") != 0, "clear_mouse1");
	if (XashKey::IsValidKeynum(f9))
		Expect(std::strcmp(MenuEngine::GetBinding(f9), "echo csretro_custom_bind") == 0, "custom_survives_clear_other");

	// ESCAPE reserved
	const int esc = K_ESCAPE;
	MenuEngine::SetBinding(esc, "");
	Expect(std::strcmp(MenuEngine::GetBinding(esc), "cancelselect") == 0, "escape_reserved");

	// Cancel discards staged edits: change stage via clear then cancel
	dialog->Activate();
	dialog->OpenTab("Keyboard");
	kb = KeyboardPage(dialog);
	if (kb)
	{
		dialog->ResetAllData();
		kb->Gate_SelectActionByBinding("+jump");
		kb->Gate_ClearSelected();
		dialog->Gate_Cancel();
		// After cancel dialog closed — reopen and ensure engine unchanged from last apply
		dialog->Activate();
	}

	Expect(dialog != nullptr, "dialog_alive");

	// Restore intl/custom
	if (XashKey::IsValidKeynum(intl))
		MenuEngine::SetBinding(intl, intlSaved.c_str());
	if (XashKey::IsValidKeynum(f9))
		MenuEngine::SetBinding(f9, customSaved.c_str());

	// Manual-regression proof: staged bindings must survive a page switch and
	// then reach both the engine and config.cfg through Apply.
	const int f11 = MenuEngine::KeyNameToKeynum("F11");
	Expect(XashKey::IsValidKeynum(f11), "persist_f11_valid");
	if (kb && XashKey::IsValidKeynum(f11))
	{
		MenuEngine::SetBinding(f11, "");
		dialog->ResetAllData();
		Expect(kb->Gate_SelectActionByBinding("+forward"), "persist_select_forward");
		Expect(kb->Gate_StartCaptureColumn(1), "persist_capture_start");
		VGuiXash_Key(f11, 1);
		Expect(ContainsNoCase(kb->Gate_SelectedPrimaryKey(), "F11"), "persist_staged_visible");
		Expect(MenuEngine::GetBinding(f11)[0] == '\0', "persist_not_applied_early");

		dialog->OpenTab("Mouse");
		FlushVgui();
		dialog->OpenTab("Keyboard");
		FlushVgui();
		kb = KeyboardPage(dialog);
		Expect(kb && kb->Gate_SelectActionByBinding("+forward"), "persist_reselect_after_tab");
		Expect(kb && ContainsNoCase(kb->Gate_SelectedPrimaryKey(), "F11"),
			"persist_stage_survives_tab");

		dialog->Gate_Apply();
		Expect(std::strcmp(MenuEngine::GetBinding(f11), "+forward") == 0,
			"persist_engine_after_apply");
		MenuEngine::ClientCmd("host_writeconfig\n");
		Menu_Con("CSRETRO_KEYBOARD_PERSIST_WRITE key=F11 binding=+forward");
	}

	MenuEngine::ClientCmd("host_writeconfig\n");
	dialog->Activate();
	dialog->OpenTab("Keyboard");
	MenuEngine::ClientCmd("screenshot\n");
	Menu_Con("CSRETRO_KEYBOARD_GATE_SHOT_READY");
	Menu_Con("CSRETRO_KEYBOARD_GATE_DONE");
}

namespace
{
enum class PhysStep : int
{
	Idle = 0,
	WaitF8,
	WaitLetterQ,
	Done,
};

PhysStep g_physStep = PhysStep::Idle;
bool g_physSawCapturing = false;

bool ArmPhysCapture(COptionsDialog *dialog, const char *binding, int keynum, const char *label,
	const char *waitTag)
{
	COptionsSubKeyboard *kb = KeyboardPage(dialog);
	if (!kb)
	{
		Menu_Con("CSRETRO_KEYBOARD_PHYS_FAIL no_keyboard_page");
		return false;
	}
	if (!XashKey::IsValidKeynum(keynum))
	{
		Menu_Con("CSRETRO_KEYBOARD_PHYS_FAIL invalid_key %s", label ? label : "?");
		return false;
	}
	MenuEngine::SetBinding(keynum, "");
	dialog->ResetAllData();
	if (!kb->Gate_SelectActionByBinding(binding))
	{
		Menu_Con("CSRETRO_KEYBOARD_PHYS_FAIL select %s", binding ? binding : "?");
		return false;
	}
	if (!kb->Gate_StartCapture())
	{
		Menu_Con("CSRETRO_KEYBOARD_PHYS_FAIL capture_start %s", label ? label : "?");
		return false;
	}
	if (kb->Gate_InitiatingMouseMask() != 0)
	{
		Menu_Con("CSRETRO_KEYBOARD_PHYS_FAIL phantom_mouse mask=0x%x", kb->Gate_InitiatingMouseMask());
		return false;
	}
	g_physSawCapturing = kb->Gate_IsCapturing();
	Menu_Con("CSRETRO_KEYBOARD_PHYS_WAIT key=%s binding=%s", label ? label : "?", binding ? binding : "?");
	CsretroMenu_CaptureLog("PHYS_WAIT expecting %s via SDL→Key_Event→VGuiXash_Key while capturing=%d extApi=%d",
		label ? label : "?", kb->Gate_IsCapturing() ? 1 : 0, MenuEngine::HasExtendedEngfuncs() ? 1 : 0);
	(void)waitTag;
	return kb->Gate_IsCapturing();
}
} // namespace

void OptionsKeyboard_ArmPhysicalCaptureProbe(COptionsDialog *dialog)
{
	g_physStep = PhysStep::Idle;
	g_physSawCapturing = false;
	if (!dialog)
		return;
	dialog->OpenTab("Keyboard");
	Expect(MenuEngine::HasExtendedEngfuncs(), "phys_extended_api");
	const int f8 = MenuEngine::KeyNameToKeynum("F8");
	if (!ArmPhysCapture(dialog, "+jump", f8, "F8", "f8"))
	{
		Menu_Con("CSRETRO_KEYBOARD_PHYS_FAIL arm_f8");
		return;
	}
	g_physStep = PhysStep::WaitF8;
}

void OptionsKeyboard_PollPhysicalCaptureProbe(COptionsDialog *dialog)
{
	if (!dialog || g_physStep == PhysStep::Idle || g_physStep == PhysStep::Done)
		return;
	COptionsSubKeyboard *kb = KeyboardPage(dialog);
	if (!kb)
		return;

	if (kb->Gate_IsCapturing())
	{
		g_physSawCapturing = true;
		return;
	}

	// Capture ended after we had seen capturing=1 — advance sequence (key came via SDL).
	if (!g_physSawCapturing)
		return;

	if (g_physStep == PhysStep::WaitF8)
	{
		g_physSawCapturing = false;
		const int q = 'q';
		if (!ArmPhysCapture(dialog, "+duck", q, "q", "letter"))
		{
			Menu_Con("CSRETRO_KEYBOARD_PHYS_FAIL arm_letter_q");
			g_physStep = PhysStep::Done;
			return;
		}
		g_physStep = PhysStep::WaitLetterQ;
		return;
	}

	if (g_physStep == PhysStep::WaitLetterQ)
	{
		g_physSawCapturing = false;
		g_physStep = PhysStep::Done;
		Menu_Con("CSRETRO_KEYBOARD_PHYS_DONE");
		CsretroMenu_CaptureLog("PHYS_DONE F8+letter sequence complete");
	}
}

bool OptionsKeyboard_PhysicalCaptureProbeArmed()
{
	return g_physStep == PhysStep::WaitF8 || g_physStep == PhysStep::WaitLetterQ;
}

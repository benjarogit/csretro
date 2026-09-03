#include "OptionsMenuComboGate.h"
#include "OptionsDialog.h"
#include "OptionsMetricsDump.h"

#include "vgui/IVGui.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/MenuItem.h"
#include "tier1/KeyValues.h"

#include "../src/menu_priv.h"

#include <cstdint>

using namespace vgui2;

namespace
{
void Expect(bool ok, const char *tag)
{
	Menu_Con(ok ? "CSRETRO_COMBO_GATE_OK %s" : "CSRETRO_COMBO_GATE_FAIL %s", tag);
}

ComboBox *FindCombo(COptionsDialog *dialog, const char *name)
{
	return dialog ? dynamic_cast<ComboBox *>(dialog->FindChildByName(name, true)) : nullptr;
}

// Drive MenuItem cursor enter/exit the same way the mouse does (posts CursorEnteredMenuItem).
bool ExerciseComboMenu(ComboBox *combo, const char *tag)
{
	if (!combo)
	{
		Expect(false, tag);
		return false;
	}

	combo->ShowMenu();
	if (!combo->IsDropdownVisible())
	{
		Expect(false, tag);
		return false;
	}

	// One-shot size probe, then quiet.
	static bool s_loggedSizes = false;
	if (!s_loggedSizes)
	{
		s_loggedSizes = true;
		Menu_Con("CSRETRO_COMBO_GATE_SIZEOF VPANEL=%zu uintptr_t=%zu uint64=%zu", sizeof(VPANEL),
			sizeof(uintptr_t), sizeof(uint64));
	}

	Menu *menu = combo->GetMenu();
	if (!menu)
	{
		combo->HideMenu();
		Menu_Con("CSRETRO_COMBO_GATE_FAIL %s_no_menu_ptr", tag);
		Expect(false, tag);
		return false;
	}

	const int n = menu->GetItemCount();
	Expect(n > 0, tag);
	bool roundTripOk = true;
	for (int row = 0; row < n; ++row)
	{
		const int itemId = menu->GetMenuID(row);
		MenuItem *item = menu->GetMenuItem(itemId);
		if (!item)
			continue;
		const VPANEL vp = item->GetVPanel();
		const uint64 packed = static_cast<uint64>(vp);
		const VPANEL back = static_cast<VPANEL>(packed);
		if (vp != back)
		{
			roundTripOk = false;
			Menu_Con("CSRETRO_COMBO_GATE_FAIL %s_vpanel_trunc item=%d in=%llx out=%llx", tag, row,
				static_cast<unsigned long long>(vp), static_cast<unsigned long long>(back));
		}

		item->OnCursorEntered();
		if (ivgui())
			ivgui()->RunFrame();
		item->OnCursorExited();
		if (ivgui())
			ivgui()->RunFrame();
	}

	combo->HideMenu();
	if (ivgui())
		ivgui()->RunFrame();

	Expect(roundTripOk, tag);
	return roundTripOk;
}
} // namespace

void OptionsMenuCombo_RunGate(COptionsDialog *dialog)
{
	if (!dialog)
	{
		Menu_Con("CSRETRO_COMBO_GATE_FAIL no_dialog");
		return;
	}

	dialog->Activate();
	dialog->OpenTab("Video");
	OptionsMetrics_DumpTree(dialog);

	static const char *kVideoCombos[] = {"Resolution", "AspectRatio", "DisplayMode", "Renderer"};
	bool allOk = true;
	for (const char *name : kVideoCombos)
		allOk = ExerciseComboMenu(FindCombo(dialog, name), name) && allOk;

	dialog->OpenTab("Audio");
	allOk = ExerciseComboMenu(FindCombo(dialog, "Sound Quality"), "SoundQuality") && allOk;

	// Repeat open/close stress on Display Mode (crash repro path).
	if (ComboBox *dm = FindCombo(dialog, "DisplayMode"))
	{
		dialog->OpenTab("Video");
		for (int i = 0; i < 3; ++i)
		{
			dm->ShowMenu();
			if (ivgui())
				ivgui()->RunFrame();
			dm->HideMenu();
			if (ivgui())
				ivgui()->RunFrame();
		}
		allOk = ExerciseComboMenu(dm, "DisplayMode_repeat") && allOk;
	}

	Expect(allOk, "all_combos");
	Menu_Con(allOk ? "CSRETRO_COMBO_GATE_DONE" : "CSRETRO_COMBO_GATE_DONE_WITH_FAILS");
}

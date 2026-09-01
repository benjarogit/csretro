#include "OptionsMetricsDump.h"
#include "OptionsDialog.h"

#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Button.h>

#include "../src/menu_priv.h"

#include <cstdlib>
#include <cstdio>

using namespace vgui2;

namespace
{
void DumpPanelLine(const char *tag, Panel *p)
{
	if (!p)
	{
		Menu_Con("CSRETRO_METRICS %s null", tag);
		return;
	}
	int x = 0, y = 0, w = 0, h = 0;
	p->GetBounds(x, y, w, h);
	Menu_Con("CSRETRO_METRICS %s name=%s prop=%d bounds=%d,%d %dx%d", tag,
		p->GetName() ? p->GetName() : "?", p->IsProportional() ? 1 : 0, x, y, w, h);
}
} // namespace

void OptionsMetrics_DumpTree(COptionsDialog *dialog)
{
	if (!dialog || !std::getenv("CSRETRO_VGUI_METRICS_DUMP"))
		return;

	int sw = 0, sh = 0, pbW = 0, pbH = 0;
	surface()->GetScreenSize(sw, sh);
	surface()->GetProportionalBase(pbW, pbH);
	Menu_Con("CSRETRO_METRICS_BEGIN screen=%dx%d proportionalBase=%dx%d", sw, sh, pbW, pbH);

	DumpPanelLine("OptionsDialog", dialog);
	PropertySheet *sheet = dialog->GetPropertySheet();
	DumpPanelLine("PropertySheet", sheet);

	if (sheet)
	{
		IScheme *sch = scheme()->GetIScheme(sheet->GetScheme());
		HFont tabFont = sch ? sch->GetFont("Default", sheet->IsProportional()) : 0;
		Menu_Con("CSRETRO_METRICS PropertySheet tabFontTall=%d", surface()->GetFontTall(tabFont));
		for (int i = 0; i < sheet->GetNumPages(); ++i)
		{
			Panel *page = sheet->GetPage(i);
			char tag[64];
			std::snprintf(tag, sizeof(tag), "Page[%d]", i);
			DumpPanelLine(tag, page);
		}
	}

	// Scheme fonts (non-proportional + proportional aliases)
	IScheme *sch = scheme()->GetIScheme(dialog->GetScheme());
	if (sch)
	{
		static const char *kFonts[] = {"Default", "DefaultSmall", "DefaultVerySmall", "DefaultBold", "Marlett"};
		for (const char *fn : kFonts)
		{
			for (int prop = 0; prop < 2; ++prop)
			{
				HFont f = sch->GetFont(fn, prop != 0);
				Menu_Con("CSRETRO_METRICS_FONT scheme=%s prop=%d handle=%u tall=%d", fn, prop,
					static_cast<unsigned>(f), surface()->GetFontTall(f));
			}
		}
	}

	DumpPanelLine("OKButton", dialog->FindChildByName("OKButton", true));
	DumpPanelLine("CancelButton", dialog->FindChildByName("CancelButton", true));
	DumpPanelLine("ApplyButton", dialog->FindChildByName("ApplyButton", true));

	static const char *kControls[] = {
		"ReverseMouse", "MouseFilter", "MouseLook", "Joystick", "JoystickLook",
		"Slider", "SensitivityLabel",
		"SFX Slider", "MP3 Volume", "Suit Slider", "Sound Quality",
		"sfx label", "mp3 label", "suit label",
	};
	for (const char *name : kControls)
		DumpPanelLine(name, dialog->FindChildByName(name, true));

	Menu_Con("CSRETRO_METRICS_END");
}

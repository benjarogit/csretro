#include "OptionsMetricsDump.h"
#include "OptionsDialog.h"

#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/TextImage.h>

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

	static const char *kButtons[] = {"OKButton", "CancelButton", "ApplyButton"};
	for (const char *name : kButtons)
	{
		Panel *p = dialog->FindChildByName(name, true);
		DumpPanelLine(name, p);
		if (auto *btn = dynamic_cast<Button *>(p))
		{
			char text[128] = {};
			btn->GetText(text, sizeof(text));
			const Color fg = btn->GetFgColor();
			int tw = 0, th = 0;
			if (TextImage *ti = btn->GetTextImage())
			{
				ti->GetContentSize(tw, th);
				const Color tc = ti->GetColor();
				const int imgCount = btn->GetImageCount();
				IImage *slot0 = btn->GetImageAtIndex(0);
				Menu_Con("CSRETRO_METRICS_BTN %s text='%s' font=%u tiFont=%u tw=%d th=%d imgs=%d slot0=%s visible=%d enabled=%d fg=%d,%d,%d,%d tc=%d,%d,%d,%d paint=%d",
					name, text, static_cast<unsigned>(btn->GetFont()), static_cast<unsigned>(ti->GetFont()),
					tw, th, imgCount, slot0 == static_cast<IImage *>(ti) ? "text" : (slot0 ? "other" : "null"),
					btn->IsVisible() ? 1 : 0, btn->IsEnabled() ? 1 : 0, fg.r(), fg.g(), fg.b(), fg.a(),
					tc.r(), tc.g(), tc.b(), tc.a(), btn->ShouldPaint() ? 1 : 0);
			}
			else
			{
				Menu_Con("CSRETRO_METRICS_BTN %s text='%s' font=%u visible=%d enabled=%d fg=%d,%d,%d,%d no_textimage",
					name, text, static_cast<unsigned>(btn->GetFont()), btn->IsVisible() ? 1 : 0,
					btn->IsEnabled() ? 1 : 0, fg.r(), fg.g(), fg.b(), fg.a());
			}
		}
	}

	static const char *kControls[] = {
		"ReverseMouse", "MouseFilter", "MouseLook", "Joystick", "JoystickLook",
		"Slider", "SensitivityLabel",
		"SFX Slider", "MP3 Volume", "Suit Slider", "Sound Quality",
		"sfx label", "mp3 label", "suit label",
		"Resolution", "AspectRatio", "DisplayMode", "Renderer", "VSync", "DetailTextures",
		"Brightness", "Gamma", "VideoNote",
	};
	for (const char *name : kControls)
	{
		Panel *p = dialog->FindChildByName(name, true);
		DumpPanelLine(name, p);
		if (p)
			Menu_Con("CSRETRO_METRICS_VIS %s visible=%d enabled=%d", name, p->IsVisible() ? 1 : 0,
				p->IsEnabled() ? 1 : 0);
	}

	Menu_Con("CSRETRO_METRICS_END");
}

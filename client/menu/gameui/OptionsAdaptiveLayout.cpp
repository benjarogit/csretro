#include "OptionsAdaptiveLayout.h"

#include <vgui_controls/Panel.h>
#include <vgui_controls/PropertyDialog.h>
#include <vgui_controls/PropertySheet.h>

#include <algorithm>
#include <cstring>

using namespace vgui2;

namespace CsretroOptionsLayout
{
void CaptureRect(Panel *p, Rect &out)
{
	out = {};
	if (!p)
		return;
	p->GetBounds(out.x, out.y, out.w, out.h);
	out.valid = out.w > 0 && out.h > 0;
}

void MeasureVisibleBounds(Panel *page, int &right, int &bottom)
{
	right = 0;
	bottom = 0;
	if (!page)
		return;
	const int n = page->GetChildCount();
	for (int i = 0; i < n; ++i)
	{
		Panel *c = page->GetChild(i);
		if (!c || !c->IsVisible())
			continue;
		int x = 0, y = 0, w = 0, h = 0;
		c->GetBounds(x, y, w, h);
		if (w <= 0 || h <= 0)
			continue;
		right = std::max(right, x + w);
		bottom = std::max(bottom, y + h);
	}
}

Panel *FindOptionsDialog(Panel *start)
{
	for (Panel *p = start; p; p = p->GetParent())
	{
		const char *n = p->GetName();
		if (n && !std::strcmp(n, "OptionsDialog"))
			return p;
	}
	return nullptr;
}

void ComputeMinimum(Panel *dialog, int &minW, int &minH)
{
	minW = CsretroOptionsClassic::kPreferredWide;
	minH = CsretroOptionsClassic::kPreferredTall;
	if (!dialog)
		return;

	auto *prop = dynamic_cast<PropertyDialog *>(dialog);
	Panel *page = prop ? prop->GetActivePage() : nullptr;
	int dw = 0, dh = 0, pw = 0, ph = 0;
	dialog->GetSize(dw, dh);
	if (page)
		page->GetSize(pw, ph);
	if (dw < 1 || dh < 1 || pw < 1 || ph < 1)
		return;

	const int chromeW = dw - pw;
	const int chromeH = dh - ph;
	int contentR = 0, contentB = 0;
	if (prop)
	{
		if (auto *sheet = prop->GetPropertySheet())
		{
			const int tabs = sheet->GetNumPages();
			for (int t = 0; t < tabs; ++t)
			{
				Panel *pg = sheet->GetPage(t);
				int r = 0, b = 0;
				MeasureVisibleBounds(pg, r, b);
				contentR = std::max(contentR, r);
				contentB = std::max(contentB, b);
			}
		}
	}
	else
		MeasureVisibleBounds(page, contentR, contentB);

	const int pageMinW = std::max(contentR + 8,
		CsretroOptionsClassic::kKeyboardListMinW + 16);
	const int pageMinH = std::max(contentB + 8,
		CsretroOptionsClassic::kKeyboardListMinH + 40);
	const int rawW = std::max(chromeW + pageMinW, 360);
	const int rawH = std::max(chromeH + pageMinH, 280);
	// Mouse/Audio/Video .res already fills Classic Preferred — do not demand larger.
	minW = std::min(rawW, CsretroOptionsClassic::kPreferredWide);
	minH = std::min(rawH, CsretroOptionsClassic::kPreferredTall);
}

void ApplyKeyboardGrow(Panel *page, Panel *list, Panel *defaultsBtn, Panel *changeBtn,
	Panel *clearBtn, const Rect &designList, const Rect &designDefaults,
	const Rect &designChange, const Rect &designClear, int extraW, int extraH)
{
	if (!page || !list || !designList.valid)
		return;

	if (extraW == 0 && extraH == 0)
	{
		list->SetBounds(designList.x, designList.y, designList.w, designList.h);
		if (defaultsBtn && designDefaults.valid)
			defaultsBtn->SetBounds(designDefaults.x, designDefaults.y, designDefaults.w, designDefaults.h);
		if (changeBtn && designChange.valid)
			changeBtn->SetBounds(designChange.x, designChange.y, designChange.w, designChange.h);
		if (clearBtn && designClear.valid)
			clearBtn->SetBounds(designClear.x, designClear.y, designClear.w, designClear.h);
		return;
	}

	int pw = 0, ph = 0;
	page->GetSize(pw, ph);
	int listW = std::max(designList.w + extraW, CsretroOptionsClassic::kKeyboardListMinW);
	int listH = std::max(designList.h + extraH, CsretroOptionsClassic::kKeyboardListMinH);
	const int btnH = designDefaults.valid ? designDefaults.h : 24;
	const int bottomReserve = btnH + 12;
	if (pw > 0)
		listW = std::min(listW, std::max(CsretroOptionsClassic::kKeyboardListMinW, pw - designList.x - 8));
	if (ph > 0)
		listH = std::min(listH, std::max(CsretroOptionsClassic::kKeyboardListMinH, ph - designList.y - bottomReserve));
	list->SetBounds(designList.x, designList.y, listW, listH);

	const int btnY = designList.y + listH + 8;
	if (defaultsBtn && designDefaults.valid)
		defaultsBtn->SetBounds(designDefaults.x, btnY, designDefaults.w, designDefaults.h);
	if (clearBtn && designClear.valid)
	{
		const int cx = designList.x + listW - designClear.w;
		clearBtn->SetBounds(cx, btnY, designClear.w, designClear.h);
	}
	if (changeBtn && designChange.valid)
	{
		const int gap = designClear.valid
			? (designClear.x - (designChange.x + designChange.w))
			: 6;
		const int clearX = (clearBtn && designClear.valid)
			? (designList.x + listW - designClear.w)
			: (designList.x + listW);
		changeBtn->SetBounds(clearX - gap - designChange.w, btnY, designChange.w, designChange.h);
	}
}
} // namespace CsretroOptionsLayout

#pragma once

#include "OptionsClassicMetrics.h"

namespace vgui2
{
class Panel;
}

// Adaptive Options layout — Classic Preferred 512×406 is the reference.
// The current classic Mouse/Audio/Video resources derive the same effective minimum.
namespace CsretroOptionsLayout
{
struct Rect
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	bool valid = false;
};

void CaptureRect(vgui2::Panel *p, Rect &out);
void MeasureVisibleBounds(vgui2::Panel *page, int &right, int &bottom);
void ComputeMinimum(vgui2::Panel *dialog, int &minW, int &minH);
vgui2::Panel *FindOptionsDialog(vgui2::Panel *start);

// Keyboard list/buttons from Classic .res + extra pixels (dialog − Preferred).
void ApplyKeyboardGrow(vgui2::Panel *page, vgui2::Panel *list, vgui2::Panel *defaultsBtn,
	vgui2::Panel *changeBtn, vgui2::Panel *clearBtn, const Rect &designList,
	const Rect &designDefaults, const Rect &designChange, const Rect &designClear,
	int extraW, int extraH);
} // namespace CsretroOptionsLayout

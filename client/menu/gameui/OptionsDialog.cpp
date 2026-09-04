#include "OptionsDialog.h"
#include "OptionsAdaptiveLayout.h"
#include "OptionsClassicMetrics.h"
#include "OptionsSubAudio.h"
#include "OptionsSubKeyboard.h"
#include "OptionsSubMouse.h"
#include "OptionsSubVideo.h"
#include "OptionsMetricsDump.h"

#include "../vgui/xash_key_contract.h"
#include "../vgui/window_geometry.h"

#include <vgui/KeyCode.h>
#include <vgui/IInputInternal.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/PropertySheet.h>

#include "../src/menu_priv.h"

#include <cstring>
#include <algorithm>

using namespace vgui2;

COptionsDialog::COptionsDialog(Panel *parent)
	: PropertyDialog(parent, "OptionsDialog")
{
	SetDeleteSelfOnClose(false);
	// Classic Preferred Size — not a max; PropertyDialog/Sheet layout fills ClientArea.
	SetBounds(0, 0, CsretroOptionsClassic::kPreferredWide, CsretroOptionsClassic::kPreferredTall);
	// Adaptive page layout, minimum size and workspace clamping are established.
	// Use Frame's native eight edge/corner grips for real user resizing.
	SetSizeable(true);
	SetMoveable(true);
	SetClipToParent(true);
	SetTitle("#GameUI_Options", true);
	SetApplyButtonVisible(true);
	if (PropertySheet *sheet = GetPropertySheet())
	{
		// Classic compact tabs: min width 72, height 24 (Golden/CKF closer than default 84×28).
		sheet->SetTabWidth(72);
		sheet->SetTabHeight(24);
	}

	// Keine Stub-Tabs. Sichtbar: Keyboard | Mouse | Audio | Video
	RegisterPage(new COptionsSubKeyboard(this), "Keyboard", "#GameUI_Keyboard");
	RegisterPage(new COptionsSubMouse(this), "Mouse", "#GameUI_Mouse");
	RegisterPage(new COptionsSubAudio(this), "Audio", "#GameUI_Audio");
	RegisterPage(new COptionsSubVideo(this), "Video", "#GameUI_Video");
}

COptionsDialog::~COptionsDialog() = default;

void COptionsDialog::RegisterPage(PropertyPage *page, const char *tabKey, const char *tabTitle)
{
	if (!page || !tabKey || !tabTitle)
		return;
	AddPage(page, tabTitle);
	m_tabNames.Insert(tabKey, page);
}

bool COptionsDialog::HasPages() const
{
	return m_tabNames.Count() > 0;
}

PropertyPage *COptionsDialog::FindPage(const char *tabKey) const
{
	if (!tabKey)
		return nullptr;
	const unsigned short idx = m_tabNames.Find(tabKey);
	if (!m_tabNames.IsValidIndex(idx))
		return nullptr;
	return m_tabNames[idx];
}

bool COptionsDialog::IsKeyboardCapturing() const
{
	auto *kb = dynamic_cast<COptionsSubKeyboard *>(FindPage("Keyboard"));
	return kb && kb->IsCapturing();
}

bool COptionsDialog::OnRawXashKey(int keynum, bool down)
{
	auto *kb = dynamic_cast<COptionsSubKeyboard *>(FindPage("Keyboard"));
	if (!kb)
		return false;
	// Always enter page sink while Options visible so capture_debug sees non-capture keys too.
	return kb->OnRawXashKey(keynum, down);
}

void COptionsDialog::Gate_Apply() { OnCommand("Apply"); }
void COptionsDialog::Gate_OK() { OnCommand("OK"); }
void COptionsDialog::Gate_Cancel() { OnCommand("Cancel"); }

void COptionsDialog::OnKeyCodeTyped(KeyCode code)
{
	if (code == KEY_ESCAPE)
	{
		if (IsKeyboardCapturing())
		{
			OnRawXashKey(K_ESCAPE, true);
			return;
		}
		Close();
		return;
	}
	BaseClass::OnKeyCodeTyped(code);
}

void COptionsDialog::OpenTab(const char *tabName)
{
	if (!tabName || !GetPropertySheet())
		return;
	const unsigned short idx = m_tabNames.Find(tabName);
	if (m_tabNames.IsValidIndex(idx))
		GetPropertySheet()->SetActivePage(m_tabNames[idx]);
}

void COptionsDialog::Activate()
{
	BaseClass::Activate();
	// Compute after all page resources exist and keep Frame's real drag limit in
	// sync before the user can reach an edge/corner grip.
	int minW = 0, minH = 0;
	GetAdaptiveMinimum(minW, minH);
	if (minW > 0 && minH > 0)
		SetMinimumSize(minW, minH);
	SetVisible(true);
	SetMouseInputEnabled(true);
	SetKeyBoardInputEnabled(true);
	MoveToFront();
	m_geometryTracking = true;
	m_geometryDirty = false;
	Menu_Con("CSRETRO_OPTIONS_VISIBLE");
	OptionsMetrics_DumpTree(this);
}

void COptionsDialog::PerformLayout()
{
	BaseClass::PerformLayout();
	int minW = 0, minH = 0;
	GetAdaptiveMinimum(minW, minH);
	if (minW > 0 && minH > 0)
		SetMinimumSize(minW, minH);
}

void COptionsDialog::GetAdaptiveMinimum(int &minW, int &minH)
{
	int derivedW = 0, derivedH = 0;
	CsretroOptionsLayout::ComputeMinimum(this, derivedW, derivedH);
	// Live child bounds change while pages grow/shrink. They must not lower a
	// content minimum that was already established from the Classic resources.
	m_adaptiveMinW = std::max(m_adaptiveMinW, derivedW);
	m_adaptiveMinH = std::max(m_adaptiveMinH, derivedH);
	minW = m_adaptiveMinW;
	minH = m_adaptiveMinH;
}

void COptionsDialog::ClampToCurrentWorkspace(int workW, int workH)
{
	int x = 0, y = 0, w = 0, h = 0;
	GetBounds(x, y, w, h);
	int minW = 0, minH = 0;
	GetAdaptiveMinimum(minW, minH);
	CsretroWindowGeometry::ClampBounds(x, y, w, h, minW, minH, 0, 0, workW, workH);
	SetSize(w, h);
	SetPos(x, y);
}

void COptionsDialog::Gate_SetSize(int wide, int tall)
{
	int minW = 0, minH = 0;
	GetAdaptiveMinimum(minW, minH);
	if (wide < minW)
		wide = minW;
	if (tall < minH)
		tall = minH;
	SetSize(wide, tall);
	InvalidateLayout(true, true);
}

bool COptionsDialog::Gate_IsSizeable()
{
	return IsSizeable();
}

void COptionsDialog::ScheduleGeometrySave()
{
	if (m_geometryTracking)
		m_geometryDirty = true;
}

void COptionsDialog::SaveGeometryNow()
{
	int x = 0, y = 0, w = 0, h = 0;
	GetBounds(x, y, w, h);
	CsretroWindowGeometry::Save("Options", x, y, w, h);
	m_geometryDirty = false;
}

void COptionsDialog::OnMove()
{
	BaseClass::OnMove();
	ScheduleGeometrySave();
}

void COptionsDialog::OnSizeChanged(int newWide, int newTall)
{
	BaseClass::OnSizeChanged(newWide, newTall);
	ScheduleGeometrySave();
}

void COptionsDialog::OnThink()
{
	BaseClass::OnThink();
	// Persist once the drag ends. Geometry is independent of Apply/Cancel,
	// while page settings retain their normal staged semantics.
	if (m_geometryDirty && (!vgui2::input() || !vgui2::input()->IsMouseDown(MOUSE_LEFT)))
		SaveGeometryNow();
}

void COptionsDialog::OnClose()
{
	SaveGeometryNow();
	m_geometryTracking = false;
	// X/ESC/Cancel discard staged page values. OK already applied them, so the
	// same reset is harmless there and gives the next open a fresh snapshot.
	ResetAllData();
	Menu_Con("CSRETRO_OPTIONS_CLOSE");
	BaseClass::OnClose();
}

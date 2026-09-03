#include "VControlsListPanel.h"

#include "tier1/KeyValues.h"
#include "vgui/IInputInternal.h"
#include "vgui/ISchemeNext.h"
#include "vgui/ISurfaceNext.h"
#include "vgui/Cursor.h"
#include "vgui/KeyCode.h"
#include "vgui_controls/Label.h"

using namespace vgui2;

namespace
{
class CsretroInlineEditPanel : public Label
{
public:
	CsretroInlineEditPanel() : Label(nullptr, "InlineEditPanel", "#GameUI_PressAKey") {}

	void OnKeyCodeTyped(KeyCode code) override
	{
		if (GetParent())
			GetParent()->OnKeyCodeTyped(code);
	}

	void OnKeyCodePressed(KeyCode code) override
	{
		if (GetParent())
			GetParent()->OnKeyCodePressed(code);
	}

	void ApplySchemeSettings(IScheme *pScheme) override
	{
		Label::ApplySchemeSettings(pScheme);
		SetBorder(pScheme->GetBorder("DepressedButtonBorder"));
		SetBgColor(GetSchemeColor("ControlBG", pScheme));
		SetFgColor(GetSchemeColor("ControlFG", pScheme));
		SetContentAlignment(Label::a_west);
		SetTextInset(4, 0);
	}

	void OnMousePressed(MouseCode code) override
	{
		if (GetParent())
			GetParent()->OnMousePressed(code);
	}

	void OnMouseWheeled(int delta) override
	{
		if (GetParent())
			GetParent()->OnMouseWheeled(delta);
	}
};
} // namespace

VControlsListPanel::VControlsListPanel(Panel *parent, const char *listName)
	: SectionedListPanel(parent, listName)
{
	m_pInlineEditPanel = new CsretroInlineEditPanel();
}

VControlsListPanel::~VControlsListPanel()
{
	if (m_pInlineEditPanel)
		m_pInlineEditPanel->MarkForDeletion();
	m_pInlineEditPanel = nullptr;
}

void VControlsListPanel::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	m_hFont = pScheme->GetFont("Default", IsProportional());
}

void VControlsListPanel::StartCaptureMode(int column, HCursor hCursor)
{
	m_bCaptureMode = true;
	m_nCaptureColumn = (column == 2) ? 2 : 1;
	if (IsItemIDValid(GetSelectedItem()))
		m_nClickRow = GetSelectedItem();
	if (auto *label = dynamic_cast<Label *>(m_pInlineEditPanel))
		label->SetText("#GameUI_PressAKey");
	EnterEditMode(m_nClickRow, m_nCaptureColumn, m_pInlineEditPanel);
	input()->SetMouseFocus(m_pInlineEditPanel->GetVPanel());
	input()->SetMouseCapture(m_pInlineEditPanel->GetVPanel());
	m_pInlineEditPanel->RequestFocus();
	RequestFocus();
	if (hCursor)
	{
		m_pInlineEditPanel->SetCursor(hCursor);
		input()->GetCursorPos(m_iMouseX, m_iMouseY);
	}
}

void VControlsListPanel::EndCaptureMode(HCursor hCursor)
{
	m_bCaptureMode = false;
	input()->SetMouseCapture(0);
	LeaveEditMode();
	RequestFocus();
	input()->SetMouseFocus(GetVPanel());
	if (hCursor)
	{
		m_pInlineEditPanel->SetCursor(hCursor);
		surface()->SetCursor(hCursor);
		if (hCursor != dc_none)
			input()->SetCursorPos(m_iMouseX, m_iMouseY);
	}
}

void VControlsListPanel::SetItemOfInterest(int itemID) { m_nClickRow = itemID; }
int VControlsListPanel::GetItemOfInterest() const { return m_nClickRow; }
bool VControlsListPanel::IsCapturing() const { return m_bCaptureMode; }

int VControlsListPanel::ResolveCaptureColumnAtCursor(int itemID, int preferredDefault) const
{
	auto *self = const_cast<VControlsListPanel *>(this);
	if (!self->IsItemIDValid(itemID))
		return preferredDefault == 2 ? 2 : 1;
	int cx = 0, cy = 0;
	input()->GetCursorPos(cx, cy);
	self->ScreenToLocal(cx, cy);
	for (int col = 1; col <= 2; ++col)
	{
		int x = 0, y = 0, w = 0, h = 0;
		if (!self->GetCellBounds(itemID, col, x, y, w, h))
			continue;
		if (cx >= x && cx < x + w && cy >= y && cy < y + h)
			return col;
	}
	return preferredDefault == 2 ? 2 : 1;
}

bool VControlsListPanel::AreVisibleChildrenContained() const
{
	auto *self = const_cast<VControlsListPanel *>(this);
	const int listWide = self->GetWide();
	const int listTall = self->GetTall();
	for (int i = 0; i < self->GetChildCount(); ++i)
	{
		Panel *child = self->GetChild(i);
		if (!child || !child->IsVisible())
			continue;
		int x = 0, y = 0, wide = 0, tall = 0;
		child->GetBounds(x, y, wide, tall);
		if (x < 0 || y < 0 || x + wide > listWide || y + tall > listTall)
			return false;
	}
	return true;
}

void VControlsListPanel::OnKeyCodeTyped(KeyCode code)
{
	// Physical Enter while list focused → page BeginCapture.
	// Double-click no longer synthesizes KEY_ENTER (SectionedListPanel CS Retro patch).
	if (code == KEY_ENTER && GetParent())
	{
		GetParent()->OnKeyCodeTyped(code);
		return;
	}
	if (IsCapturing() && GetParent())
	{
		GetParent()->OnKeyCodeTyped(code);
		return;
	}
	BaseClass::OnKeyCodeTyped(code);
}

void VControlsListPanel::OnKeyCodePressed(KeyCode code)
{
	if (IsCapturing() && GetParent())
	{
		GetParent()->OnKeyCodePressed(code);
		return;
	}
	BaseClass::OnKeyCodePressed(code);
}

void VControlsListPanel::OnMousePressed(MouseCode code)
{
	if (IsCapturing())
	{
		if (GetParent())
			GetParent()->OnMousePressed(code);
		return;
	}
	BaseClass::OnMousePressed(code);
}

void VControlsListPanel::OnMouseDoublePressed(MouseCode code)
{
	if (IsCapturing())
	{
		if (GetParent())
			GetParent()->OnMouseDoublePressed(code);
		return;
	}
	// Canonical path is CItemButton → ItemDoubleLeftClick → BeginCapture.
	// Do not PostMessage ChangeKey here (that was a second BeginCapture path).
	BaseClass::OnMouseDoublePressed(code);
}

void VControlsListPanel::OnMouseWheeled(int delta)
{
	if (IsCapturing() && GetParent())
	{
		GetParent()->OnMouseWheeled(delta);
		return;
	}
	BaseClass::OnMouseWheeled(delta);
}

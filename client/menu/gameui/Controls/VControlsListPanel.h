#pragma once

#include <vgui_controls/SectionedListPanel.h>

class VControlsListPanel : public vgui2::SectionedListPanel
{
	DECLARE_CLASS_SIMPLE(VControlsListPanel, vgui2::SectionedListPanel);

public:
	VControlsListPanel(vgui2::Panel *parent, const char *listName);
	~VControlsListPanel() override;

	// column: 1 = Key/Primary, 2 = Alternate (matches section columns).
	void StartCaptureMode(int column = 1, vgui2::HCursor hCursor = 0);
	void EndCaptureMode(vgui2::HCursor hCursor = 0);
	bool IsCapturing() const;
	void SetItemOfInterest(int itemID);
	int GetItemOfInterest() const;
	int GetCaptureColumn() const { return m_nCaptureColumn; }

	// Returns 1 (Key) or 2 (Alt) for the column under the cursor, else preferredDefault.
	int ResolveCaptureColumnAtCursor(int itemID, int preferredDefault = 1) const;
	bool AreVisibleChildrenContained() const;

	void OnMousePressed(vgui2::MouseCode code) override;
	void OnMouseDoublePressed(vgui2::MouseCode code) override;
	void OnMouseWheeled(int delta) override;
	void OnKeyCodeTyped(vgui2::KeyCode code) override;
	void OnKeyCodePressed(vgui2::KeyCode code) override;

protected:
	void ApplySchemeSettings(vgui2::IScheme *pScheme) override;

private:
	vgui2::Panel *m_pInlineEditPanel = nullptr;
	bool m_bCaptureMode = false;
	int m_nClickRow = 0;
	int m_nCaptureColumn = 1;
	vgui2::HFont m_hFont = vgui2::INVALID_FONT;
	int m_iMouseX = 0;
	int m_iMouseY = 0;
};

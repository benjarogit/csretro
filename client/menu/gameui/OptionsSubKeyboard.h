#pragma once

#include "OptionsAdaptiveLayout.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/PropertyPage.h"

#include <string>
#include <vector>

class VControlsListPanel;

class COptionsSubKeyboard : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE(COptionsSubKeyboard, vgui2::PropertyPage);

public:
	COptionsSubKeyboard(vgui2::Panel *parent);
	~COptionsSubKeyboard() override;

	void OnResetData() override;
	void OnApplyChanges() override;
	void OnPageShow() override;
	void PerformLayout() override;

	void OnKeyCodePressed(vgui2::KeyCode code) override;
	void OnKeyCodeTyped(vgui2::KeyCode code) override;
	void OnMousePressed(vgui2::MouseCode code) override;
	void OnMouseDoublePressed(vgui2::MouseCode code) override;
	void OnMouseWheeled(int delta) override;

	MESSAGE_FUNC_INT(ItemSelected, "ItemSelected", itemID);
	MESSAGE_FUNC_INT(ItemDoubleLeftClick, "ItemDoubleLeftClick", itemID);
	MESSAGE_FUNC(OnDefaultsOK, "DefaultsOK");
	MESSAGE_FUNC(OnReplaceOK, "ReplaceOK");
	MESSAGE_FUNC(OnReplaceCancel, "ReplaceCancel");

	bool IsCapturing() const;
	bool OnRawXashKey(int keynum, bool down); // true = consumed
	void CancelCapture();

	// Gate helpers
	int Gate_VisibleActionCount() const;
	bool Gate_SelectActionByBinding(const char *binding);
	bool Gate_StartCapture();
	bool Gate_StartCaptureColumn(int column); // 1=Primary 2=Alt
	bool Gate_ClearSelected();
	bool Gate_IsCapturing() const;
	unsigned Gate_InitiatingMouseMask() const;
	bool Gate_FinishCaptureKeynum(int keynum);
	const char *Gate_SelectedPrimaryKey() const;
	const char *Gate_SelectedAltKey() const;
	int Gate_ScrollValue() const;
	int Gate_ScrollRange() const;
	bool Gate_ListScreenCenter(int &x, int &y) const;
	void Gate_ListBounds(int &x, int &y, int &w, int &h) const;
	bool Gate_ListVisibleChildrenContained() const;
	bool Gate_OpenDefaultsQuery();
	bool Gate_HasQueryBox() const;
	bool Gate_DismissQueryBox();

private:
	enum class ActionAvail : int
	{
		Implemented = 0,
		Compatibility,
		Unavailable,
	};

	struct StagedKey
	{
		std::string binding; // empty = unbound
		bool dirty = false;
		// 0 = auto (classic primary-then-alt), 1 = Primary, 2 = Alternate
		char slotHint = 0;
	};

	void OnCommand(const char *command) override;
	void CreateKeyBindingList();
	void ParseActionDescriptions();
	void AppendActionsFromFile(const char *path, bool isOverlay);
	void FillInCurrentBindings();
	void FillInDefaultBindings();
	void ClearBindItems();
	void ApplyDiffBindings();
	void SnapshotEngine();
	bool HasPendingChanges() const;
	void RefreshColumnWidths();
	void CaptureDesignRects();
	void OwnClassicControlPins();
	void ApplyAdaptiveListLayout();
	vgui2::Panel *FindVisibleQueryBox() const;

	void AddBindingToItem(KeyValues *item, int keynum, int preferredColumn = 0);
	void RemoveKeyFromItems(int keynum);
	KeyValues *GetItemForBinding(const char *binding);
	void FinishCapture(int keynum);
	void ApplyCapturedKey(int keynum, KeyValues *item, const char *binding, int column);
	void PromptReplace(int keynum, const char *existingBinding, const char *newActionLabel);
	void CaptureDebug(const char *tag, int keynum, const char *extra = nullptr) const;
	void MarkDirty();
	void BeginCapture(int preferredColumn = -1); // -1 = Edit-key heuristic; 1=Primary 2=Alt
	void ArmInitiatingMouseGate();
	void ShowCaptureWaitingState();
	void RestoreCaptureSlotText();
	int ResolveEditKeyColumn(KeyValues *item) const;
	void RunBindingAudit() const;
	bool ShouldSuppressCaptureMouse(vgui2::MouseCode code) const;
	void NoteCaptureMouseReleased(vgui2::MouseCode code);

	ActionAvail ClassifyAction(const char *binding) const;
	static int MouseCodeToKeynum(vgui2::MouseCode code);
	static int MouseCodeBit(vgui2::MouseCode code);

	VControlsListPanel *m_pKeyBindList = nullptr;
	vgui2::Button *m_pSetBindingButton = nullptr;
	vgui2::Button *m_pClearBindingButton = nullptr;

	std::vector<std::string> m_snapshot;
	std::vector<StagedKey> m_stage;
	int m_pendingReplaceKeynum = -1;
	int m_pendingReplaceItem = -1;
	int m_pendingReplaceColumn = 1;

	// Bits of MouseCode that started Capture (Edit/DblClick) — must not become the binding.
	unsigned m_initiatingMouseMask = 0;

	char m_captureSavedKey[64]{};
	char m_captureSavedAlt[64]{};
	int m_captureItemId = -1;

	int m_sectionIndex = 0;
	int m_lastColumnWide = 0;

	CsretroOptionsLayout::Rect m_designList;
	CsretroOptionsLayout::Rect m_designDefaults;
	CsretroOptionsLayout::Rect m_designChange;
	CsretroOptionsLayout::Rect m_designClear;
};

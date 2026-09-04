#pragma once

#include <vgui_controls/ComboBox.h>
#include "tier1/utlvector.h"

// NextClient GameUI control: ComboBox item → engine command on Apply.
class CLabeledCommandComboBox : public vgui2::ComboBox
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CLabeledCommandComboBox, vgui2::ComboBox);

public:
	CLabeledCommandComboBox(vgui2::Panel *parent, const char *panelName);
	~CLabeledCommandComboBox() override = default;

	void DeleteAllItems();
	using vgui2::ComboBox::AddItem;
	void AddItem(const char *text, const char *engineCommand);
	void ActivateCommandItem(int itemIndex);

	const char *GetActiveItemCommand();
	void SetInitialItem(int itemIndex);
	void ApplyChanges();
	void Reset();
	bool HasBeenModified();
	int GetCurrentSelection() const { return m_iCurrentSelection; }
	void MarkApplied(); // m_iStartSelection = current (ohne Engine-Cmd)

	enum
	{
		MAX_NAME_LEN = 256,
		MAX_COMMAND_LEN = 256
	};

private:
	MESSAGE_FUNC_CHARPTR(OnTextChanged, "TextChanged", text);

	struct COMMANDITEM
	{
		char name[MAX_NAME_LEN];
		char command[MAX_COMMAND_LEN];
		int comboBoxID;
	};

	CUtlVector<COMMANDITEM> m_Items;
	int m_iCurrentSelection = -1;
	int m_iStartSelection = -1;
};

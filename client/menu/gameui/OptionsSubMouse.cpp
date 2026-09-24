#include "OptionsSubMouse.h"

#include "Controls/CvarNegateCheckButton.h"
#include "Controls/CvarSlider.h"
#include "Controls/CvarToggleCheckButton.h"
#include "Controls/KeyToggleCheckButton.h"
#include "Controls/MenuEngine.h"

#include "tier1/KeyValues.h"
#include "tier1/strtools.h"
#include "vgui/ISchemeNext.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/TextEntry.h"

#include <algorithm>
#include <cstdlib>

using namespace vgui2;

COptionsSubMouse::COptionsSubMouse(Panel *parent) : PropertyPage(parent, "OptionsSubMouse")
{
	// NextClient-Struktur; CVars gegen Xash/CS-Retro geprüft:
	// m_pitch, m_rawinput, sensitivity, joystick, sv_aim, in_mlook/mlook, in_jlook/jlook — vorhanden.
	// m_filter (GoldSrc/NextClient) → look_filter (Xash Engine).
	m_pReverseMouseCheckBox = new CCvarNegateCheckButton(this, "ReverseMouse", "#GameUI_ReverseMouse", "m_pitch");
	m_pMouseLookCheckBox = new CKeyToggleCheckButton(this, "MouseLook", "#GameUI_MouseLook", "in_mlook", "mlook");
	m_pMouseFilterCheckBox = new CCvarToggleCheckButton(this, "MouseFilter", "#GameUI_MouseFilter", "look_filter");
	m_pMouseRawInputCheckBox = new CCvarToggleCheckButton(this, "RawInput", "#GameUI_RawInput", "m_rawinput");
	m_pWeaponLagCheckBox = new CCvarToggleCheckButton(this, "WeaponLag", "Weapon lag", "cl_weaponlag");
	m_pFastSwitchCheckBox = new CCvarToggleCheckButton(this, "FastSwitch", "#CsretroGameUI_FastSwitch", "hud_fastswitch");
	m_pAutoWeaponSwitchCheckBox = new CCvarToggleCheckButton(this, "AutoWeaponSwitch", "#CsretroGameUI_AutoWeaponSwitch", "_cl_autowepswitch");
	m_pJoystickCheckBox = new CCvarToggleCheckButton(this, "Joystick", "#GameUI_Joystick", "joystick");
	m_pJoystickLookCheckBox = new CKeyToggleCheckButton(this, "JoystickLook", "#GameUI_JoystickLook", "in_jlook", "jlook");
	m_pMouseSensitivitySlider = new CCvarSlider(this, "Slider", "#GameUI_MouseSensitivity", 0.2f, 20.0f, "sensitivity");
	m_pMouseSensitivityLabel = new TextEntry(this, "SensitivityLabel");
	m_pMouseSensitivityLabel->AddActionSignalTarget(this);
	m_pAutoAimCheckBox = new CCvarToggleCheckButton(this, "Auto-Aim", "#GameUI_AutoAim", "sv_aim");

	LoadControlSettings("resource/OptionsSubMouse.res");
	UpdateSensitivityLabel(MenuEngine::GetCvarFloat("sensitivity"));
}

COptionsSubMouse::~COptionsSubMouse() = default;

void COptionsSubMouse::OnPageShow()
{
	UpdateSensitivityLabel(m_pMouseSensitivitySlider->GetSliderValue());
}

void COptionsSubMouse::OnResetData()
{
	m_pReverseMouseCheckBox->Reset();
	m_pMouseLookCheckBox->Reset();
	m_pMouseFilterCheckBox->Reset();
	m_pMouseRawInputCheckBox->Reset();
	m_pWeaponLagCheckBox->Reset();
	m_pFastSwitchCheckBox->Reset();
	m_pAutoWeaponSwitchCheckBox->Reset();
	m_pJoystickCheckBox->Reset();
	m_pJoystickLookCheckBox->Reset();
	m_pMouseSensitivitySlider->Reset();
	m_pAutoAimCheckBox->Reset();
	UpdateSensitivityLabel(m_pMouseSensitivitySlider->GetSliderValue());
}

void COptionsSubMouse::OnApplyChanges()
{
	m_pReverseMouseCheckBox->ApplyChanges();
	m_pMouseLookCheckBox->ApplyChanges();
	m_pMouseFilterCheckBox->ApplyChanges();
	m_pMouseRawInputCheckBox->ApplyChanges();
	m_pWeaponLagCheckBox->ApplyChanges();
	m_pFastSwitchCheckBox->ApplyChanges();
	m_pAutoWeaponSwitchCheckBox->ApplyChanges();
	m_pJoystickCheckBox->ApplyChanges();
	m_pJoystickLookCheckBox->ApplyChanges();
	m_pAutoAimCheckBox->ApplyChanges();
	BoundSensitivityValue();
	m_pMouseSensitivitySlider->ApplyChanges();
}

void COptionsSubMouse::BoundSensitivityValue()
{
	const float fValue = std::clamp(m_pMouseSensitivitySlider->GetSliderValue(), 0.2f, 20.0f);
	m_pMouseSensitivitySlider->SetSliderValue(fValue);
	UpdateSensitivityLabel(fValue);
}

void COptionsSubMouse::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	m_pMouseSensitivityLabel->SetBorder(pScheme->GetBorder("ButtonDepressedBorder"));
}

void COptionsSubMouse::PerformLayout()
{
	BaseClass::PerformLayout();

	struct Row
	{
		Panel *control;
		const char *descriptionName;
	};
	const Row rows[] = {
		{m_pReverseMouseCheckBox, "Reverse Mouse label"},
		{m_pMouseLookCheckBox, "Label1"},
		{m_pMouseFilterCheckBox, "Mouse filter"},
		{m_pJoystickCheckBox, "Joystick label"},
		{m_pJoystickLookCheckBox, "Label2"},
		{m_pAutoAimCheckBox, "AutoaimLabel"},
		{m_pMouseRawInputCheckBox, "RawInputLabel"},
		{m_pWeaponLagCheckBox, nullptr},
		{m_pFastSwitchCheckBox, "FastSwitchLabel"},
		{m_pAutoWeaponSwitchCheckBox, "AutoWeaponSwitchLabel"},
	};

	int wide = 0;
	int tall = 0;
	GetSize(wide, tall);
	const int left = 36;
	const int right = 32;
	const int gap = 14;
	const int available = std::max(120, wide - left - right);
	const int labelWide = std::min(210, std::max(155, available * 2 / 5));
	const bool stacked = available < 480;
	int y = 32;

	for (const Row &row : rows)
	{
		Label *description = row.descriptionName ? dynamic_cast<Label *>(FindChildByName(row.descriptionName)) : nullptr;
		if (description)
			description->SetWrap(stacked);

		if (stacked && description)
		{
			row.control->SetBounds(left, y, available, 24);
			description->SetBounds(left + 20, y + 22, available - 20, 32);
			y += 56;
		}
		else
		{
			row.control->SetBounds(left, y, labelWide, 24);
			if (description)
				description->SetBounds(left + labelWide + gap, y + 2, available - labelWide - gap, 24);
			y += 26;
		}
	}

	Panel *sensitivityCaption = FindChildByName("Label3");
	if (sensitivityCaption)
		sensitivityCaption->SetBounds(left + 4, y + 6, 164, 24);
	m_pMouseSensitivitySlider->SetBounds(left + 4, y + 28, std::max(120, available - 96), 40);
	m_pMouseSensitivityLabel->SetBounds(left + available - 84, y + 28, 48, 24);
}

void COptionsSubMouse::OnControlModified(Panel *panel)
{
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
	if (panel == m_pMouseSensitivitySlider)
		UpdateSensitivityLabel(m_pMouseSensitivitySlider->GetSliderValue());
}

void COptionsSubMouse::OnCvarChanged(Panel *panel)
{
	if (panel == m_pMouseSensitivitySlider)
		UpdateSensitivityLabel(m_pMouseSensitivitySlider->GetSliderValue());
}

void COptionsSubMouse::OnTextChanged(Panel *panel)
{
	if (panel != m_pMouseSensitivityLabel)
		return;
	char buf[64];
	m_pMouseSensitivityLabel->GetText(buf, 64);
	m_pMouseSensitivitySlider->SetSliderValue(static_cast<float>(atof(buf)));
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

void COptionsSubMouse::OnCheckButtonChecked(Panel *panel)
{
	OnControlModified(panel);
}

void COptionsSubMouse::UpdateSensitivityLabel(float value)
{
	char buf[64];
	Q_snprintf(buf, sizeof(buf), " %.2f", value);
	m_pMouseSensitivityLabel->SetText(buf);
}

void COptionsSubMouse::Gate_SetSensitivityPending(float value)
{
	m_pMouseSensitivitySlider->SetSliderValue(value);
	UpdateSensitivityLabel(value);
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

float COptionsSubMouse::Gate_GetSensitivityPending() const
{
	return m_pMouseSensitivitySlider->GetSliderValue();
}

void COptionsSubMouse::Gate_SetFilterPending(bool on)
{
	m_pMouseFilterCheckBox->SetSelected(on);
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

bool COptionsSubMouse::Gate_GetFilterPending() const
{
	return m_pMouseFilterCheckBox->IsSelected();
}

void COptionsSubMouse::Gate_SetRawInputPending(bool on)
{
	m_pMouseRawInputCheckBox->SetSelected(on);
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

void COptionsSubMouse::Gate_SetReverseMousePending(bool on)
{
	m_pReverseMouseCheckBox->SetSelected(on);
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

void COptionsSubMouse::Gate_SyncLabelFromSlider()
{
	UpdateSensitivityLabel(m_pMouseSensitivitySlider->GetSliderValue());
}

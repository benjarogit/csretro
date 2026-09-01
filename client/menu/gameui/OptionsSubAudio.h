#pragma once

#include <vgui_controls/PropertyPage.h>

class CCvarSlider;
class CLabeledCommandComboBox;

class COptionsSubAudio : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE(COptionsSubAudio, vgui2::PropertyPage);

public:
	explicit COptionsSubAudio(vgui2::Panel *parent);
	~COptionsSubAudio() override;

	void OnResetData() override;
	void OnApplyChanges() override;

	// Functional gate (CSRETRO_OPTIONS_AUDIO_GATE)
	void Gate_SetVolumePending(float value);
	float Gate_GetVolumePending() const;
	void Gate_SetMp3Pending(float value);
	float Gate_GetMp3Pending() const;
	void Gate_SetQualityHigh(bool high);
	bool Gate_IsQualityHigh() const;

private:
	MESSAGE_FUNC(OnControlModified, "ControlModified");

	CCvarSlider *m_pSFXSlider = nullptr;
	CCvarSlider *m_pHEVSlider = nullptr;
	CCvarSlider *m_pMP3Slider = nullptr;
	CLabeledCommandComboBox *m_pSoundQualityCombo = nullptr;
};

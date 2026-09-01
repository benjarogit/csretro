#pragma once

#include <vgui_controls/Slider.h>

class CCvarSlider : public vgui2::Slider
{
	DECLARE_CLASS_SIMPLE(CCvarSlider, vgui2::Slider);

public:
	CCvarSlider(vgui2::Panel *parent, const char *panelName);
	CCvarSlider(vgui2::Panel *parent, const char *panelName, const char *caption, float minValue,
		float maxValue, const char *cvarname, bool bAllowOutOfRange = false);
	~CCvarSlider() override;

	void SetupSlider(float minValue, float maxValue, const char *cvarname, bool bAllowOutOfRange);
	void SetCVarName(const char *cvarname);
	void SetMinMaxValues(float minValue, float maxValue, bool bSetTickDisplay = true);
	void SetTickColor(Color color);

	void Paint() override;
	void ApplySettings(KeyValues *inResourceData) override;
	void GetSettings(KeyValues *outResourceData) override;

	void ApplyChanges();
	float GetSliderValue();
	void SetSliderValue(float fValue);
	void Reset();
	bool HasBeenModified();

private:
	MESSAGE_FUNC(OnSliderMoved, "SliderMoved");
	MESSAGE_FUNC(OnApplyChanges, "ApplyChanges");

	bool m_bAllowOutOfRange = false;
	bool m_bModifiedOnce = false;
	float m_fStartValue = 0.f;
	int m_iStartValue = 0;
	int m_iLastSliderValue = 0;
	float m_fCurrentValue = 0.f;
	char m_szCvarName[64]{};
	bool m_bCreatedInCode = false;
	float m_flMinValue = 0.f;
	float m_flMaxValue = 1.f;
};

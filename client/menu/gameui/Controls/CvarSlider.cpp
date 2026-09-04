#include "CvarSlider.h"
#include "MenuEngine.h"

#include "tier1/KeyValues.h"
#include "tier1/strtools.h"
#include "vgui_controls/PropertyPage.h"

#define CVARSLIDER_SCALE_FACTOR 100.0f

using namespace vgui2;

DECLARE_BUILD_FACTORY(CCvarSlider);

CCvarSlider::CCvarSlider(Panel *parent, const char *name) : Slider(parent, name)
{
	SetupSlider(0, 1, "", false);
	m_bCreatedInCode = false;
	AddActionSignalTarget(this);
}

CCvarSlider::CCvarSlider(Panel *parent, const char *panelName, const char *caption, float minValue,
	float maxValue, const char *cvarname, bool bAllowOutOfRange)
	: Slider(parent, panelName)
{
	(void)caption;
	AddActionSignalTarget(this);
	if (parent)
		AddActionSignalTarget(parent);
	SetupSlider(minValue, maxValue, cvarname, bAllowOutOfRange);
	m_bCreatedInCode = true;
}

void CCvarSlider::SetupSlider(float minValue, float maxValue, const char *cvarname, bool bAllowOutOfRange)
{
	m_flMinValue = minValue;
	m_flMaxValue = maxValue;
	SetRange(static_cast<int>(CVARSLIDER_SCALE_FACTOR * minValue),
		static_cast<int>(CVARSLIDER_SCALE_FACTOR * maxValue));

	char szMin[32];
	char szMax[32];
	Q_snprintf(szMin, sizeof(szMin), "%.2f", minValue);
	Q_snprintf(szMax, sizeof(szMax), "%.2f", maxValue);
	SetTickCaptions(szMin, szMax);

	Q_strncpy(m_szCvarName, cvarname ? cvarname : "", sizeof(m_szCvarName));
	m_bModifiedOnce = false;
	m_bAllowOutOfRange = bAllowOutOfRange;
	Reset();
}

CCvarSlider::~CCvarSlider() = default;

void CCvarSlider::ApplySettings(KeyValues *inResourceData)
{
	BaseClass::ApplySettings(inResourceData);
	if (m_bCreatedInCode)
		return;

	const float minValue = inResourceData->GetFloat("minvalue", 0);
	const float maxValue = inResourceData->GetFloat("maxvalue", 1);
	const char *cvarname = inResourceData->GetString("cvar_name", "");
	const bool bAllowOutOfRange = inResourceData->GetInt("allowoutofrange", 0) != 0;
	SetupSlider(minValue, maxValue, cvarname, bAllowOutOfRange);

	if (GetParent())
	{
		if (dynamic_cast<PropertyPage *>(GetParent()) && GetParent()->GetParent())
			GetParent()->GetParent()->AddActionSignalTarget(this);
		else
			GetParent()->AddActionSignalTarget(this);
	}
}

void CCvarSlider::GetSettings(KeyValues *outResourceData)
{
	BaseClass::GetSettings(outResourceData);
	if (m_bCreatedInCode)
		return;
	outResourceData->SetFloat("minvalue", m_flMinValue);
	outResourceData->SetFloat("maxvalue", m_flMaxValue);
	outResourceData->SetString("cvar_name", m_szCvarName);
	outResourceData->SetInt("allowoutofrange", m_bAllowOutOfRange ? 1 : 0);
}

void CCvarSlider::SetCVarName(const char *cvarname)
{
	Q_strncpy(m_szCvarName, cvarname ? cvarname : "", sizeof(m_szCvarName));
	m_bModifiedOnce = false;
	Reset();
}

void CCvarSlider::SetMinMaxValues(float minValue, float maxValue, bool bSetTickDisplay)
{
	SetRange(static_cast<int>(CVARSLIDER_SCALE_FACTOR * minValue),
		static_cast<int>(CVARSLIDER_SCALE_FACTOR * maxValue));
	if (bSetTickDisplay)
	{
		char szMin[32];
		char szMax[32];
		Q_snprintf(szMin, sizeof(szMin), "%.2f", minValue);
		Q_snprintf(szMax, sizeof(szMax), "%.2f", maxValue);
		SetTickCaptions(szMin, szMax);
	}
	Reset();
}

void CCvarSlider::SetTickColor(Color color)
{
	m_TickColor = color;
}

void CCvarSlider::Paint()
{
	if (m_szCvarName[0])
	{
		const float curvalue = MenuEngine::GetCvarFloat(m_szCvarName);
		if (curvalue != m_fStartValue)
		{
			const int val = static_cast<int>(CVARSLIDER_SCALE_FACTOR * curvalue);
			m_fStartValue = curvalue;
			m_fCurrentValue = curvalue;
			SetValue(val, false);
			m_iStartValue = GetValue();
			m_iLastSliderValue = m_iStartValue;
			PostActionSignal(new KeyValues("CvarChanged"));
		}
	}
	BaseClass::Paint();
}

void CCvarSlider::ApplyChanges()
{
	if (!m_bModifiedOnce || !m_szCvarName[0])
		return;

	m_iStartValue = GetValue();
	if (m_bAllowOutOfRange)
		m_fStartValue = m_fCurrentValue;
	else
		m_fStartValue = static_cast<float>(m_iStartValue) / CVARSLIDER_SCALE_FACTOR;

	char value[128];
	Q_snprintf(value, sizeof(value), "%.2f", m_fStartValue);
	MenuEngine::CvarSet(m_szCvarName, value);
}

float CCvarSlider::GetSliderValue()
{
	if (m_bAllowOutOfRange)
		return m_fCurrentValue;
	return static_cast<float>(GetValue()) / CVARSLIDER_SCALE_FACTOR;
}

void CCvarSlider::SetSliderValue(float fValue)
{
	const int nVal = static_cast<int>(CVARSLIDER_SCALE_FACTOR * fValue);
	SetValue(nVal, false);
	m_iLastSliderValue = GetValue();
	if (m_fCurrentValue != fValue)
	{
		m_fCurrentValue = fValue;
		m_bModifiedOnce = true;
	}
}

void CCvarSlider::Reset()
{
	if (!m_szCvarName[0])
		return;
	m_fStartValue = MenuEngine::GetCvarFloat(m_szCvarName);
	m_fCurrentValue = m_fStartValue;
	const int value = static_cast<int>(CVARSLIDER_SCALE_FACTOR * m_fStartValue);
	SetValue(value, false);
	m_iStartValue = GetValue();
	m_iLastSliderValue = m_iStartValue;
	m_bModifiedOnce = false;
}

bool CCvarSlider::HasBeenModified()
{
	if (GetValue() != m_iStartValue)
		m_bModifiedOnce = true;
	return m_bModifiedOnce;
}

void CCvarSlider::OnSliderMoved()
{
	if (!HasBeenModified())
		return;
	if (m_iLastSliderValue != GetValue())
	{
		m_iLastSliderValue = GetValue();
		m_fCurrentValue = static_cast<float>(m_iLastSliderValue) / CVARSLIDER_SCALE_FACTOR;
	}
	PostActionSignal(new KeyValues("ControlModified"));
}

void CCvarSlider::OnApplyChanges()
{
	if (!m_bCreatedInCode)
		ApplyChanges();
}

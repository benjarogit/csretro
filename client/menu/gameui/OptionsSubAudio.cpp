#include "OptionsSubAudio.h"

#include "Controls/CvarSlider.h"
#include "Controls/LabeledCommandComboBox.h"
#include "Controls/MenuEngine.h"

#include "tier1/KeyValues.h"
#include "vgui_controls/Label.h"

using namespace vgui2;

namespace
{
// GoldSrc/NextClient: hisound 0|1 (binary).
// Xash: room_hires 1=22k, 2=44k (empfohlen), 3=96k — CVar-Name „room_hires“ (Variable hisound in s_dsp.c).
constexpr const char *kQualityHighCmd = "room_hires 2\n";
constexpr const char *kQualityLowCmd = "room_hires 1\n";
}

COptionsSubAudio::COptionsSubAudio(Panel *parent) : PropertyPage(parent, "OptionsSubAudio")
{
	// s_eax / s_a3d: GoldSrc/Miles — unter Xash nicht vorhanden → keine toten Controls.
	m_pSFXSlider = new CCvarSlider(this, "SFX Slider", "#GameUI_SoundEffectVolume", 0.0f, 2.0f, "volume");
	m_pHEVSlider = new CCvarSlider(this, "Suit Slider", "#GameUI_HEVSuitVolume", 0.0f, 2.0f, "suitvolume");
	// NextClient „mp3volume“ → Xash registriert „MP3Volume“ (case-insensitive Lookup).
	m_pMP3Slider = new CCvarSlider(this, "MP3 Volume", "#GameUI_MP3Volume", 0.0f, 1.0f, "MP3Volume");

	m_pSoundQualityCombo = new CLabeledCommandComboBox(this, "Sound Quality");
	m_pSoundQualityCombo->AddItem("#GameUI_High", kQualityHighCmd);
	m_pSoundQualityCombo->AddItem("#GameUI_Low", kQualityLowCmd);
	const float hires = MenuEngine::GetCvarFloat("room_hires");
	m_pSoundQualityCombo->SetInitialItem(hires >= 2.0f ? 0 : 1);

	LoadControlSettings("resource/OptionsSubAudio.res");

	// CS ist Multiplayer-only: HEV-Suit-Volume ausblenden (wie NextClient ModInfo).
	if (Panel *child = FindChildByName("suit label"))
		child->SetVisible(false);
	if (m_pHEVSlider)
		m_pHEVSlider->SetVisible(false);

	// Miles-Branding gehört nicht zur Xash-Audio-Runtime.
	if (Panel *miles = FindChildByName("MilesAudioLabel"))
		miles->SetVisible(false);
}

COptionsSubAudio::~COptionsSubAudio() = default;

void COptionsSubAudio::OnResetData()
{
	m_pSFXSlider->Reset();
	if (m_pHEVSlider && m_pHEVSlider->IsVisible())
		m_pHEVSlider->Reset();
	m_pMP3Slider->Reset();
	// Quality immer aus Runtime lesen (Ctor kann vor Sound-Init laufen).
	const float hires = MenuEngine::GetCvarFloat("room_hires");
	m_pSoundQualityCombo->SetInitialItem(hires >= 2.0f ? 0 : 1);
}

void COptionsSubAudio::OnApplyChanges()
{
	m_pSFXSlider->ApplyChanges();
	if (m_pHEVSlider && m_pHEVSlider->IsVisible())
		m_pHEVSlider->ApplyChanges();
	m_pMP3Slider->ApplyChanges();
	// Sofortiges CVar-Set (ClientCmd kann einen Frame verzögern); High=2 / Low=1.
	const int sel = m_pSoundQualityCombo->GetCurrentSelection();
	if (sel == 0)
		MenuEngine::CvarSetValue("room_hires", 2.0f);
	else if (sel == 1)
		MenuEngine::CvarSetValue("room_hires", 1.0f);
	m_pSoundQualityCombo->MarkApplied();
}

void COptionsSubAudio::OnControlModified()
{
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

void COptionsSubAudio::Gate_SetVolumePending(float value)
{
	m_pSFXSlider->SetSliderValue(value);
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

float COptionsSubAudio::Gate_GetVolumePending() const
{
	return m_pSFXSlider->GetSliderValue();
}

void COptionsSubAudio::Gate_SetMp3Pending(float value)
{
	m_pMP3Slider->SetSliderValue(value);
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

float COptionsSubAudio::Gate_GetMp3Pending() const
{
	return m_pMP3Slider->GetSliderValue();
}

void COptionsSubAudio::Gate_SetQualityHigh(bool high)
{
	m_pSoundQualityCombo->ActivateCommandItem(high ? 0 : 1);
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

bool COptionsSubAudio::Gate_IsQualityHigh() const
{
	return m_pSoundQualityCombo->GetCurrentSelection() == 0;
}

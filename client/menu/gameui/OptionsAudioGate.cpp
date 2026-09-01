#include "OptionsAudioGate.h"
#include "OptionsDialog.h"
#include "OptionsSubAudio.h"
#include "Controls/MenuEngine.h"

#include "vgui_controls/PropertySheet.h"

#include "../src/menu_priv.h"

#include <cmath>
#include <cstring>

namespace
{
bool Near(float a, float b, float eps = 0.05f)
{
	return std::fabs(a - b) <= eps;
}

void Expect(bool ok, const char *tag)
{
	Menu_Con(ok ? "CSRETRO_AUDIO_GATE_OK %s" : "CSRETRO_AUDIO_GATE_FAIL %s", tag);
}

COptionsSubAudio *AudioPage(COptionsDialog *dialog)
{
	if (!dialog || !dialog->GetPropertySheet())
		return nullptr;
	return dynamic_cast<COptionsSubAudio *>(dialog->GetPropertySheet()->GetActivePage());
}
} // namespace

void OptionsAudio_RunFunctionalGate(COptionsDialog *dialog)
{
	if (!dialog)
	{
		Menu_Con("CSRETRO_AUDIO_GATE_FAIL no_dialog");
		return;
	}

	dialog->OpenTab("Audio");
	COptionsSubAudio *audio = AudioPage(dialog);
	if (!audio)
	{
		Menu_Con("CSRETRO_AUDIO_GATE_FAIL no_audio_page");
		return;
	}

	MenuEngine::CvarSetValue("volume", 0.7f);
	MenuEngine::CvarSetValue("MP3Volume", 0.5f);
	MenuEngine::ClientCmd("room_hires 2\n");

	dialog->ResetAllData();
	Expect(Near(audio->Gate_GetVolumePending(), 0.7f), "open_reads_volume");
	Expect(Near(audio->Gate_GetMp3Pending(), 0.5f), "open_reads_mp3");
	Expect(audio->Gate_IsQualityHigh(), "open_reads_quality_high");
	Expect(Near(MenuEngine::GetCvarFloat("room_hires"), 2.0f, 0.1f), "cvar_room_hires");

	audio->Gate_SetVolumePending(1.25f);
	audio->Gate_SetMp3Pending(0.8f);
	audio->Gate_SetQualityHigh(false);
	audio->OnApplyChanges();
	Expect(Near(MenuEngine::GetCvarFloat("volume"), 1.25f), "apply_volume");
	Expect(Near(MenuEngine::GetCvarFloat("MP3Volume"), 0.8f), "apply_mp3");
	Expect(Near(MenuEngine::GetCvarFloat("room_hires"), 1.0f, 0.1f), "apply_room_hires_low");
	dialog->Gate_Apply();
	Expect(dialog->IsVisible(), "apply_keeps_open");

	MenuEngine::CvarSetValue("volume", 0.4f);
	dialog->ResetAllData();
	audio->Gate_SetVolumePending(1.8f);
	dialog->Gate_Cancel();
	Expect(Near(MenuEngine::GetCvarFloat("volume"), 0.4f), "cancel_discards");

	dialog->Activate();
	dialog->OpenTab("Audio");
	audio = AudioPage(dialog);
	Expect(audio != nullptr, "reopen_audio");
	if (audio)
	{
		dialog->ResetAllData();
		audio->Gate_SetVolumePending(0.55f);
		audio->Gate_SetQualityHigh(true);
		dialog->Gate_OK();
		Expect(Near(MenuEngine::GetCvarFloat("volume"), 0.55f), "ok_applies");
		Expect(Near(MenuEngine::GetCvarFloat("room_hires"), 2.0f, 0.1f), "ok_quality_high");
		Expect(!dialog->IsVisible(), "ok_closes");
	}

	dialog->Activate();
	dialog->OpenTab("Audio");
	audio = AudioPage(dialog);
	if (audio)
	{
		dialog->ResetAllData();
		Expect(Near(audio->Gate_GetVolumePending(), 0.55f), "reopen_shows_applied");
	}

	MenuEngine::CvarSetValue("volume", 0.33f);
	dialog->ResetAllData();
	if (audio)
		Expect(Near(audio->Gate_GetVolumePending(), 0.33f), "reset_from_runtime");

	MenuEngine::ClientCmd("host_writeconfig\n");
	Menu_Con("CSRETRO_AUDIO_GATE_WRITECONFIG");
	Menu_Con("CSRETRO_AUDIO_CVAR volume=%g", MenuEngine::GetCvarFloat("volume"));
	Menu_Con("CSRETRO_AUDIO_CVAR MP3Volume=%g", MenuEngine::GetCvarFloat("MP3Volume"));
	Menu_Con("CSRETRO_AUDIO_CVAR room_hires=%g", MenuEngine::GetCvarFloat("room_hires"));
	Menu_Con("CSRETRO_AUDIO_CVAR suitvolume=%g", MenuEngine::GetCvarFloat("suitvolume"));
	Menu_Con("CSRETRO_AUDIO_CVAR s_eax_absent=%d", MenuEngine::GetCvarString("s_eax")[0] ? 0 : 1);
	Menu_Con("CSRETRO_AUDIO_CVAR s_a3d_absent=%d", MenuEngine::GetCvarString("s_a3d")[0] ? 0 : 1);

	dialog->Activate();
	dialog->OpenTab("Audio");
	MenuEngine::ClientCmd("screenshot\n");
	Menu_Con("CSRETRO_AUDIO_GATE_SHOT_READY");
	Menu_Con("CSRETRO_AUDIO_GATE_DONE");
}

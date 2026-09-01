#include "OptionsSubVideo.h"

#include "Controls/CvarSlider.h"
#include "Controls/CvarToggleCheckButton.h"
#include "Controls/MenuEngine.h"

#include "tier1/KeyValues.h"
#include "vgui/IVGui.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/QueryBox.h"

#include <chrono>
#include <cstdio>
#include <cstring>

using namespace vgui2;

namespace
{
double SteadyNow()
{
	using clock = std::chrono::steady_clock;
	return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

bool ParseModeWh(const char *desc, int &w, int &h)
{
	w = h = 0;
	if (!desc || !*desc)
		return false;
	return std::sscanf(desc, "%dx%d", &w, &h) == 2 || std::sscanf(desc, "%d x %d", &w, &h) == 2;
}
} // namespace

COptionsSubVideo::AspectFilter COptionsSubVideo::ClassifyAspect(int w, int h)
{
	if (w <= 0 || h <= 0)
		return kAspectOther;
	if (w * 3 == h * 4)
		return kAspect4x3;
	if (w * 4 == h * 5)
		return kAspect5x4;
	if (w * 9 == h * 16)
		return kAspect16x9;
	if (w * 10 == h * 16)
		return kAspect16x10;
	return kAspectOther;
}

COptionsSubVideo::COptionsSubVideo(Panel *parent) : PropertyPage(parent, "OptionsSubVideo")
{
	m_pBrightness = new CCvarSlider(this, "Brightness", "#GameUI_Brightness", 0.0f, 2.0f, "brightness");
	m_pGamma = new CCvarSlider(this, "Gamma", "#GameUI_Gamma", 1.0f, 3.0f, "gamma");
	m_pVSync = new CCvarToggleCheckButton(this, "VSync", "#GameUI_VSync", "gl_vsync");
	m_pDetailTextures = new CCvarToggleCheckButton(this, "DetailTextures", "#GameUI_DetailTextures", "r_detailtextures");

	m_pResolution = new ComboBox(this, "Resolution", 12, false);
	m_pAspectRatio = new ComboBox(this, "AspectRatio", 6, false);
	m_pDisplayMode = new ComboBox(this, "DisplayMode", 3, false);
	m_pRenderer = new ComboBox(this, "Renderer", 4, false);

	LoadControlSettings("resource/OptionsSubVideo.res");

	// GoldSrc-only / FileConfig / FOV — not offered without backend.
	static const char *kHide[] = {
		"Windowed", "HDModels", "AddonsFolder", "LowVideoDetail", "DisableMultitexture",
		"StretchAspect", "ColorDepth", "AdvancedVideo", "Label5",
	};
	for (const char *name : kHide)
	{
		if (Panel *p = FindChildByName(name))
			p->SetVisible(false);
	}

	m_bIgnoreTextChanged = true;
	m_pAspectRatio->RemoveAll();
	m_pAspectRatio->AddItem("#GameUI_AspectAll", new KeyValues("aspect", "id", kAspectAll));
	m_pAspectRatio->AddItem("#GameUI_Aspect4x3", new KeyValues("aspect", "id", kAspect4x3));
	m_pAspectRatio->AddItem("#GameUI_Aspect5x4", new KeyValues("aspect", "id", kAspect5x4));
	m_pAspectRatio->AddItem("#GameUI_Aspect16x9", new KeyValues("aspect", "id", kAspect16x9));
	m_pAspectRatio->AddItem("#GameUI_Aspect16x10", new KeyValues("aspect", "id", kAspect16x10));
	m_pAspectRatio->AddItem("#GameUI_AspectOther", new KeyValues("aspect", "id", kAspectOther));

	m_pDisplayMode->RemoveAll();
	m_pDisplayMode->AddItem("#GameUI_Windowed", new KeyValues("fs", "v", 0));
	m_pDisplayMode->AddItem("#GameUI_Fullscreen", new KeyValues("fs", "v", 1));
	m_pDisplayMode->AddItem("#GameUI_Borderless", new KeyValues("fs", "v", 2));

	if (Label *note = dynamic_cast<Label *>(FindChildByName("VideoNote")))
		m_pVideoNote = note;
	else
	{
		m_pVideoNote = new Label(this, "VideoNote", "#GameUI_VideoSoftRestart");
		m_pVideoNote->SetBounds(40, 280, 430, 40);
		m_pVideoNote->SetContentAlignment(Label::a_northwest);
	}

	m_pDetailTextures->SetVisible(true);

	RefreshRendererCombo();
	ReadAppliedFromEngine(m_applied);
	PrepareResolutionList();
	SelectCurrentResolution();
	const int fs = (m_applied.fullscreen < 0) ? 0 : (m_applied.fullscreen > 2 ? 2 : m_applied.fullscreen);
	m_pDisplayMode->ActivateItemByRow(fs);

	const AspectFilter start = ClassifyAspect(m_applied.w, m_applied.h);
	m_pAspectRatio->ActivateItemByRow(static_cast<int>(start == kAspectOther ? kAspectAll : start));
	m_bIgnoreTextChanged = false;

	ivgui()->AddTickSignal(GetVPanel(), 100);
}

COptionsSubVideo::~COptionsSubVideo()
{
	ivgui()->RemoveTickSignal(GetVPanel());
	if (m_pConfirm)
	{
		m_pConfirm->MarkForDeletion();
		m_pConfirm = nullptr;
	}
}

void COptionsSubVideo::OnPageShow()
{
	OnResetData();
}

void COptionsSubVideo::OnResetData()
{
	if (m_bConfirmOpen)
		return;
	m_pBrightness->Reset();
	m_pGamma->Reset();
	m_pVSync->Reset();
	m_pDetailTextures->Reset();
	ReadAppliedFromEngine(m_applied);
	m_bIgnoreTextChanged = true;
	PrepareResolutionList();
	SelectCurrentResolution();
	const int fs = (m_applied.fullscreen < 0) ? 0 : (m_applied.fullscreen > 2 ? 2 : m_applied.fullscreen);
	m_pDisplayMode->ActivateItemByRow(fs);
	RefreshRendererCombo();
	m_bIgnoreTextChanged = false;
}

void COptionsSubVideo::OnApplyChanges()
{
	if (m_bConfirmOpen)
		return;
	ApplyLiveCvars();
	ApplyModeChangesTransactional();
}

void COptionsSubVideo::ApplyLiveCvars()
{
	m_pBrightness->ApplyChanges();
	m_pGamma->ApplyChanges();
	m_pVSync->ApplyChanges();
	m_pDetailTextures->ApplyChanges();
}

void COptionsSubVideo::ReadAppliedFromEngine(VidSnapshot &out) const
{
	out.w = static_cast<int>(MenuEngine::GetCvarFloat("width"));
	out.h = static_cast<int>(MenuEngine::GetCvarFloat("height"));
	if (out.w <= 0)
		out.w = static_cast<int>(MenuEngine::GetCvarFloat("vid_width"));
	if (out.h <= 0)
		out.h = static_cast<int>(MenuEngine::GetCvarFloat("vid_height"));
	out.fullscreen = static_cast<int>(MenuEngine::GetCvarFloat("fullscreen"));
	const char *ref = MenuEngine::GetCvarString("r_refdll_loaded");
	if (!ref || !*ref)
		ref = MenuEngine::GetCvarString("r_refdll");
	std::snprintf(out.renderer, sizeof(out.renderer), "%s", ref ? ref : "");
}

void COptionsSubVideo::RefreshRendererCombo()
{
	if (!m_pRenderer)
		return;
	m_bIgnoreTextChanged = true;
	m_pRenderer->RemoveAll();
	const char *loaded = MenuEngine::GetCvarString("r_refdll_loaded");
	if (!loaded || !*loaded)
		loaded = MenuEngine::GetCvarString("r_refdll");
	if (!loaded || !*loaded)
		loaded = "gl";
	m_pRenderer->AddItem(loaded, new KeyValues("ref", "name", loaded));
	m_pRenderer->ActivateItemByRow(0);
	// Extended MenuAPI pfnGetRenderers not wired in CS-Retro menu yet — display only.
	m_pRenderer->SetEnabled(false);
	m_bIgnoreTextChanged = false;
}

void COptionsSubVideo::PrepareResolutionList()
{
	if (!m_pResolution)
		return;
	m_bIgnoreTextChanged = true;
	m_pResolution->RemoveAll();
	const AspectFilter filter = CurrentAspectFilter();
	int curW = m_applied.w;
	int curH = m_applied.h;
	bool haveCurrent = false;

	for (int i = 0;; ++i)
	{
		const char *desc = MenuEngine::GetModeString(i);
		if (!desc)
			break;
		int w = 0, h = 0;
		if (!ParseModeWh(desc, w, h) || w < 640 || h < 480)
			continue;
		if (filter != kAspectAll && ClassifyAspect(w, h) != filter)
			continue;
		char label[64];
		std::snprintf(label, sizeof(label), "%d x %d", w, h);
		KeyValues *kv = new KeyValues("mode");
		kv->SetInt("w", w);
		kv->SetInt("h", h);
		kv->SetInt("idx", i);
		m_pResolution->AddItem(label, kv);
		if (w == curW && h == curH)
			haveCurrent = true;
	}

	if (!haveCurrent && curW >= 640 && curH >= 480 &&
		(filter == kAspectAll || ClassifyAspect(curW, curH) == filter))
	{
		char label[64];
		std::snprintf(label, sizeof(label), "%d x %d", curW, curH);
		KeyValues *kv = new KeyValues("mode");
		kv->SetInt("w", curW);
		kv->SetInt("h", curH);
		kv->SetInt("idx", -1);
		m_pResolution->AddItem(label, kv);
	}
	m_bIgnoreTextChanged = false;
}

void COptionsSubVideo::SelectCurrentResolution()
{
	if (!m_pResolution)
		return;
	m_bIgnoreTextChanged = true;
	const int n = m_pResolution->GetItemCount();
	int best = 0;
	for (int i = 0; i < n; ++i)
	{
		KeyValues *kv = m_pResolution->GetItemUserData(i);
		if (kv && kv->GetInt("w") == m_applied.w && kv->GetInt("h") == m_applied.h)
		{
			best = i;
			break;
		}
	}
	if (n > 0)
		m_pResolution->ActivateItemByRow(best);
	m_bIgnoreTextChanged = false;
}

bool COptionsSubVideo::GetSelectedResolution(int &w, int &h) const
{
	w = h = 0;
	if (!m_pResolution)
		return false;
	KeyValues *kv = m_pResolution->GetActiveItemUserData();
	if (!kv)
		return false;
	w = kv->GetInt("w");
	h = kv->GetInt("h");
	return w > 0 && h > 0;
}

COptionsSubVideo::AspectFilter COptionsSubVideo::CurrentAspectFilter() const
{
	if (!m_pAspectRatio)
		return kAspectAll;
	KeyValues *kv = m_pAspectRatio->GetActiveItemUserData();
	if (!kv)
		return kAspectAll;
	const int id = kv->GetInt("id", kAspectAll);
	if (id < 0 || id >= kAspectCount)
		return kAspectAll;
	return static_cast<AspectFilter>(id);
}

bool COptionsSubVideo::ApplyModeChangesTransactional()
{
	int w = 0, h = 0;
	if (!GetSelectedResolution(w, h))
		return false;

	int fs = 0;
	if (KeyValues *kv = m_pDisplayMode->GetActiveItemUserData())
		fs = kv->GetInt("v", 0);
	if (fs < 0)
		fs = 0;
	if (fs > 2)
		fs = 2;

	ReadAppliedFromEngine(m_applied);
	const bool modeChanged = (w != m_applied.w || h != m_applied.h || fs != m_applied.fullscreen);
	if (!modeChanged)
		return true;

	m_rollback = m_applied;

	char cmd[128];
	std::snprintf(cmd, sizeof(cmd), "vid_setmode %d %d\n", w, h);
	MenuEngine::CvarSetValue("fullscreen", static_cast<float>(fs));
	MenuEngine::ClientCmdNow(cmd);

	const bool needsConfirm = (fs != 0) || (w != m_applied.w || h != m_applied.h);
	if (needsConfirm)
		BeginConfirm(m_rollback);
	else
		ReadAppliedFromEngine(m_applied);
	return true;
}

void COptionsSubVideo::BeginConfirm(const VidSnapshot &previous)
{
	m_rollback = previous;
	m_bConfirmOpen = true;
	m_confirmDeadline = SteadyNow() + 10.0;

	if (m_pConfirm)
	{
		m_pConfirm->MarkForDeletion();
		m_pConfirm = nullptr;
	}

	m_pConfirm = new QueryBox("#GameUI_VideoConfirmTitle", "#GameUI_VideoConfirmText", this);
	m_pConfirm->SetOKButtonText("#GameUI_VideoKeep");
	m_pConfirm->SetCancelButtonText("#GameUI_VideoRevert");
	m_pConfirm->SetOKCommand(new KeyValues("KeepVideoSettings"));
	m_pConfirm->SetCancelCommand(new KeyValues("RevertVideoSettings"));
	m_pConfirm->AddActionSignalTarget(this);
	m_pConfirm->DoModal();
}

void COptionsSubVideo::RollbackTo(const VidSnapshot &snap)
{
	MenuEngine::CvarSetValue("fullscreen", static_cast<float>(snap.fullscreen));
	char cmd[128];
	std::snprintf(cmd, sizeof(cmd), "vid_setmode %d %d\n", snap.w, snap.h);
	MenuEngine::ClientCmdNow(cmd);
	if (snap.renderer[0])
		MenuEngine::CvarSet("r_refdll", snap.renderer);
}

void COptionsSubVideo::EndConfirm(bool keep)
{
	m_bConfirmOpen = false;
	m_confirmDeadline = 0.0;
	if (m_pConfirm)
	{
		m_pConfirm->MarkForDeletion();
		m_pConfirm = nullptr;
	}
	if (keep)
		ReadAppliedFromEngine(m_applied);
	else
	{
		RollbackTo(m_rollback);
		ReadAppliedFromEngine(m_applied);
		m_bIgnoreTextChanged = true;
		PrepareResolutionList();
		SelectCurrentResolution();
		m_pDisplayMode->ActivateItemByRow(m_applied.fullscreen < 0 ? 0 : (m_applied.fullscreen > 2 ? 2 : m_applied.fullscreen));
		m_bIgnoreTextChanged = false;
	}
}

void COptionsSubVideo::OnKeepVideoSettings()
{
	EndConfirm(true);
}

void COptionsSubVideo::OnRevertVideoSettings()
{
	EndConfirm(false);
}

void COptionsSubVideo::OnTick()
{
	if (!m_bConfirmOpen)
		return;
	if (SteadyNow() >= m_confirmDeadline)
		EndConfirm(false);
}

void COptionsSubVideo::MarkDirty()
{
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

void COptionsSubVideo::OnControlModified()
{
	MarkDirty();
}

void COptionsSubVideo::OnTextChanged(Panel *panel)
{
	if (m_bIgnoreTextChanged)
		return;
	if (panel == m_pAspectRatio)
	{
		PrepareResolutionList();
		SelectCurrentResolution();
	}
	MarkDirty();
}

void COptionsSubVideo::Gate_SetBrightnessPending(float value)
{
	m_pBrightness->SetSliderValue(value);
}

float COptionsSubVideo::Gate_GetBrightnessPending() const
{
	return m_pBrightness->GetSliderValue();
}

void COptionsSubVideo::Gate_SetGammaPending(float value)
{
	m_pGamma->SetSliderValue(value);
}

float COptionsSubVideo::Gate_GetGammaPending() const
{
	return m_pGamma->GetSliderValue();
}

void COptionsSubVideo::Gate_SetVSyncPending(bool on)
{
	m_pVSync->SetSelected(on);
}

bool COptionsSubVideo::Gate_GetVSyncPending() const
{
	return m_pVSync->IsSelected();
}

int COptionsSubVideo::Gate_GetDisplayModePending() const
{
	KeyValues *kv = m_pDisplayMode->GetActiveItemUserData();
	return kv ? kv->GetInt("v", 0) : 0;
}

int COptionsSubVideo::Gate_GetResolutionWide() const
{
	int w = 0, h = 0;
	GetSelectedResolution(w, h);
	return w;
}

int COptionsSubVideo::Gate_GetResolutionTall() const
{
	int w = 0, h = 0;
	GetSelectedResolution(w, h);
	return h;
}

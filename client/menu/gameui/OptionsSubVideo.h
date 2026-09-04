#pragma once

#include <cstdint>
#include <vgui_controls/PropertyPage.h>

class CCvarSlider;
class CCvarToggleCheckButton;

namespace vgui2
{
class ComboBox;
class Label;
class QueryBox;
}

class COptionsSubVideo : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(COptionsSubVideo, vgui2::PropertyPage);

public:
	explicit COptionsSubVideo(vgui2::Panel *parent);
	~COptionsSubVideo() override;

	void OnPageShow() override;
	void OnResetData() override;
	void OnApplyChanges() override;
	void OnTick() override;

	// Functional gate helpers
	void Gate_SetBrightnessPending(float value);
	float Gate_GetBrightnessPending() const;
	void Gate_SetGammaPending(float value);
	float Gate_GetGammaPending() const;
	void Gate_PreviewGammaBrightness();
	void Gate_SetVSyncPending(bool on);
	bool Gate_GetVSyncPending() const;
	int Gate_GetDisplayModePending() const;
	int Gate_GetResolutionWide() const;
	int Gate_GetResolutionTall() const;
	bool Gate_SelectResolution(int w, int h);
	void Gate_SetDisplayModePending(int fullscreen);
	bool Gate_IsConfirmOpen() const;
	void Gate_ConfirmKeep();
	void Gate_ConfirmRevert();
	void Gate_ExpireConfirmNow(); // exercises OnTick timeout path without blocking UI thread
	void Gate_GetApplied(int &w, int &h, int &fullscreen) const;
	int64_t Gate_GetConfirmShownMs() const { return m_confirmShownMs; }

	enum AspectFilter : int
	{
		kAspectAll = 0,
		kAspect4x3,
		kAspect5x4,
		kAspect16x9,
		kAspect16x10,
		kAspectOther,
		kAspectCount
	};

	static AspectFilter ClassifyAspect(int w, int h);

protected:
	MESSAGE_FUNC_PTR(OnControlModified, "ControlModified", panel);
	MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);
	MESSAGE_FUNC(OnKeepVideoSettings, "KeepVideoSettings");
	MESSAGE_FUNC(OnRevertVideoSettings, "RevertVideoSettings");

private:
	struct VidSnapshot
	{
		int w = 0;
		int h = 0;
		int fullscreen = 0;
		char renderer[128]{};
	};

	void PrepareResolutionList();
	void SelectCurrentResolution();
	bool GetSelectedResolution(int &w, int &h) const;
	AspectFilter CurrentAspectFilter() const;
	void ReadAppliedFromEngine(VidSnapshot &out) const;
	void SyncUiFromApplied();
	void ApplyLiveCvars();
	void PreviewGammaBrightness();
	void CancelGammaBrightnessPreview();
	bool ApplyModeChangesTransactional();
	void BeginConfirm(const VidSnapshot &previous);
	void RollbackTo(const VidSnapshot &snap);
	void EndConfirm(bool keep);
	void MarkDirty();
	void RefreshRendererCombo();

	CCvarSlider *m_pBrightness = nullptr;
	CCvarSlider *m_pGamma = nullptr;
	CCvarToggleCheckButton *m_pVSync = nullptr;
	CCvarToggleCheckButton *m_pDetailTextures = nullptr;
	vgui2::ComboBox *m_pResolution = nullptr;
	vgui2::ComboBox *m_pAspectRatio = nullptr;
	vgui2::ComboBox *m_pDisplayMode = nullptr;
	vgui2::ComboBox *m_pRenderer = nullptr;
	vgui2::Label *m_pVideoNote = nullptr;
	vgui2::QueryBox *m_pConfirm = nullptr;

	VidSnapshot m_applied{};
	VidSnapshot m_rollback{};
	bool m_bConfirmOpen = false;
	double m_confirmDeadline = 0.0;
	int64_t m_confirmShownMs = 0;
	bool m_bIgnoreTextChanged = false;
	bool m_bGammaBrightnessPreview = false;
	float m_previewOriginalBrightness = 0.0f;
	float m_previewOriginalGamma = 2.5f;

	friend class COptionsDialog;
};

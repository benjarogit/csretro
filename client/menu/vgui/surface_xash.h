#pragma once

#include <string>
#include <unordered_map>

#include <vgui/ISurfaceNext.h>

namespace vgui2
{
class CSurfaceXash : public ISurfaceNext
{
public:
	CSurfaceXash();
	~CSurfaceXash() override;

	void Shutdown() override;
	void RunFrame() override;
	VPANEL GetEmbeddedPanel() override;
	void SetEmbeddedPanel(VPANEL pPanel) override;
	void PushMakeCurrent(VPANEL panel, bool useInsets) override;
	void PopMakeCurrent(VPANEL panel) override;
	void DrawSetColor(int r, int g, int b, int a) override;
	void DrawSetColor(Color col) override;
	void DrawFilledRect(int x0, int y0, int x1, int y1) override;
	void DrawOutlinedRect(int x0, int y0, int x1, int y1) override;
	void DrawLine(int x0, int y0, int x1, int y1) override;
	void DrawPolyLine(int *px, int *py, int numPoints) override;
	void DrawSetTextFont(HFont font) override;
	void DrawSetTextColor(int r, int g, int b, int a) override;
	void DrawSetTextColor(Color col) override;
	void DrawSetTextPos(int x, int y) override;
	void DrawGetTextPos(int &x, int &y) override;
	void DrawPrintText(const wchar_t *text, int textLen) override;
	void DrawUnicodeChar(wchar_t wch) override;
	void DrawUnicodeCharAdd(wchar_t wch) override;
	void DrawFlushText() override;
	IHTML *CreateHTMLWindow(IHTMLEvents *events, VPANEL context) override;
	void PaintHTMLWindow(IHTML *htmlwin) override;
	void DeleteHTMLWindow(IHTML *htmlwin) override;
	void DrawSetTextureFile(int id, const char *filename, int hardwareFilter, bool forceReload) override;
	void DrawSetTextureRGBA(int id, const unsigned char *rgba, int wide, int tall, int hardwareFilter, bool forceReload) override;
	void DrawSetTexture(int id) override;
	void DrawGetTextureSize(int id, int &wide, int &tall) override;
	void DrawTexturedRect(int x0, int y0, int x1, int y1) override;
	bool IsTextureIDValid(int id) override;
	int CreateNewTextureID(bool procedural = false) override;
	void GetScreenSize(int &wide, int &tall) override;
	void SetAsTopMost(VPANEL panel, bool state) override;
	void BringToFront(VPANEL panel) override;
	void SetForegroundWindow(VPANEL panel) override;
	void SetPanelVisible(VPANEL panel, bool state) override;
	void SetMinimized(VPANEL panel, bool state) override;
	bool IsMinimized(VPANEL panel) override;
	void FlashWindow(VPANEL panel, bool state) override;
	void SetTitle(VPANEL panel, const wchar_t *title) override;
	void SetAsToolBar(VPANEL panel, bool state) override;
	void CreatePopup(VPANEL panel, bool minimised, bool showTaskbarIcon = true, bool disabled = false, bool mouseInput = true, bool kbInput = true) override;
	void SwapBuffers(VPANEL panel) override;
	void Invalidate(VPANEL panel) override;
	void SetCursor(HCursor cursor) override;
	bool IsCursorVisible() override;
	void ApplyChanges() override;
	bool IsWithin(int x, int y) override;
	bool HasFocus() override;
	bool SupportsFeature(SurfaceFeature_e feature) override;
	void RestrictPaintToSinglePanel(VPANEL panel) override;
	void SetModalPanel(VPANEL panel) override;
	VPANEL GetModalPanel() override;
	void UnlockCursor() override;
	void LockCursor() override;
	void SetTranslateExtendedKeys(bool state) override;
	VPANEL GetTopmostPopup() override;
	void SetTopLevelFocus(VPANEL panel) override;
	HFont CreateFont() override;
	bool AddGlyphSetToFont(HFont font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags, int lowRange, int highRange) override;
	bool AddCustomFontFile(const char *fontFileName) override;
	int GetFontTall(HFont font) override;
	void GetCharABCwide(HFont font, int ch, int &a, int &b, int &c) override;
	int GetCharacterWidth(HFont font, int ch) override;
	void GetTextSize(HFont font, const wchar_t *text, int &wide, int &tall) override;
	VPANEL GetNotifyPanel() override;
	void SetNotifyIcon(VPANEL context, HTexture icon, VPANEL panelToReceiveMessages, const char *text) override;
	void PlaySound(const char *fileName) override;
	int GetPopupCount() override;
	VPANEL GetPopup(int index) override;
	bool ShouldPaintChildPanel(VPANEL childPanel) override;
	bool RecreateContext(VPANEL panel) override;
	void AddPanel(VPANEL panel) override;
	void ReleasePanel(VPANEL panel) override;
	void MovePopupToFront(VPANEL panel) override;
	void MovePopupToBack(VPANEL panel) override;
	void SolveTraverse(VPANEL panel, bool forceApplySchemeSettings = false) override;
	void PaintTraverse(VPANEL panel) override;
	void EnableMouseCapture(VPANEL panel, bool state) override;
	void GetWorkspaceBounds(int &x, int &y, int &wide, int &tall) override;
	void GetAbsoluteWindowBounds(int &x, int &y, int &wide, int &tall) override;
	void GetProportionalBase(int &width, int &height) override;
	void CalculateMouseVisible() override;
	bool NeedKBInput() override;
	bool HasCursorPosFunctions() override;
	void SurfaceGetCursorPos(int &x, int &y) override;
	void SurfaceSetCursorPos(int x, int y) override;
	void DrawTexturedPolygon(VGuiVertex *pVertices, int n) override;
	int GetFontAscent(HFont font, wchar_t wch) override;
	void SetAllowHTMLJavaScript(bool state) override;
	void SetLanguage(const char *pchLang) override;
	const char *GetLanguage() override;
	bool DeleteTextureByID(int id) override;
	void DrawUpdateRegionTextureBGRA(int nTextureID, int x, int y, const unsigned char *pchData, int wide, int tall) override;
	void DrawSetTextureBGRA(int id, const unsigned char *pchData, int wide, int tall) override;
	void CreateBrowser(VPANEL panel, IHTMLResponses *pBrowser, bool bPopupWindow, const char *pchUserAgentIdentifier) override;
	void RemoveBrowser(VPANEL panel, IHTMLResponses *pBrowser) override;
	IHTMLChromeController *AccessChromeHTMLController() override;

	float DrawGetAlphaMultiplier() const override;
	void DrawSetAlphaMultiplier(float a) override;

	void SetCursorPosInternal(int x, int y);

private:
	VPANEL m_embedded = 0;
	VPANEL m_restrictPaint = 0;
	VPANEL m_modal = 0;
	VPANEL m_topFocus = 0;
	VPANEL m_mouseCapture = 0;
	HFont m_textFont = 0;
	HCursor m_cursor = 0;
	int m_textureId = -1;
	int m_drawR = 255, m_drawG = 255, m_drawB = 255, m_drawA = 255;
	int m_textR = 255, m_textG = 255, m_textB = 255, m_textA = 255;
	int m_textX = 0, m_textY = 0;
	int m_cursorX = 0, m_cursorY = 0;
	float m_alphaMult = 1.f;
	bool m_cursorVisible = true;
	bool m_cursorLocked = false;
	bool m_translateKeys = true;
	std::string m_language = "english";
	std::unordered_map<VPANEL, bool> m_minimized;
	std::unordered_map<VPANEL, std::wstring> m_titles;
};
} // namespace vgui2

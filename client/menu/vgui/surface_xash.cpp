// ISurfaceNext → Xash ui_enginefuncs_t. FreeType glyphs → TGA → pfnPIC_Load.
#include "surface_xash.h"
#include "font_resolver.h"
#include "vgui_symbols.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SIZES_H

#include "Color.h"
#include "tier1/interface.h"
#include "vgui/IPanel.h"
#include "vgui_internal.h"

#include "../src/menu_priv.h"

using namespace vgui2;

namespace
{
struct Context
{
	VPANEL panel = 0;
	int absX = 0;
	int absY = 0;
};

struct FontInfo
{
	std::string name;
	int tall = 12;	  // Scheme-Request (Win32 CreateFont-Höhe)
	int height = 12;  // effektive Zellhöhe (wie Win32 tmHeight / GetFontTall)
	int weight = 400;
	int ascent = 0;
	int flags = 0;
	bool antialias = false;
	FT_Face face = nullptr;
	bool ok = false;
	bool symbol = false; // Marlett / MarlettSmall — geometrische VGUI-Symbole
};

struct Texture
{
	bool valid = false;
	int wide = 0;
	int tall = 0;
	std::vector<unsigned char> rgba;
	HIMAGE pic = 0;
	std::string picName;
};

struct GlyphEntry
{
	HIMAGE pic = 0;
	std::string picName;
	int bearingX = 0;
	int bearingY = 0;
	int advance = 0;
	int width = 0;
	int height = 0;
	int a = 0;
	int b = 0;
	int c = 0;
};

struct GlyphKey
{
	HFont font = 0;
	uint32_t codepoint = 0;
	bool operator==(const GlyphKey &o) const { return font == o.font && codepoint == o.codepoint; }
};

struct GlyphKeyHash
{
	size_t operator()(const GlyphKey &k) const
	{
		return static_cast<size_t>(k.font) * 1315423911u ^ static_cast<size_t>(k.codepoint);
	}
};

#pragma pack(push, 1)
struct CsretroTgaHeader
{
	uint8_t id_length, colormap_type, image_type;
	uint16_t cm_first, cm_length;
	uint8_t cm_size;
	uint16_t x_origin, y_origin, width, height;
	uint8_t pixel_size, attributes;
};
#pragma pack(pop)

FT_Library g_ft = nullptr;
std::vector<FontInfo> g_fonts; // index 0 unused; HFont is 1-based
std::vector<Texture> g_textures;
std::vector<VPANEL> g_popups;
std::vector<Context> g_ctx;
std::unordered_map<GlyphKey, GlyphEntry, GlyphKeyHash> g_glyphs;
bool g_loggedGlyphPath = false;
bool g_freetypeGlyphsLogged = false;
CSurfaceXash *g_symbolPaintSurface = nullptr;

void SymbolFillThunk(int x0, int y0, int x1, int y1)
{
	if (g_symbolPaintSurface)
		g_symbolPaintSurface->DrawFilledRect(x0, y0, x1, y1);
}

void SymbolLineThunk(int x0, int y0, int x1, int y1)
{
	if (g_symbolPaintSurface)
		g_symbolPaintSurface->DrawLine(x0, y0, x1, y1);
}

void EnsureFT()
{
	if (!g_ft)
		FT_Init_FreeType(&g_ft);
}

FontInfo *GetFont(HFont font)
{
	if (font == 0 || font >= g_fonts.size())
		return nullptr;
	return &g_fonts[font];
}

void FreeGlyphEntry(GlyphEntry &g)
{
	if (g.pic && gEng.pfnPIC_Free && !g.picName.empty())
		gEng.pfnPIC_Free(g.picName.c_str());
	g = GlyphEntry{};
}

void ClearGlyphsForFont(HFont font)
{
	for (auto it = g_glyphs.begin(); it != g_glyphs.end();)
	{
		if (it->first.font == font)
		{
			FreeGlyphEntry(it->second);
			it = g_glyphs.erase(it);
		}
		else
			++it;
	}
}

void ClearAllGlyphs()
{
	for (auto &kv : g_glyphs)
		FreeGlyphEntry(kv.second);
	g_glyphs.clear();
}

std::vector<uint8_t> BuildBgraTga(int wide, int tall, const uint8_t *bgraPixels)
{
	std::vector<uint8_t> buf(sizeof(CsretroTgaHeader) + static_cast<size_t>(wide * tall * 4));
	auto *hdr = reinterpret_cast<CsretroTgaHeader *>(buf.data());
	std::memset(hdr, 0, sizeof(*hdr));
	hdr->image_type = 2;
	hdr->width = static_cast<uint16_t>(wide);
	hdr->height = static_cast<uint16_t>(tall);
	hdr->pixel_size = 32;
	hdr->attributes = 0x28; // upper-left origin + 8-bit alpha
	std::memcpy(buf.data() + sizeof(CsretroTgaHeader), bgraPixels, static_cast<size_t>(wide * tall * 4));
	return buf;
}

std::vector<uint8_t> RgbaToBgraTga(const unsigned char *rgba, int wide, int tall)
{
	std::vector<uint8_t> bgra(static_cast<size_t>(wide * tall * 4));
	for (int i = 0; i < wide * tall; ++i)
	{
		bgra[static_cast<size_t>(i * 4 + 0)] = rgba[i * 4 + 2];
		bgra[static_cast<size_t>(i * 4 + 1)] = rgba[i * 4 + 1];
		bgra[static_cast<size_t>(i * 4 + 2)] = rgba[i * 4 + 0];
		bgra[static_cast<size_t>(i * 4 + 3)] = rgba[i * 4 + 3];
	}
	return BuildBgraTga(wide, tall, bgra.data());
}

constexpr int kPicFlags = PIC_NOMIPMAP | PIC_NEAREST | PIC_HAS_ALPHA;

GlyphEntry *EnsureGlyph(HFont font, uint32_t codepoint)
{
	GlyphKey key{font, codepoint};
	auto it = g_glyphs.find(key);
	if (it != g_glyphs.end())
		return &it->second;

	FontInfo *fi = GetFont(font);
	if (!fi || !fi->ok || !fi->face || !gEng.pfnPIC_Load)
		return nullptr;

	// Win32 VGUI: ohne FONTFLAG_ANTIALIAS → NONANTIALIASED_QUALITY (TrackerScheme Default).
	const int loadFlags = fi->antialias ? (FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL)
					    : (FT_LOAD_RENDER | FT_LOAD_TARGET_MONO);
	if (FT_Load_Char(fi->face, codepoint, loadFlags) != 0)
		return nullptr;

	FT_GlyphSlot slot = fi->face->glyph;
	const int bearingX = slot->bitmap_left;
	const int bearingY = slot->bitmap_top;
	const int advance = static_cast<int>(slot->advance.x >> 6);
	const int a = static_cast<int>(slot->metrics.horiBearingX >> 6);
	const int b = static_cast<int>(slot->metrics.width >> 6);
	const int c = advance - a - b;

	GlyphEntry entry;
	entry.bearingX = bearingX;
	entry.bearingY = bearingY;
	entry.advance = advance;
	entry.a = a;
	entry.b = b;
	entry.c = c;

	const int gw = static_cast<int>(slot->bitmap.width);
	const int gh = static_cast<int>(slot->bitmap.rows);
	entry.width = gw;
	entry.height = gh;

	if (gw > 0 && gh > 0 && slot->bitmap.buffer)
	{
		std::vector<uint8_t> bgra(static_cast<size_t>(gw * gh * 4));
		const int pitch = slot->bitmap.pitch;
		for (int y = 0; y < gh; ++y)
		{
			const uint8_t *src = slot->bitmap.buffer + y * pitch;
			for (int x = 0; x < gw; ++x)
			{
				const size_t di = static_cast<size_t>((y * gw + x) * 4);
				uint8_t gray = 0;
				if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY)
					gray = src[x];
				else if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
					gray = (src[x >> 3] & (0x80 >> (x & 7))) ? 255 : 0;
				bgra[di + 0] = 255;
				bgra[di + 1] = 255;
				bgra[di + 2] = 255;
				bgra[di + 3] = gray;
			}
		}

		auto tga = BuildBgraTga(gw, gh, bgra.data());
		char name[96];
		std::snprintf(name, sizeof(name), "#csretro_g_%u_%u.tga", static_cast<unsigned>(font), codepoint);
		entry.picName = name;
		if (gEng.pfnPIC_Free)
			gEng.pfnPIC_Free(name);
		entry.pic = gEng.pfnPIC_Load(name, tga.data(), static_cast<int>(tga.size()), kPicFlags);
	}

	auto inserted = g_glyphs.emplace(key, std::move(entry));
	return &inserted.first->second;
}
} // namespace

CSurfaceXash::CSurfaceXash()
{
	g_fonts.emplace_back(); // dummy index 0
	EnsureFT();
}

CSurfaceXash::~CSurfaceXash()
{
	Shutdown();
}

void CSurfaceXash::Shutdown()
{
	ClearAllGlyphs();
	for (auto &f : g_fonts)
	{
		if (f.face)
		{
			FT_Done_Face(f.face);
			f.face = nullptr;
		}
	}
	g_fonts.clear();
	g_fonts.emplace_back();
	for (auto &t : g_textures)
	{
		if (t.pic && gEng.pfnPIC_Free && !t.picName.empty())
			gEng.pfnPIC_Free(t.picName.c_str());
	}
	g_textures.clear();
	g_popups.clear();
	g_ctx.clear();
}

void CSurfaceXash::RunFrame() {}

VPANEL CSurfaceXash::GetEmbeddedPanel()
{
	return m_embedded;
}

void CSurfaceXash::SetEmbeddedPanel(VPANEL pPanel)
{
	m_embedded = pPanel;
}

void CSurfaceXash::PushMakeCurrent(VPANEL panel, bool useInsets)
{
	Context c;
	c.panel = panel;
	if (g_pIPanel && panel)
	{
		g_pIPanel->GetAbsPos(panel, c.absX, c.absY);
		if (useInsets)
		{
			int l, t, r, b;
			g_pIPanel->GetInset(panel, l, t, r, b);
			c.absX += l;
			c.absY += t;
		}
	}
	g_ctx.push_back(c);
}

void CSurfaceXash::PopMakeCurrent(VPANEL)
{
	if (!g_ctx.empty())
		g_ctx.pop_back();
}

void CSurfaceXash::DrawSetColor(int r, int g, int b, int a)
{
	m_drawR = r;
	m_drawG = g;
	m_drawB = b;
	m_drawA = a;
}

void CSurfaceXash::DrawSetColor(Color col)
{
	DrawSetColor(col.r(), col.g(), col.b(), col.a());
}

static void CurrentOffset(int &ox, int &oy)
{
	ox = oy = 0;
	if (!g_ctx.empty())
	{
		ox = g_ctx.back().absX;
		oy = g_ctx.back().absY;
	}
}

void CSurfaceXash::DrawFilledRect(int x0, int y0, int x1, int y1)
{
	static bool s_rectLogged;
	if (!s_rectLogged && getenv("CSRETRO_V1POC"))
	{
		s_rectLogged = true;
		Menu_Con("CSRETRO_V1POC_DRAW_RECT");
	}
	if (!gEng.pfnFillRGBA)
		return;
	int ox, oy;
	CurrentOffset(ox, oy);
	int a = static_cast<int>(m_drawA * m_alphaMult);
	if (a <= 0)
		return;
	int w = x1 - x0;
	int h = y1 - y0;
	if (w <= 0 || h <= 0)
		return;
	gEng.pfnFillRGBA(ox + x0, oy + y0, w, h, m_drawR, m_drawG, m_drawB, a);
}

void CSurfaceXash::DrawOutlinedRect(int x0, int y0, int x1, int y1)
{
	DrawFilledRect(x0, y0, x1, y0 + 1);
	DrawFilledRect(x0, y1 - 1, x1, y1);
	DrawFilledRect(x0, y0, x0 + 1, y1);
	DrawFilledRect(x1 - 1, y0, x1, y1);
}

void CSurfaceXash::DrawLine(int x0, int y0, int x1, int y1)
{
	int dx = x1 - x0;
	int dy = y1 - y0;
	int steps = std::max(std::abs(dx), std::abs(dy));
	if (steps <= 0)
	{
		DrawFilledRect(x0, y0, x0 + 1, y0 + 1);
		return;
	}
	for (int i = 0; i <= steps; ++i)
	{
		int x = x0 + dx * i / steps;
		int y = y0 + dy * i / steps;
		DrawFilledRect(x, y, x + 1, y + 1);
	}
}

void CSurfaceXash::DrawPolyLine(int *px, int *py, int numPoints)
{
	if (!px || !py || numPoints < 2)
		return;
	for (int i = 0; i + 1 < numPoints; ++i)
		DrawLine(px[i], py[i], px[i + 1], py[i + 1]);
}

void CSurfaceXash::DrawSetTextFont(HFont font)
{
	m_textFont = font;
	FontInfo *fi = GetFont(font);
	if (fi && fi->ok)
		return;
	// Scheme/control may pass INVALID_FONT (0) before ApplyScheme — use first ready face.
	for (size_t i = 1; i < g_fonts.size(); ++i)
	{
		if (g_fonts[i].ok)
		{
			m_textFont = static_cast<HFont>(i);
			return;
		}
	}
}

void CSurfaceXash::DrawSetTextColor(int r, int g, int b, int a)
{
	m_textR = r;
	m_textG = g;
	m_textB = b;
	m_textA = a;
}

void CSurfaceXash::DrawSetTextColor(Color col)
{
	DrawSetTextColor(col.r(), col.g(), col.b(), col.a());
}

void CSurfaceXash::DrawSetTextPos(int x, int y)
{
	m_textX = x;
	m_textY = y;
}

void CSurfaceXash::DrawGetTextPos(int &x, int &y)
{
	x = m_textX;
	y = m_textY;
}

void CSurfaceXash::DrawPrintText(const wchar_t *text, int textLen)
{
	if (!text || textLen <= 0)
		return;
	for (int i = 0; i < textLen; ++i)
		DrawUnicodeChar(text[i]);
}

void CSurfaceXash::DrawUnicodeChar(wchar_t wch)
{
	if (wch == L'\0')
		return;

	FontInfo *fi = GetFont(m_textFont);
	if (fi && fi->ok && fi->symbol)
	{
		const int aRaw = static_cast<int>(m_textA * m_alphaMult);
		const int a = aRaw > 0 ? aRaw : 255;
		DrawSetColor(m_textR, m_textG, m_textB, a);
		const int tall = fi->tall > 0 ? fi->tall : 12;
		// Panel-relative (m_textX/Y): DrawFilledRect/DrawLine addieren CurrentOffset selbst.
		g_symbolPaintSurface = this;
		CsretroVguiSymbols::PaintCodepoint(m_textX, m_textY, tall, static_cast<uint32_t>(wch), SymbolFillThunk,
			SymbolLineThunk);
		g_symbolPaintSurface = nullptr;
		m_textX += CsretroVguiSymbols::AdvanceForTall(tall);
		return;
	}

	const uint32_t cp = static_cast<uint32_t>(wch);
	GlyphEntry *glyph = EnsureGlyph(m_textFont, cp);
	fi = GetFont(m_textFont);

	if (glyph)
	{
		int ox, oy;
		CurrentOffset(ox, oy);
		const int aRaw = static_cast<int>(m_textA * m_alphaMult);
		// Scheme-Farben ohne Alpha kommen oft als 0 an — sichtbarer Default.
		const int a = aRaw > 0 ? aRaw : 255;
		if (glyph->pic && glyph->width > 0 && glyph->height > 0 && gEng.pfnPIC_Set && gEng.pfnPIC_DrawTrans && a > 0)
		{
			const int ascent = fi && fi->ascent > 0 ? fi->ascent : (fi ? fi->tall : 12);
			const int dx = ox + m_textX + glyph->bearingX;
			const int dy = oy + m_textY + (ascent - glyph->bearingY);
			gEng.pfnPIC_Set(glyph->pic, m_textR, m_textG, m_textB, a);
			gEng.pfnPIC_DrawTrans(dx, dy, glyph->width, glyph->height, nullptr);
			if (!g_freetypeGlyphsLogged)
			{
				g_freetypeGlyphsLogged = true;
				Menu_Con("CSRETRO_V1POC_FREETYPE_GLYPHS");
			}
		}
		m_textX += glyph->advance > 0 ? glyph->advance : GetCharacterWidth(m_textFont, static_cast<int>(wch));
		return;
	}

	m_textX += GetCharacterWidth(m_textFont, static_cast<int>(wch));
}

void CSurfaceXash::DrawUnicodeCharAdd(wchar_t wch) { DrawUnicodeChar(wch); }
void CSurfaceXash::DrawFlushText() {}

IHTML *CSurfaceXash::CreateHTMLWindow(IHTMLEvents *, VPANEL) { return nullptr; }
void CSurfaceXash::PaintHTMLWindow(IHTML *) {}
void CSurfaceXash::DeleteHTMLWindow(IHTML *) {}

void CSurfaceXash::DrawSetTextureFile(int id, const char *, int, bool)
{
	if (!IsTextureIDValid(id))
		return;
	(void)g_textures[static_cast<size_t>(id)];
}

void CSurfaceXash::DrawSetTextureRGBA(int id, const unsigned char *rgba, int wide, int tall, int, bool)
{
	if (id < 0)
		return;
	if (static_cast<size_t>(id) >= g_textures.size())
		g_textures.resize(static_cast<size_t>(id) + 1);
	Texture &t = g_textures[static_cast<size_t>(id)];

	if (t.pic && gEng.pfnPIC_Free && !t.picName.empty())
	{
		gEng.pfnPIC_Free(t.picName.c_str());
		t.pic = 0;
	}

	t.valid = rgba && wide > 0 && tall > 0;
	t.wide = wide;
	t.tall = tall;
	t.rgba.clear();
	t.picName.clear();
	if (!t.valid)
		return;

	t.rgba.assign(rgba, rgba + static_cast<size_t>(wide * tall * 4));

	if (gEng.pfnPIC_Load)
	{
		auto tga = RgbaToBgraTga(rgba, wide, tall);
		char name[64];
		std::snprintf(name, sizeof(name), "#csretro_tex_%d.tga", id);
		t.picName = name;
		if (gEng.pfnPIC_Free)
			gEng.pfnPIC_Free(name);
		t.pic = gEng.pfnPIC_Load(name, tga.data(), static_cast<int>(tga.size()), kPicFlags);
	}
}

void CSurfaceXash::DrawSetTexture(int id) { m_textureId = id; }

void CSurfaceXash::DrawGetTextureSize(int id, int &wide, int &tall)
{
	wide = tall = 0;
	if (!IsTextureIDValid(id))
		return;
	wide = g_textures[static_cast<size_t>(id)].wide;
	tall = g_textures[static_cast<size_t>(id)].tall;
}

void CSurfaceXash::DrawTexturedRect(int x0, int y0, int x1, int y1)
{
	if (!IsTextureIDValid(m_textureId))
		return;
	const Texture &t = g_textures[static_cast<size_t>(m_textureId)];
	if (!t.valid || !t.pic || !gEng.pfnPIC_Set || !gEng.pfnPIC_DrawTrans)
		return;
	int ox, oy;
	CurrentOffset(ox, oy);
	const int a = static_cast<int>(m_drawA * m_alphaMult);
	if (a <= 0)
		return;
	const int w = x1 - x0;
	const int h = y1 - y0;
	if (w <= 0 || h <= 0)
		return;
	gEng.pfnPIC_Set(t.pic, m_drawR, m_drawG, m_drawB, a);
	gEng.pfnPIC_DrawTrans(ox + x0, oy + y0, w, h, nullptr);
}

bool CSurfaceXash::IsTextureIDValid(int id)
{
	return id >= 0 && static_cast<size_t>(id) < g_textures.size() && g_textures[static_cast<size_t>(id)].valid;
}

int CSurfaceXash::CreateNewTextureID(bool)
{
	g_textures.emplace_back();
	g_textures.back().valid = true;
	return static_cast<int>(g_textures.size() - 1);
}

void CSurfaceXash::GetScreenSize(int &wide, int &tall)
{
	wide = gGlobals ? gGlobals->scrWidth : 640;
	tall = gGlobals ? gGlobals->scrHeight : 480;
	// Menu kann vor dem ersten Video-Mode-Callback laufen (scrWidth noch 0).
	if (wide <= 0)
		wide = 640;
	if (tall <= 0)
		tall = 480;
}

void CSurfaceXash::SetAsTopMost(VPANEL, bool) {}
void CSurfaceXash::BringToFront(VPANEL panel)
{
	if (g_pIPanel)
		g_pIPanel->MoveToFront(panel);
}
void CSurfaceXash::SetForegroundWindow(VPANEL panel) { BringToFront(panel); }
void CSurfaceXash::SetPanelVisible(VPANEL panel, bool state)
{
	// VPanel::SetVisible already updated visibility and calls us for surface/OS hooks.
	// Must NOT call ipanel()->SetVisible — that re-enters VPanel::SetVisible (stack overflow).
	(void)panel;
	(void)state;
}
void CSurfaceXash::SetMinimized(VPANEL panel, bool state)
{
	m_minimized[panel] = state;
}
bool CSurfaceXash::IsMinimized(VPANEL panel)
{
	auto it = m_minimized.find(panel);
	return it != m_minimized.end() && it->second;
}
void CSurfaceXash::FlashWindow(VPANEL, bool) {}
void CSurfaceXash::SetTitle(VPANEL panel, const wchar_t *title)
{
	if (!title)
		return;
	m_titles[panel] = title;
}
void CSurfaceXash::SetAsToolBar(VPANEL, bool) {}

void CSurfaceXash::CreatePopup(VPANEL panel, bool, bool, bool, bool mouseInput, bool kbInput)
{
	if (!panel)
		return;
	if (g_pIPanel)
	{
		g_pIPanel->SetPopup(panel, true);
		g_pIPanel->SetMouseInputEnabled(panel, mouseInput);
		g_pIPanel->SetKeyBoardInputEnabled(panel, kbInput);
	}
	for (VPANEL p : g_popups)
	{
		if (p == panel)
			return;
	}
	g_popups.push_back(panel);
}

void CSurfaceXash::SwapBuffers(VPANEL) {}
void CSurfaceXash::Invalidate(VPANEL panel)
{
	// Panel::Repaint already sets NEEDS_REPAINT then calls us.
	// Must NOT call ipanel()->Repaint — that re-enters Panel::Repaint (infinite recursion).
	(void)panel;
}
void CSurfaceXash::SetCursor(HCursor cursor) { m_cursor = cursor; }
bool CSurfaceXash::IsCursorVisible() { return m_cursorVisible; }
void CSurfaceXash::ApplyChanges() {}
bool CSurfaceXash::IsWithin(int x, int y)
{
	int w, h;
	GetScreenSize(w, h);
	return x >= 0 && y >= 0 && x < w && y < h;
}
bool CSurfaceXash::HasFocus() { return true; }
bool CSurfaceXash::SupportsFeature(SurfaceFeature_e feature)
{
	return feature == ESCAPE_KEY || feature == ANTIALIASED_FONTS;
}
void CSurfaceXash::RestrictPaintToSinglePanel(VPANEL panel) { m_restrictPaint = panel; }
void CSurfaceXash::SetModalPanel(VPANEL panel) { m_modal = panel; }
VPANEL CSurfaceXash::GetModalPanel() { return m_modal; }
void CSurfaceXash::UnlockCursor() { m_cursorLocked = false; }
void CSurfaceXash::LockCursor() { m_cursorLocked = true; }
void CSurfaceXash::SetTranslateExtendedKeys(bool state) { m_translateKeys = state; }
VPANEL CSurfaceXash::GetTopmostPopup()
{
	return g_popups.empty() ? 0 : g_popups.back();
}
void CSurfaceXash::SetTopLevelFocus(VPANEL panel) { m_topFocus = panel; }

HFont CSurfaceXash::CreateFont()
{
	g_fonts.emplace_back();
	return static_cast<HFont>(g_fonts.size() - 1);
}

bool CSurfaceXash::AddGlyphSetToFont(HFont font, const char *windowsFontName, int tall, int weight, int, int, int flags, int, int)
{
	FontInfo *fi = GetFont(font);
	if (!fi)
		return false;

	// Scheme lädt nach dem Primärfont immer „DejaVu Sans“ als lastResort.
	// Win32 FontManager hängt Fallbacks an; unser Surface ersetzt sonst den Face → falsche Metriken.
	if (fi->ok && (fi->symbol || fi->face) && windowsFontName &&
		strcasecmp(windowsFontName, fi->name.c_str()) != 0)
		return true;

	// Scheme lädt nach Marlett immer „DejaVu Sans“ als lastResort — Symbolfonts nicht überschreiben.
	if (fi->symbol && !CsretroVguiSymbols::IsSymbolFontName(windowsFontName))
		return true;

	EnsureFT();
	ClearGlyphsForFont(font);
	fi->name = windowsFontName ? windowsFontName : "";
	fi->tall = tall > 0 ? tall : 12;
	fi->height = fi->tall;
	fi->weight = weight;
	fi->flags = flags;
	fi->antialias = (flags & FONTFLAG_ANTIALIAS) != 0;
	fi->ascent = 0;
	fi->symbol = false;
	if (fi->face)
	{
		FT_Done_Face(fi->face);
		fi->face = nullptr;
	}

	if (CsretroVguiSymbols::IsSymbolFontName(windowsFontName))
	{
		fi->ok = true;
		fi->symbol = true;
		fi->ascent = fi->tall * 3 / 4;
		fi->height = fi->tall;
		Menu_Con("CSRETRO_VGUI_SYMBOL_FONT %s tall=%d", windowsFontName, fi->tall);
		return true;
	}

	std::string path = Csretro_ResolveFontFile(windowsFontName, weight);
	if (path.empty() || FT_New_Face(g_ft, path.c_str(), 0, &fi->face) != 0)
	{
		fi->ok = false;
		return false;
	}

	// Win32 CreateFont(positive tall) = Zellhöhe (ascent+descent inkl. internal leading).
	// FreeType FT_Set_Pixel_Sizes setzt die EM-Größe — zu groß/breit vs. GDI-Tahoma.
	// REAL_DIM: ascender - descender ≈ requested tall (GDI-Zellhöhen-Semantik).
	FT_Size_RequestRec req{};
	req.type = FT_SIZE_REQUEST_TYPE_REAL_DIM;
	req.width = 0;
	req.height = static_cast<FT_Long>(fi->tall) << 6;
	req.horiResolution = 0;
	req.vertResolution = 0;
	if (FT_Request_Size(fi->face, &req) != 0)
		FT_Set_Pixel_Sizes(fi->face, 0, static_cast<FT_UInt>(fi->tall));

	const int asc = static_cast<int>(fi->face->size->metrics.ascender >> 6);
	const int desc = static_cast<int>((-fi->face->size->metrics.descender) >> 6);
	const int cell = static_cast<int>(fi->face->size->metrics.height >> 6);
	fi->ascent = asc > 0 ? asc : (fi->tall * 3 / 4);
	fi->height = cell > 0 ? cell : (asc + desc > 0 ? asc + desc : fi->tall);
	fi->ok = true;
	static std::unordered_map<std::string, bool> s_loggedMetrics;
	const std::string metricKey =
		(windowsFontName ? windowsFontName : "?") + std::to_string(fi->tall) + (fi->antialias ? "a" : "n");
	if (!s_loggedMetrics[metricKey])
	{
		s_loggedMetrics[metricKey] = true;
		Menu_Con("CSRETRO_FONT_METRICS name=%s req=%d cell=%d ascent=%d aa=%d → %s",
			windowsFontName ? windowsFontName : "?", fi->tall, fi->height, fi->ascent, fi->antialias ? 1 : 0,
			path.c_str());
	}
	return true;
}

bool CSurfaceXash::AddCustomFontFile(const char *fontFileName)
{
	if (!fontFileName || !*fontFileName)
		return false;
	// If given a file path, add its directory to the resolver search list.
	std::string path = fontFileName;
	const auto slash = path.find_last_of('/');
	if (slash != std::string::npos)
		Csretro_AddFontSearchDir(path.substr(0, slash).c_str());
	else
		Csretro_AddFontSearchDir(".");
	return true;
}

int CSurfaceXash::GetFontTall(HFont font)
{
	FontInfo *fi = GetFont(font);
	if (!fi)
		return 12;
	// Wie Win32Font::GetHeight → tmHeight (Zellhöhe), nicht nur Scheme-Request.
	return fi->height > 0 ? fi->height : fi->tall;
}

void CSurfaceXash::GetCharABCwide(HFont font, int ch, int &a, int &b, int &c)
{
	a = 0;
	b = 0;
	c = 0;
	FontInfo *fi = GetFont(font);
	if (fi && fi->ok && fi->symbol)
	{
		b = CsretroVguiSymbols::AdvanceForTall(fi->tall);
		return;
	}
	GlyphEntry *glyph = EnsureGlyph(font, static_cast<uint32_t>(ch));
	if (glyph)
	{
		a = glyph->a;
		b = glyph->b;
		c = glyph->c;
		return;
	}
	b = GetCharacterWidth(font, ch);
}

int CSurfaceXash::GetCharacterWidth(HFont font, int ch)
{
	(void)ch;
	FontInfo *fi = GetFont(font);
	if (fi && fi->ok && fi->symbol)
		return CsretroVguiSymbols::AdvanceForTall(fi->tall);
	GlyphEntry *glyph = EnsureGlyph(font, static_cast<uint32_t>(ch));
	if (glyph && glyph->advance > 0)
		return glyph->advance;
	fi = GetFont(font);
	int tall = fi ? fi->tall : 12;
	return std::max(1, tall / 2);
}

void CSurfaceXash::GetTextSize(HFont font, const wchar_t *text, int &wide, int &tall)
{
	wide = 0;
	tall = GetFontTall(font);
	if (!text)
		return;
	for (const wchar_t *p = text; *p; ++p)
		wide += GetCharacterWidth(font, *p);
}

VPANEL CSurfaceXash::GetNotifyPanel() { return 0; }
void CSurfaceXash::SetNotifyIcon(VPANEL, HTexture, VPANEL, const char *) {}

void CSurfaceXash::PlaySound(const char *fileName)
{
	if (fileName && gEng.pfnPlayLocalSound)
		gEng.pfnPlayLocalSound(fileName);
}

int CSurfaceXash::GetPopupCount() { return static_cast<int>(g_popups.size()); }
VPANEL CSurfaceXash::GetPopup(int index)
{
	if (index < 0 || index >= static_cast<int>(g_popups.size()))
		return 0;
	return g_popups[static_cast<size_t>(index)];
}

bool CSurfaceXash::ShouldPaintChildPanel(VPANEL child)
{
	// Valve-Verhalten: Popups nicht über Parent-Traverse malen — eigener Popup-Pass.
	if (child && g_pIPanel && g_pIPanel->IsPopup(child))
		return false;
	return true;
}
bool CSurfaceXash::RecreateContext(VPANEL) { return true; }
void CSurfaceXash::AddPanel(VPANEL) {}
void CSurfaceXash::ReleasePanel(VPANEL panel)
{
	g_popups.erase(std::remove(g_popups.begin(), g_popups.end(), panel), g_popups.end());
	m_minimized.erase(panel);
	m_titles.erase(panel);
}

void CSurfaceXash::MovePopupToFront(VPANEL panel)
{
	g_popups.erase(std::remove(g_popups.begin(), g_popups.end(), panel), g_popups.end());
	g_popups.push_back(panel);
}

void CSurfaceXash::MovePopupToBack(VPANEL panel)
{
	g_popups.erase(std::remove(g_popups.begin(), g_popups.end(), panel), g_popups.end());
	g_popups.insert(g_popups.begin(), panel);
}

static void InternalSchemeTraverse(VPANEL panel, bool force)
{
	if (!g_pIPanel || !panel)
		return;
	if (!g_pIPanel->IsVisible(panel) && !force)
		return;
	g_pIPanel->PerformApplySchemeSettings(panel);
	const int n = g_pIPanel->GetChildCount(panel);
	for (int i = 0; i < n; ++i)
		InternalSchemeTraverse(g_pIPanel->GetChild(panel, i), force);
}

static void InternalSolveTraverse(VPANEL panel)
{
	if (!g_pIPanel || !panel)
		return;
	if (!g_pIPanel->IsVisible(panel))
		return;
	g_pIPanel->Think(panel);
	g_pIPanel->Solve(panel);
	const int n = g_pIPanel->GetChildCount(panel);
	for (int i = 0; i < n; ++i)
		InternalSolveTraverse(g_pIPanel->GetChild(panel, i));
}

void CSurfaceXash::SolveTraverse(VPANEL panel, bool forceApplySchemeSettings)
{
	if (forceApplySchemeSettings)
		InternalSchemeTraverse(panel, true);
	InternalSolveTraverse(panel);
}

void CSurfaceXash::PaintTraverse(VPANEL panel)
{
	if (g_pIPanel && panel)
		g_pIPanel->PaintTraverse(panel, true, true);
}

void CSurfaceXash::EnableMouseCapture(VPANEL panel, bool state)
{
	m_mouseCapture = state ? panel : 0;
}

void CSurfaceXash::GetWorkspaceBounds(int &x, int &y, int &wide, int &tall)
{
	x = y = 0;
	GetScreenSize(wide, tall);
}

void CSurfaceXash::GetAbsoluteWindowBounds(int &x, int &y, int &wide, int &tall)
{
	GetWorkspaceBounds(x, y, wide, tall);
}

void CSurfaceXash::GetProportionalBase(int &width, int &height)
{
	// Classic VGUI2 / GoldSrc-Baseline: immer 640×480.
	// TrackerScheme-Keys ProportionalBaseWidthHD/HeightHD und MetaHook-HiDPI
	// sind ein späterer, bewusster CS-Retro-Modus — nicht die Classic-Gate-Basis.
	// (Kurzzeitig war hier eine HD-Umschaltung 1280×720; für Classic zurückgenommen.)
	(void)0;
	width = 640;
	height = 480;
}

void CSurfaceXash::CalculateMouseVisible() { m_cursorVisible = true; }
bool CSurfaceXash::NeedKBInput() { return true; }
bool CSurfaceXash::HasCursorPosFunctions() { return true; }
void CSurfaceXash::SurfaceGetCursorPos(int &x, int &y)
{
	x = m_cursorX;
	y = m_cursorY;
}
void CSurfaceXash::SurfaceSetCursorPos(int x, int y)
{
	m_cursorX = x;
	m_cursorY = y;
}

void CSurfaceXash::DrawTexturedPolygon(VGuiVertex *, int) {}
int CSurfaceXash::GetFontAscent(HFont font, wchar_t)
{
	FontInfo *fi = GetFont(font);
	if (fi && fi->ascent > 0)
		return fi->ascent;
	return GetFontTall(font) * 3 / 4;
}
void CSurfaceXash::SetAllowHTMLJavaScript(bool) {}
void CSurfaceXash::SetLanguage(const char *pchLang)
{
	m_language = pchLang ? pchLang : "english";
}
const char *CSurfaceXash::GetLanguage() { return m_language.c_str(); }

bool CSurfaceXash::DeleteTextureByID(int id)
{
	if (id < 0 || static_cast<size_t>(id) >= g_textures.size())
		return false;
	Texture &t = g_textures[static_cast<size_t>(id)];
	if (t.pic && gEng.pfnPIC_Free && !t.picName.empty())
		gEng.pfnPIC_Free(t.picName.c_str());
	t = Texture{};
	return true;
}

void CSurfaceXash::DrawUpdateRegionTextureBGRA(int, int, int, const unsigned char *, int, int) {}
void CSurfaceXash::DrawSetTextureBGRA(int id, const unsigned char *pchData, int wide, int tall)
{
	if (!pchData || wide <= 0 || tall <= 0)
		return;
	std::vector<unsigned char> rgba(static_cast<size_t>(wide * tall * 4));
	for (int i = 0; i < wide * tall; ++i)
	{
		rgba[static_cast<size_t>(i * 4 + 0)] = pchData[i * 4 + 2];
		rgba[static_cast<size_t>(i * 4 + 1)] = pchData[i * 4 + 1];
		rgba[static_cast<size_t>(i * 4 + 2)] = pchData[i * 4 + 0];
		rgba[static_cast<size_t>(i * 4 + 3)] = pchData[i * 4 + 3];
	}
	DrawSetTextureRGBA(id, rgba.data(), wide, tall, 0, true);
}

void CSurfaceXash::CreateBrowser(VPANEL, IHTMLResponses *, bool, const char *) {}
void CSurfaceXash::RemoveBrowser(VPANEL, IHTMLResponses *) {}
IHTMLChromeController *CSurfaceXash::AccessChromeHTMLController() { return nullptr; }

float CSurfaceXash::DrawGetAlphaMultiplier() const { return m_alphaMult; }
void CSurfaceXash::DrawSetAlphaMultiplier(float a) { m_alphaMult = a; }

void CSurfaceXash::SetCursorPosInternal(int x, int y)
{
	m_cursorX = x;
	m_cursorY = y;
}

static CSurfaceXash g_SurfaceXash;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CSurfaceXash, ISurfaceNext, VGUI_SURFACE_NEXT_INTERFACE_VERSION, g_SurfaceXash);
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CSurfaceXash, ISurface, VGUI_SURFACE_INTERFACE_VERSION_GS, g_SurfaceXash);

namespace vgui2
{
ISurfaceNext *g_pSurfaceNext = &g_SurfaceXash;
ISurface *g_pSurface = &g_SurfaceXash;
}

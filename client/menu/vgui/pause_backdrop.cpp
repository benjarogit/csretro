#include "../src/menu_priv.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <vector>

namespace
{
constexpr unsigned int kGlRgba = 0x1908;
constexpr unsigned int kGlUnsignedByte = 0x1401;
constexpr unsigned int kGlPackAlignment = 0x0D05;
constexpr int kDownsample = 8;
constexpr int kBlurRadius = 2;
constexpr int kPicFlags = PIC_NOMIPMAP | PIC_HAS_ALPHA;

#pragma pack(push, 1)
struct TgaHeader
{
	uint8_t id_length, colormap_type, image_type;
	uint16_t cm_first, cm_length;
	uint8_t cm_size;
	uint16_t x_origin, y_origin, width, height;
	uint8_t pixel_size, attributes;
};
#pragma pack(pop)

using GlReadPixelsFn = void (*)(int, int, int, int, unsigned int, unsigned int, void *);
using GlPixelStoreiFn = void (*)(unsigned int, int);

void *g_gl = nullptr;
GlReadPixelsFn g_readPixels = nullptr;
GlPixelStoreiFn g_pixelStorei = nullptr;
HIMAGE g_blurPic = 0;
int g_blurW = 0;
int g_blurH = 0;
bool g_logged = false;

void EnsureGl()
{
	if (g_gl || g_readPixels)
		return;
	g_gl = dlopen("libGL.so.1", RTLD_NOW | RTLD_GLOBAL);
	if (!g_gl)
		g_gl = dlopen("libGL.so", RTLD_NOW | RTLD_GLOBAL);
	if (!g_gl)
		return;
	g_readPixels = reinterpret_cast<GlReadPixelsFn>(dlsym(g_gl, "glReadPixels"));
	g_pixelStorei = reinterpret_cast<GlPixelStoreiFn>(dlsym(g_gl, "glPixelStorei"));
}

void BoxBlur(std::vector<uint8_t> &img, int w, int h)
{
	std::vector<uint8_t> tmp(img.size());
	const int r = kBlurRadius;
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			int sr = 0, sg = 0, sb = 0, n = 0;
			for (int k = -r; k <= r; ++k)
			{
				const int xx = std::clamp(x + k, 0, w - 1);
				const uint8_t *p = &img[static_cast<size_t>((y * w + xx) * 4)];
				sr += p[0];
				sg += p[1];
				sb += p[2];
				++n;
			}
			uint8_t *o = &tmp[static_cast<size_t>((y * w + x) * 4)];
			o[0] = static_cast<uint8_t>(sr / n);
			o[1] = static_cast<uint8_t>(sg / n);
			o[2] = static_cast<uint8_t>(sb / n);
			o[3] = 255;
		}
	}
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			int sr = 0, sg = 0, sb = 0, n = 0;
			for (int k = -r; k <= r; ++k)
			{
				const int yy = std::clamp(y + k, 0, h - 1);
				const uint8_t *p = &tmp[static_cast<size_t>((yy * w + x) * 4)];
				sr += p[0];
				sg += p[1];
				sb += p[2];
				++n;
			}
			uint8_t *o = &img[static_cast<size_t>((y * w + x) * 4)];
			o[0] = static_cast<uint8_t>(sr / n);
			o[1] = static_cast<uint8_t>(sg / n);
			o[2] = static_cast<uint8_t>(sb / n);
			o[3] = 255;
		}
	}
}

std::vector<uint8_t> RgbaToTga(const uint8_t *rgba, int w, int h)
{
	std::vector<uint8_t> buf(sizeof(TgaHeader) + static_cast<size_t>(w * h * 4));
	auto *hdr = reinterpret_cast<TgaHeader *>(buf.data());
	std::memset(hdr, 0, sizeof(*hdr));
	hdr->image_type = 2;
	hdr->width = static_cast<uint16_t>(w);
	hdr->height = static_cast<uint16_t>(h);
	hdr->pixel_size = 32;
	hdr->attributes = 0x28;
	uint8_t *dst = buf.data() + sizeof(TgaHeader);
	for (int i = 0; i < w * h; ++i)
	{
		dst[i * 4 + 0] = rgba[i * 4 + 2];
		dst[i * 4 + 1] = rgba[i * 4 + 1];
		dst[i * 4 + 2] = rgba[i * 4 + 0];
		dst[i * 4 + 3] = 255;
	}
	return buf;
}

bool Capture(int sw, int sh)
{
	EnsureGl();
	if (!g_readPixels || sw < 16 || sh < 16)
		return false;

	std::vector<uint8_t> full(static_cast<size_t>(sw * sh * 4));
	if (g_pixelStorei)
		g_pixelStorei(kGlPackAlignment, 1);
	g_readPixels(0, 0, sw, sh, kGlRgba, kGlUnsignedByte, full.data());

	const int dw = std::max(1, sw / kDownsample);
	const int dh = std::max(1, sh / kDownsample);
	std::vector<uint8_t> small(static_cast<size_t>(dw * dh * 4));
	for (int y = 0; y < dh; ++y)
	{
		const int srcY = (dh - 1 - y) * sh / dh;
		for (int x = 0; x < dw; ++x)
		{
			const int srcX = x * sw / dw;
			const uint8_t *s = &full[static_cast<size_t>((srcY * sw + srcX) * 4)];
			uint8_t *d = &small[static_cast<size_t>((y * dw + x) * 4)];
			d[0] = s[0];
			d[1] = s[1];
			d[2] = s[2];
			d[3] = 255;
		}
	}
	BoxBlur(small, dw, dh);
	BoxBlur(small, dw, dh);

	if (!gEng.pfnPIC_Load)
		return false;
	if (g_blurPic && gEng.pfnPIC_Free)
		gEng.pfnPIC_Free("#csretro_pause_blur.tga");
	auto tga = RgbaToTga(small.data(), dw, dh);
	g_blurPic = gEng.pfnPIC_Load("#csretro_pause_blur.tga", tga.data(), static_cast<int>(tga.size()),
		kPicFlags);
	if (!g_blurPic)
		return false;
	g_blurW = dw;
	g_blurH = dh;
	return true;
}
} // namespace

void PauseBackdrop_Invalidate()
{
	if (g_blurPic && gEng.pfnPIC_Free)
		gEng.pfnPIC_Free("#csretro_pause_blur.tga");
	g_blurPic = 0;
	g_blurW = 0;
	g_blurH = 0;
	g_logged = false;
}

bool PauseBackdrop_IsBlurred()
{
	return g_blurPic != 0;
}

void PauseBackdrop_Paint()
{
	const int sw = gGlobals ? gGlobals->scrWidth : 640;
	const int sh = gGlobals ? gGlobals->scrHeight : 480;
	// This full-screen pass runs before VGUI starts its next clip stack. A
	// previous child panel (notably a 3D model preview) may have left the menu
	// image scissor at its own bounds, which otherwise exposes a world strip.
	if (gEng.pfnPIC_DisableScissor)
		gEng.pfnPIC_DisableScissor();
	if (!g_blurPic)
		Capture(sw, sh);

	if (g_blurPic && gEng.pfnPIC_Set && gEng.pfnPIC_Draw)
	{
		gEng.pfnPIC_Set(g_blurPic, 255, 255, 255, 255);
		gEng.pfnPIC_Draw(0, 0, sw, sh, nullptr);
		if (gEng.pfnFillRGBA)
			gEng.pfnFillRGBA(0, 0, sw, sh, 0, 0, 0, 88);
		if (!g_logged)
		{
			Menu_Con("CSRETRO_PAUSE_BG blur=1 wallpaper=0 capture=%dx%d", g_blurW, g_blurH);
			g_logged = true;
		}
		return;
	}

	if (gEng.pfnFillRGBA)
		gEng.pfnFillRGBA(0, 0, sw, sh, 0, 0, 0, 160);
	if (!g_logged)
	{
		Menu_Con("CSRETRO_PAUSE_BG blur=0 wallpaper=0 fallback=1");
		g_logged = true;
	}
}

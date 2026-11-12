#include "xash_surface/xash_surface.h"

#include "xash_engine/xash_engine.h"

#include "font_atlas.h"
#include "font_face.h"
#include "surface_map.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include <GL/gl.h>

#include "Color.h"
#include <vgui/Cursor.h>
#include <vgui/ISurface.h>

#if defined(__linux__)
#include <dlfcn.h>
#endif

namespace
{

    constexpr int kTextureCap = 4096;
    constexpr int kFontCap = 256;
    constexpr int kMaxTextureEdge = 4096;
    constexpr int kMaxTextureFile = 8 * 1024 * 1024;

    bool DecodeUncompressedTga(const unsigned char* data, int size, std::vector<unsigned char>* rgba, int* wide, int* tall)
    {
        if (data == nullptr || rgba == nullptr || wide == nullptr || tall == nullptr || size < 18)
        {
            return false;
        }
        const int id_length = data[0];
        const int color_map = data[1];
        const int image_type = data[2];
        if (color_map != 0 || image_type != 2)
        {
            return false;
        }
        const int image_wide = data[12] | (data[13] << 8);
        const int image_tall = data[14] | (data[15] << 8);
        const int bpp = data[16];
        if (image_wide <= 0 || image_tall <= 0 || image_wide > kMaxTextureEdge || image_tall > kMaxTextureEdge)
        {
            return false;
        }
        if (bpp != 24 && bpp != 32)
        {
            return false;
        }
        const int stride = bpp / 8;
        const int payload = image_wide * image_tall * stride;
        if (payload <= 0 || payload > kMaxTextureFile || id_length < 0 || 18 + id_length > size || payload > size - 18 - id_length)
        {
            return false;
        }
        rgba->assign(static_cast<size_t>(image_wide) * static_cast<size_t>(image_tall) * 4, 0);
        const unsigned char* src = data + 18 + id_length;
        const bool top_origin = (data[17] & 0x20) != 0;
        for (int y = 0; y < image_tall; ++y)
        {
            const int dst_y = top_origin ? y : image_tall - 1 - y;
            for (int x = 0; x < image_wide; ++x)
            {
                const unsigned char* pixel = src + (static_cast<size_t>(y) * static_cast<size_t>(image_wide) + static_cast<size_t>(x)) * static_cast<size_t>(stride);
                unsigned char* dst = rgba->data() + (static_cast<size_t>(dst_y) * static_cast<size_t>(image_wide) + static_cast<size_t>(x)) * 4;
                dst[0] = pixel[2];
                dst[1] = pixel[1];
                dst[2] = pixel[0];
                dst[3] = stride == 4 ? pixel[3] : 255;
            }
        }
        *wide = image_wide;
        *tall = image_tall;
        return true;
    }
    constexpr int kProportionalWide = 640;
    constexpr int kProportionalTall = 480;

    bool HasGlContext()
    {
#if defined(__linux__)
        using CurrentFn = void* (*)();
        static CurrentFn egl_current = nullptr;
        static CurrentFn glx_current = nullptr;
        static bool resolved = false;
        if (!resolved)
        {
            resolved = true;
            if (void* egl = dlopen("libEGL.so.1", RTLD_LAZY | RTLD_LOCAL))
            {
                egl_current = reinterpret_cast<CurrentFn>(dlsym(egl, "eglGetCurrentContext"));
            }
            if (void* gl = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL))
            {
                glx_current = reinterpret_cast<CurrentFn>(dlsym(gl, "glXGetCurrentContext"));
            }
        }
        if (egl_current != nullptr && egl_current() != nullptr)
        {
            return true;
        }
        if (glx_current != nullptr && glx_current() != nullptr)
        {
            return true;
        }
        return false;
#elif defined(_WIN32)
        return wglGetCurrentContext() != nullptr;
#else
        return false;
#endif
    }

    void DrainGlErrors()
    {
        if (!HasGlContext())
        {
            return;
        }
        for (int i = 0; i < 8 && glGetError() != GL_NO_ERROR; ++i)
        {
        }
    }

    // Restores the attributes a draw changes. Leaving GL_TEXTURE_2D disabled
    // makes the engine's next 2D batch draw white quads over the frame.
    struct GlScope
    {
        int matrix_mode{};

        GlScope(int wide, int tall, const xash::SurfaceFrame& frame)
        {
            glPushAttrib(GL_ALL_ATTRIB_BITS);
            glGetIntegerv(GL_MATRIX_MODE, &matrix_mode);
            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            glOrtho(0.0, static_cast<double>(wide), static_cast<double>(tall), 0.0, -1.0, 1.0);
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();
            glViewport(0, 0, wide, tall);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            if (frame.clip)
            {
                const int clip_wide = frame.clip_x1 - frame.clip_x0;
                const int clip_tall = frame.clip_y1 - frame.clip_y0;
                glEnable(GL_SCISSOR_TEST);
                glScissor(frame.clip_x0, tall - frame.clip_y1, clip_wide, clip_tall);
            }
            else
            {
                glDisable(GL_SCISSOR_TEST);
            }
        }

        ~GlScope()
        {
            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
            glMatrixMode(matrix_mode);
            glPopAttrib();
        }
    };

    void* PanelKey(vgui2::VPANEL panel)
    {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(panel));
    }

} // namespace

namespace xash
{

    class XashSurface : public vgui2::ISurface
    {
    public:
        explicit XashSurface(const csr_surface_host_t* host)
        {
            language_ = "english";
            if (host != nullptr)
            {
                host_ = *host;
                screen_wide_ = host->screen_wide;
                screen_tall_ = host->screen_tall;
            }
            textures_.resize(1);
            fonts_.resize(1);
        }

        ~XashSurface() override
        {
            if (current_ == this)
            {
                current_ = nullptr;
            }
            ReleaseGlTextures();
        }

        static XashSurface* current()
        {
            return current_;
        }

        static void set_current(XashSurface* surface)
        {
            current_ = surface;
        }

        int gl_error() const
        {
            return gl_error_;
        }

        void MoveCursor(int x, int y)
        {
            cursor_x_ = x;
            cursor_y_ = y;
            if (input_.cursor_moved != nullptr)
            {
                input_.cursor_moved(x, y, input_.user);
            }
            DispatchInput();
        }

        void SetButton(int button, int down)
        {
            button_ = button;
            button_down_ = down != 0;
            if (down != 0)
            {
                if (input_.mouse_pressed != nullptr)
                {
                    input_.mouse_pressed(button, input_.user);
                }
            }
            else if (input_.mouse_released != nullptr)
            {
                input_.mouse_released(button, input_.user);
            }
            DispatchInput();
        }

        void BeginFrame(int wide, int tall)
        {
            screen_wide_ = wide;
            screen_tall_ = tall;
            gl_error_ = 0;
            DrainGlErrors();
        }

        void EndFrame()
        {
            if (!HasGlContext())
            {
                gl_error_ = 0;
                return;
            }
            gl_error_ = static_cast<int>(glGetError());
        }

        void Shutdown() override
        {
            ReleaseGlTextures();
            stack_.clear();
        }

        // CVGui::RunFrame follows this with UpdateMouseFocus(GetCursorPos()).
        // A headless host must not take that path: it resets focus to the
        // stored cursor, which used to be 0,0. Input arrives through the
        // input table instead.
        void RunFrame() override {}

        vgui2::VPANEL GetEmbeddedPanel() override
        {
            return embedded_;
        }

        void SetEmbeddedPanel(vgui2::VPANEL panel) override
        {
            embedded_ = panel;
        }

        void PushMakeCurrent(vgui2::VPANEL panel, bool useInsets) override
        {
            SurfaceFrame frame;
            int x = 0;
            int y = 0;
            if (panel != 0 && host_.panel_abs_pos != nullptr)
            {
                host_.panel_abs_pos(PanelKey(panel), &x, &y, host_.user);
            }
            if (useInsets && panel != 0 && host_.panel_insets != nullptr)
            {
                int left = 0;
                int top = 0;
                int right = 0;
                int bottom = 0;
                host_.panel_insets(PanelKey(panel), &left, &top, &right, &bottom, host_.user);
                x += left;
                y += top;
            }
            frame.origin_x = x;
            frame.origin_y = y;
            if (panel != 0 && host_.panel_clip_rect != nullptr)
            {
                frame.clip = true;
                host_.panel_clip_rect(PanelKey(panel), &frame.clip_x0, &frame.clip_y0, &frame.clip_x1, &frame.clip_y1, host_.user);
            }
            stack_.push_back(frame);
            ++push_count_;
        }

        int PushCount() const
        {
            return push_count_;
        }

        void PopMakeCurrent(vgui2::VPANEL) override
        {
            if (!stack_.empty())
            {
                stack_.pop_back();
            }
        }

        void DrawSetColor(int r, int g, int b, int a) override
        {
            draw_r_ = r;
            draw_g_ = g;
            draw_b_ = b;
            draw_a_ = a;
        }

        void DrawSetColor(Color col) override
        {
            DrawSetColor(col.r(), col.g(), col.b(), col.a());
        }

        void DrawFilledRect(int x0, int y0, int x1, int y1) override
        {
            DrawQuad(x0, y0, x1, y1, draw_r_, draw_g_, draw_b_, draw_a_, 0, false, false);
        }

        void DrawOutlinedRect(int x0, int y0, int x1, int y1) override
        {
            DrawFilledRect(x0, y0, x1, y0 + 1);
            DrawFilledRect(x0, y1 - 1, x1, y1);
            DrawFilledRect(x0, y0, x0 + 1, y1);
            DrawFilledRect(x1 - 1, y0, x1, y1);
        }

        void DrawLine(int x0, int y0, int x1, int y1) override
        {
            if (!HasGlContext() || screen_wide_ <= 0 || screen_tall_ <= 0)
            {
                return;
            }
            const SurfaceFrame frame = CurrentFrame();
            GlScope scope(screen_wide_, screen_tall_, frame);
            glDisable(GL_TEXTURE_2D);
            glColor4ub(
                static_cast<unsigned char>(draw_r_),
                static_cast<unsigned char>(draw_g_),
                static_cast<unsigned char>(draw_b_),
                static_cast<unsigned char>(VguiAlphaToGl(draw_a_))
            );
            glBegin(GL_LINES);
            glVertex2i(x0 + frame.origin_x, y0 + frame.origin_y);
            glVertex2i(x1 + frame.origin_x, y1 + frame.origin_y);
            glEnd();
        }

        void DrawPolyLine(int* px, int* py, int numPoints) override
        {
            if (px == nullptr || py == nullptr || numPoints < 2)
            {
                return;
            }
            for (int i = 0; i < numPoints - 1; ++i)
            {
                DrawLine(px[i], py[i], px[i + 1], py[i + 1]);
            }
        }

        void DrawSetTextFont(vgui2::HFont font) override
        {
            text_font_ = font;
        }

        void DrawSetTextColor(int r, int g, int b, int a) override
        {
            text_r_ = r;
            text_g_ = g;
            text_b_ = b;
            text_a_ = a;
        }

        void DrawSetTextColor(Color col) override
        {
            DrawSetTextColor(col.r(), col.g(), col.b(), col.a());
        }

        void DrawSetTextPos(int x, int y) override
        {
            text_x_ = x;
            text_y_ = y;
        }

        void DrawGetTextPos(int& x, int& y) override
        {
            x = text_x_;
            y = text_y_;
        }

        void DrawPrintText(const wchar_t* text, int textLen) override
        {
            if (text == nullptr || textLen <= 0)
            {
                return;
            }
            for (int i = 0; i < textLen; ++i)
            {
                DrawUnicodeChar(text[i]);
            }
        }

        void DrawUnicodeChar(wchar_t wch) override
        {
            DrawCharacter(wch, false);
        }

        void DrawUnicodeCharAdd(wchar_t wch) override
        {
            DrawCharacter(wch, true);
        }

        void DrawFlushText() override {}

        vgui2::IHTML* CreateHTMLWindow(vgui2::IHTMLEvents*, vgui2::VPANEL) override
        {
            return nullptr;
        }

        void PaintHTMLWindow(vgui2::IHTML*) override {}

        void DeleteHTMLWindow(vgui2::IHTML*) override {}

        bool ReadTextureFile(const char* path, std::vector<unsigned char>* bytes) const
        {
            if (path == nullptr || path[0] == '\0' || bytes == nullptr || file_.read == nullptr)
            {
                return false;
            }
            const int size = file_.read(path, nullptr, 0, file_.user);
            if (size <= 0 || size > kMaxTextureFile)
            {
                return false;
            }
            bytes->assign(static_cast<size_t>(size), 0);
            return file_.read(path, bytes->data(), size, file_.user) == size;
        }

        void DrawSetTextureFile(int id, const char* filename, int hardwareFilter, bool) override
        {
            // GameUI strips ".tga" and asks for the stem. The file on disk keeps it.
            std::vector<unsigned char> bytes;
            if (!ReadTextureFile(filename, &bytes))
            {
                const char* slash = filename == nullptr ? nullptr : std::strrchr(filename, '/');
                const char* dot = filename == nullptr ? nullptr : std::strrchr(filename, '.');
                if (filename == nullptr || (dot != nullptr && (slash == nullptr || dot > slash)))
                {
                    return;
                }
                const std::string with_tga = std::string(filename) + ".tga";
                if (!ReadTextureFile(with_tga.c_str(), &bytes))
                {
                    return;
                }
            }
            std::vector<unsigned char> rgba;
            int wide = 0;
            int tall = 0;
            if (!DecodeUncompressedTga(bytes.data(), static_cast<int>(bytes.size()), &rgba, &wide, &tall))
            {
                return;
            }
            StoreTexture(id, rgba.data(), wide, tall, hardwareFilter != 0, false);
        }

        void DrawSetTextureRGBA(int id, const unsigned char* rgba, int wide, int tall, int hardwareFilter, bool) override
        {
            StoreTexture(id, rgba, wide, tall, hardwareFilter != 0, false);
        }

        void DrawSetTexture(int id) override
        {
            current_texture_ = id;
        }

        void DrawGetTextureSize(int id, int& wide, int& tall) override
        {
            wide = 0;
            tall = 0;
            if (id > 0 && id < static_cast<int>(textures_.size()) && textures_[static_cast<size_t>(id)].valid)
            {
                wide = textures_[static_cast<size_t>(id)].wide;
                tall = textures_[static_cast<size_t>(id)].tall;
            }
        }

        void DrawTexturedRect(int x0, int y0, int x1, int y1) override
        {
            const Texture* texture = FindTexture(current_texture_);
            if (texture == nullptr)
            {
                return;
            }
            DrawQuad(x0, y0, x1, y1, draw_r_, draw_g_, draw_b_, draw_a_, texture->gl_name, true, false);
        }

        bool IsTextureIDValid(int id) override
        {
            return id > 0 && id < static_cast<int>(textures_.size()) && textures_[static_cast<size_t>(id)].valid;
        }

        int CreateNewTextureID(bool) override
        {
            const int id = static_cast<int>(textures_.size());
            if (id >= kTextureCap)
            {
                return 0;
            }
            textures_.emplace_back();
            return id;
        }

        void GetScreenSize(int& wide, int& tall) override
        {
            wide = screen_wide_;
            tall = screen_tall_;
        }

        void SetAsTopMost(vgui2::VPANEL, bool) override {}
        void BringToFront(vgui2::VPANEL) override {}
        void SetForegroundWindow(vgui2::VPANEL) override {}
        void SetPanelVisible(vgui2::VPANEL, bool) override {}
        void SetMinimized(vgui2::VPANEL, bool) override {}

        bool IsMinimized(vgui2::VPANEL) override
        {
            return false;
        }

        void FlashWindow(vgui2::VPANEL, bool) override {}
        void SetTitle(vgui2::VPANEL, const wchar_t*) override {}
        void SetAsToolBar(vgui2::VPANEL, bool) override {}
        void CreatePopup(vgui2::VPANEL, bool, bool, bool, bool, bool) override {}
        void SwapBuffers(vgui2::VPANEL) override {}
        void Invalidate(vgui2::VPANEL) override {}

        void SetCursor(vgui2::HCursor cursor) override
        {
            cursor_ = cursor;
        }

        bool IsCursorVisible() override
        {
            return cursor_ != 0;
        }

        void ApplyChanges() override {}

        bool IsWithin(int x, int y) override
        {
            return x >= 0 && y >= 0 && x < screen_wide_ && y < screen_tall_;
        }

        // A headless host has no OS window focus. VGUI drops keyboard input when this is false.
        bool HasFocus() override
        {
            return true;
        }

        bool SupportsFeature(SurfaceFeature_e) override
        {
            return false;
        }

        void RestrictPaintToSinglePanel(vgui2::VPANEL panel) override
        {
            restricted_ = panel;
        }

        void SetModalPanel(vgui2::VPANEL panel) override
        {
            modal_ = panel;
        }

        vgui2::VPANEL GetModalPanel() override
        {
            return modal_;
        }

        void UnlockCursor() override
        {
            cursor_locked_ = false;
        }

        void LockCursor() override
        {
            cursor_locked_ = true;
        }

        void SetTranslateExtendedKeys(bool state) override
        {
            translate_keys_ = state;
        }

        vgui2::VPANEL GetTopmostPopup() override
        {
            return 0;
        }

        void SetTopLevelFocus(vgui2::VPANEL) override {}

        vgui2::HFont CreateFont() override
        {
            if (static_cast<int>(fonts_.size()) >= kFontCap)
            {
                return 0;
            }
            fonts_.emplace_back();
            return static_cast<vgui2::HFont>(fonts_.size() - 1);
        }

        bool AddGlyphSetToFont(vgui2::HFont font, const char* windows_name, int tall, int weight, int, int, int, int low, int high) override
        {
            Font* slot = FindFont(font);
            if (slot == nullptr || tall <= 0)
            {
                return false;
            }
            const int pixel_tall = slot->has_set ? slot->tall : tall;
            const std::string path = ResolveFontPath(windows_name, weight);
            Face face = faces_.Open(path, pixel_tall);
            if (!face)
            {
                return false;
            }
            // The first set defines the face and the height. Later sets only extend coverage.
            if (!slot->has_set)
            {
                slot->tall = pixel_tall;
                slot->ascent = face.ascent();
                slot->has_set = true;
            }
            FaceUse use;
            use.face = std::move(face);
            use.full = low == 0 && high == 0;
            use.low = low;
            use.high = high;
            slot->uses.push_back(std::move(use));
            return true;
        }

        bool AddCustomFontFile(const char*) override
        {
            return false;
        }

        int GetFontTall(vgui2::HFont font) override
        {
            const Font* slot = FindFont(font);
            if (slot == nullptr || !slot->has_set)
            {
                return 0;
            }
            return slot->tall;
        }

        void GetCharABCwide(vgui2::HFont font, int ch, int& a, int& b, int& c) override
        {
            a = 0;
            c = 0;
            b = CharacterWidth(font, static_cast<unsigned>(ch));
        }

        int GetCharacterWidth(vgui2::HFont font, int ch) override
        {
            return CharacterWidth(font, static_cast<unsigned>(ch));
        }

        void GetTextSize(vgui2::HFont font, const wchar_t* text, int& wide, int& tall) override
        {
            wide = 0;
            tall = GetFontTall(font);
            if (text == nullptr)
            {
                return;
            }
            for (const wchar_t* cursor = text; *cursor != 0; ++cursor)
            {
                wide += CharacterWidth(font, static_cast<unsigned>(*cursor));
            }
        }

        vgui2::VPANEL GetNotifyPanel() override
        {
            return 0;
        }

        void SetNotifyIcon(vgui2::VPANEL, vgui2::HTexture, vgui2::VPANEL, const char*) override {}

        void PlaySound(const char*) override {}

        int GetPopupCount() override
        {
            return static_cast<int>(popups_.size());
        }

        vgui2::VPANEL GetPopup(int index) override
        {
            if (index < 0 || index >= static_cast<int>(popups_.size()))
            {
                return 0;
            }
            return popups_[static_cast<size_t>(index)];
        }

        bool ShouldPaintChildPanel(vgui2::VPANEL child) override
        {
            if (panel_.is_popup == nullptr || child == 0)
            {
                return true;
            }
            return panel_.is_popup(PanelKey(child), panel_.user) == 0;
        }

        bool RecreateContext(vgui2::VPANEL) override
        {
            return true;
        }

        void AddPanel(vgui2::VPANEL) override {}

        void ReleasePanel(vgui2::VPANEL panel) override
        {
            ErasePopup(panel);
        }

        void MovePopupToFront(vgui2::VPANEL panel) override
        {
            if (!IsPopup(panel))
            {
                ErasePopup(panel);
                return;
            }
            ErasePopup(panel);
            popups_.push_back(panel);
        }

        void MovePopupToBack(vgui2::VPANEL panel) override
        {
            if (!IsPopup(panel))
            {
                ErasePopup(panel);
                return;
            }
            ErasePopup(panel);
            popups_.insert(popups_.begin(), panel);
        }

        void SolveTraverse(vgui2::VPANEL panel, bool forceApplySchemeSettings) override
        {
            SolveNode(panel, forceApplySchemeSettings, 0);
            if (panel != 0 && panel == embedded_)
            {
                for (const vgui2::VPANEL popup : popups_)
                {
                    if (popup != panel)
                    {
                        SolveNode(popup, forceApplySchemeSettings, 0);
                    }
                }
            }
        }

        // One entry into the panel. vgui2 walks children from there and calls
        // back into PushMakeCurrent. Popups were excluded by ShouldPaintChildPanel.
        void PaintTraverse(vgui2::VPANEL panel) override
        {
            if (PaintNode(panel))
            {
                for (const vgui2::VPANEL popup : popups_)
                {
                    if (popup == panel || !IsPopup(popup))
                    {
                        continue;
                    }
                    PaintNode(popup);
                }
            }
            PaintCursor();
        }

        void SetCursorTexture(int cursor, int texture)
        {
            if (cursor <= vgui2::dc_none || cursor >= vgui2::dc_last)
            {
                return;
            }
            if (static_cast<int>(cursor_texture_.size()) < vgui2::dc_last)
            {
                cursor_texture_.resize(static_cast<size_t>(vgui2::dc_last));
            }
            cursor_texture_[static_cast<size_t>(cursor)] = texture;
        }

        int CursorQuad(int* x, int* y, int* wide, int* tall, int* texture) const
        {
            if (!cursor_quad_.drawn)
            {
                return 0;
            }
            if (x != nullptr)
            {
                *x = cursor_quad_.x;
            }
            if (y != nullptr)
            {
                *y = cursor_quad_.y;
            }
            if (wide != nullptr)
            {
                *wide = cursor_quad_.wide;
            }
            if (tall != nullptr)
            {
                *tall = cursor_quad_.tall;
            }
            if (texture != nullptr)
            {
                *texture = cursor_quad_.texture;
            }
            return 1;
        }

        void SetPanelFns(const csr_surface_panel_fns_t* panel)
        {
            if (panel == nullptr)
            {
                panel_ = {};
                return;
            }
            panel_ = *panel;
        }

        void SetInputFns(const csr_surface_input_fns_t* input)
        {
            if (input == nullptr)
            {
                input_ = {};
                return;
            }
            input_ = *input;
        }

        void SetFileFns(const csr_surface_file_fns_t* file)
        {
            if (file == nullptr)
            {
                file_ = {};
                return;
            }
            file_ = *file;
        }

        void EnableMouseCapture(vgui2::VPANEL, bool) override {}

        void GetWorkspaceBounds(int& x, int& y, int& wide, int& tall) override
        {
            x = 0;
            y = 0;
            wide = screen_wide_;
            tall = screen_tall_;
        }

        void GetAbsoluteWindowBounds(int& x, int& y, int& wide, int& tall) override
        {
            GetWorkspaceBounds(x, y, wide, tall);
        }

        void GetProportionalBase(int& width, int& height) override
        {
            width = kProportionalWide;
            height = kProportionalTall;
        }

        void CalculateMouseVisible() override {}

        bool NeedKBInput() override
        {
            return false;
        }

        bool HasCursorPosFunctions() override
        {
            return true;
        }

        void SurfaceGetCursorPos(int& x, int& y) override
        {
            x = cursor_x_;
            y = cursor_y_;
        }

        void SurfaceSetCursorPos(int x, int y) override
        {
            cursor_x_ = x;
            cursor_y_ = y;
        }

        void DrawTexturedPolygon(vgui2::VGuiVertex* vertices, int count) override
        {
            if (vertices == nullptr || count < 3 || !HasGlContext() || screen_wide_ <= 0 || screen_tall_ <= 0)
            {
                return;
            }
            const Texture* texture = FindTexture(current_texture_);
            const SurfaceFrame frame = CurrentFrame();
            GlScope scope(screen_wide_, screen_tall_, frame);
            if (texture != nullptr && texture->gl_name != 0)
            {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, texture->gl_name);
            }
            else
            {
                glDisable(GL_TEXTURE_2D);
            }
            glColor4ub(
                static_cast<unsigned char>(draw_r_),
                static_cast<unsigned char>(draw_g_),
                static_cast<unsigned char>(draw_b_),
                static_cast<unsigned char>(VguiAlphaToGl(draw_a_))
            );
            glBegin(GL_TRIANGLE_FAN);
            for (int i = 0; i < count; ++i)
            {
                glTexCoord2f(vertices[i].u, vertices[i].v);
                glVertex2i(vertices[i].x + frame.origin_x, vertices[i].y + frame.origin_y);
            }
            glEnd();
        }

        int GetFontAscent(vgui2::HFont font, wchar_t) override
        {
            const Font* slot = FindFont(font);
            if (slot == nullptr || !slot->has_set)
            {
                return 0;
            }
            return slot->ascent;
        }

        void SetAllowHTMLJavaScript(bool) override {}

        void SetLanguage(const char* language) override
        {
            language_ = language != nullptr ? language : "";
        }

        const char* GetLanguage() override
        {
            return language_.c_str();
        }

        bool DeleteTextureByID(int id) override
        {
            if (id <= 0 || id >= static_cast<int>(textures_.size()))
            {
                return false;
            }
            ReleaseTexture(textures_[static_cast<size_t>(id)]);
            return true;
        }

        void DrawUpdateRegionTextureBGRA(int id, int, int, const unsigned char* data, int wide, int tall) override
        {
            DrawSetTextureBGRA(id, data, wide, tall);
        }

        void DrawSetTextureBGRA(int id, const unsigned char* data, int wide, int tall) override
        {
            StoreTexture(id, data, wide, tall, false, true);
        }

        void CreateBrowser(vgui2::VPANEL, IHTMLResponses*, bool, const char*) override {}

        void RemoveBrowser(vgui2::VPANEL, IHTMLResponses*) override {}

        IHTMLChromeController* AccessChromeHTMLController() override
        {
            return nullptr;
        }

    private:
        struct Texture
        {
            bool valid{};
            bool filter{};
            int wide{};
            int tall{};
            unsigned gl_name{};
            std::vector<unsigned char> rgba;
        };

        struct Glyph
        {
            int advance{};
            int left{};
            int top{};
            int width{};
            int height{};
            AtlasGlyph atlas{};
        };

        struct FaceUse
        {
            Face face;
            int low{};
            int high{};
            bool full{};

            bool Covers(unsigned codepoint) const
            {
                if (full)
                {
                    return true;
                }
                return codepoint >= static_cast<unsigned>(low) && codepoint <= static_cast<unsigned>(high);
            }
        };

        struct Font
        {
            bool has_set{};
            int tall{};
            int ascent{};
            std::vector<FaceUse> uses;
            std::unordered_map<unsigned, Glyph> glyphs;
        };

        SurfaceFrame CurrentFrame() const
        {
            if (stack_.empty())
            {
                return {};
            }
            return stack_.back();
        }

        void DrawQuad(
            int x0,
            int y0,
            int x1,
            int y1,
            int r,
            int g,
            int b,
            int a,
            unsigned texture,
            bool textured,
            bool additive,
            float u0 = 0.0f,
            float v0 = 0.0f,
            float u1 = 1.0f,
            float v1 = 1.0f
        )
        {
            if (!HasGlContext() || screen_wide_ <= 0 || screen_tall_ <= 0)
            {
                return;
            }
            MappedQuad quad;
            const SurfaceFrame frame = CurrentFrame();
            if (!MapQuad(frame, x0, y0, x1, y1, &quad, u0, v0, u1, v1))
            {
                return;
            }
            if (textured && texture == 0)
            {
                return;
            }
            GlScope scope(screen_wide_, screen_tall_, frame);
            if (additive)
            {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            }
            if (textured)
            {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, texture);
            }
            else
            {
                glDisable(GL_TEXTURE_2D);
            }
            glColor4ub(
                static_cast<unsigned char>(r),
                static_cast<unsigned char>(g),
                static_cast<unsigned char>(b),
                static_cast<unsigned char>(VguiAlphaToGl(a))
            );
            glBegin(GL_QUADS);
            glTexCoord2f(quad.u0, quad.v0);
            glVertex2f(quad.x0, quad.y0);
            glTexCoord2f(quad.u1, quad.v0);
            glVertex2f(quad.x1, quad.y0);
            glTexCoord2f(quad.u1, quad.v1);
            glVertex2f(quad.x1, quad.y1);
            glTexCoord2f(quad.u0, quad.v1);
            glVertex2f(quad.x0, quad.y1);
            glEnd();
        }

        void DrawCharacter(wchar_t wch, bool additive)
        {
            Font* slot = FindFont(text_font_);
            if (slot == nullptr || !slot->has_set)
            {
                return;
            }
            const Glyph& glyph = EnsureGlyph(*slot, static_cast<unsigned>(wch));
            if (glyph.width > 0 && glyph.height > 0 && glyph.atlas.placed && wch >= 32)
            {
                const unsigned texture = UploadPage(glyph.atlas.page);
                const int x = text_x_ + glyph.left;
                const int y = text_y_ + slot->ascent - glyph.top;
                DrawQuad(
                    x,
                    y,
                    x + glyph.width,
                    y + glyph.height,
                    text_r_,
                    text_g_,
                    text_b_,
                    text_a_,
                    texture,
                    true,
                    additive,
                    glyph.atlas.u0,
                    glyph.atlas.v0,
                    glyph.atlas.u1,
                    glyph.atlas.v1
                );
            }
            text_x_ += glyph.advance;
        }

        int CharacterWidth(vgui2::HFont font, unsigned codepoint)
        {
            Font* slot = FindFont(font);
            if (slot == nullptr || !slot->has_set)
            {
                return 0;
            }
            return EnsureGlyph(*slot, codepoint).advance;
        }

        const Glyph& EnsureGlyph(Font& slot, unsigned codepoint)
        {
            const auto found = slot.glyphs.find(codepoint);
            if (found != slot.glyphs.end())
            {
                return found->second;
            }
            Glyph glyph;
            FaceUse* chosen = nullptr;
            for (FaceUse& use : slot.uses)
            {
                if (!use.Covers(codepoint))
                {
                    continue;
                }
                if (chosen == nullptr)
                {
                    chosen = &use;
                }
                if (use.face.HasChar(codepoint))
                {
                    chosen = &use;
                    break;
                }
            }
            if (chosen != nullptr)
            {
                GlyphInk ink;
                if (chosen->face.Load(codepoint, &ink))
                {
                    glyph.advance = ink.advance;
                    glyph.left = ink.left;
                    glyph.top = ink.top;
                    glyph.width = ink.width;
                    glyph.height = ink.height;
                    if (ink.alpha != nullptr && ink.width > 0 && ink.height > 0)
                    {
                        atlas_.Pack(ink.width, ink.height, ink.alpha, &glyph.atlas);
                    }
                }
            }
            return slot.glyphs.emplace(codepoint, glyph).first->second;
        }

        std::string ResolveFontPath(const char* windows_name, int weight) const
        {
            if (const char* shipped = FontFaces::ShippedFile(windows_name, weight))
            {
                const std::string directory = FontFaces::DirectoryBesideModule();
                if (!directory.empty())
                {
                    const std::string path = directory + "/fonts/" + shipped;
                    if (FontFileReadable(path))
                    {
                        return path;
                    }
                }
            }
            if (host_.resolve_font != nullptr && windows_name != nullptr)
            {
                char path[4096];
                path[0] = '\0';
                if (host_.resolve_font(windows_name, weight, path, static_cast<int>(sizeof path), host_.user) != 0 && path[0] != '\0' &&
                    FontFileReadable(path))
                {
                    return path;
                }
            }
            return {};
        }

        unsigned UploadPage(int page)
        {
            if (page < 0 || !HasGlContext())
            {
                return 0;
            }
            if (static_cast<int>(page_gl_.size()) < atlas_.pages())
            {
                page_gl_.resize(static_cast<size_t>(atlas_.pages()));
            }
            if (page >= static_cast<int>(page_gl_.size()))
            {
                return 0;
            }
            const unsigned char* pixels = atlas_.page_pixels(page);
            if (pixels == nullptr)
            {
                return 0;
            }
            GLint previous = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
            const bool created = page_gl_[static_cast<size_t>(page)] == 0;
            if (created)
            {
                GLuint name = 0;
                glGenTextures(1, &name);
                page_gl_[static_cast<size_t>(page)] = name;
            }
            const GLuint name = page_gl_[static_cast<size_t>(page)];
            glBindTexture(GL_TEXTURE_2D, name);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            if (created || atlas_.page_dirty(page))
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GlyphAtlas::kPageSize, GlyphAtlas::kPageSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                atlas_.clear_dirty(page);
            }
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous));
            return name;
        }

        Font* FindFont(vgui2::HFont font)
        {
            const auto index = static_cast<size_t>(font);
            if (font == 0 || index >= fonts_.size())
            {
                return nullptr;
            }
            return &fonts_[index];
        }

        const Font* FindFont(vgui2::HFont font) const
        {
            const auto index = static_cast<size_t>(font);
            if (font == 0 || index >= fonts_.size())
            {
                return nullptr;
            }
            return &fonts_[index];
        }

        void EnsureTexture(int id)
        {
            if (id <= 0 || id >= kTextureCap)
            {
                return;
            }
            if (static_cast<int>(textures_.size()) <= id)
            {
                textures_.resize(static_cast<size_t>(id) + 1);
            }
        }

        void StoreTexture(int id, const unsigned char* data, int wide, int tall, bool filter, bool bgra)
        {
            if (data == nullptr || wide <= 0 || tall <= 0)
            {
                return;
            }
            EnsureTexture(id);
            if (id <= 0 || id >= static_cast<int>(textures_.size()))
            {
                return;
            }
            Texture& texture = textures_[static_cast<size_t>(id)];
            texture.valid = true;
            texture.filter = filter;
            texture.wide = wide;
            texture.tall = tall;
            texture.rgba.resize(static_cast<size_t>(wide) * static_cast<size_t>(tall) * 4);
            if (!bgra)
            {
                std::memcpy(texture.rgba.data(), data, texture.rgba.size());
            }
            else
            {
                for (size_t i = 0; i < texture.rgba.size(); i += 4)
                {
                    texture.rgba[i] = data[i + 2];
                    texture.rgba[i + 1] = data[i + 1];
                    texture.rgba[i + 2] = data[i];
                    texture.rgba[i + 3] = data[i + 3];
                }
            }
            UploadTexture(texture);
        }

        void UploadTexture(Texture& texture)
        {
            if (!HasGlContext())
            {
                return;
            }
            int previous = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
            if (texture.gl_name == 0)
            {
                GLuint name = 0;
                glGenTextures(1, &name);
                texture.gl_name = name;
            }
            glBindTexture(GL_TEXTURE_2D, texture.gl_name);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            const int filter = texture.filter ? GL_LINEAR : GL_NEAREST;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture.wide, texture.tall, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture.rgba.data());
            glBindTexture(GL_TEXTURE_2D, static_cast<unsigned>(previous));
        }

        const Texture* TextureSlot(int id) const
        {
            if (id <= 0 || id >= static_cast<int>(textures_.size()) || !textures_[static_cast<size_t>(id)].valid)
            {
                return nullptr;
            }
            return &textures_[static_cast<size_t>(id)];
        }

        Texture* FindTexture(int id)
        {
            if (id <= 0 || id >= static_cast<int>(textures_.size()) || !textures_[static_cast<size_t>(id)].valid)
            {
                return nullptr;
            }
            Texture& texture = textures_[static_cast<size_t>(id)];
            if (texture.gl_name == 0)
            {
                UploadTexture(texture);
            }
            if (texture.gl_name == 0)
            {
                return nullptr;
            }
            return &texture;
        }

        void ReleaseTexture(Texture& texture)
        {
            if (texture.gl_name != 0 && HasGlContext())
            {
                const GLuint name = texture.gl_name;
                glDeleteTextures(1, &name);
            }
            texture = {};
        }

        void ReleaseGlTextures()
        {
            for (Texture& texture : textures_)
            {
                if (texture.gl_name != 0 && HasGlContext())
                {
                    const GLuint name = texture.gl_name;
                    glDeleteTextures(1, &name);
                    texture.gl_name = 0;
                }
            }
            if (HasGlContext())
            {
                for (unsigned& name : page_gl_)
                {
                    if (name != 0)
                    {
                        const GLuint gl_name = name;
                        glDeleteTextures(1, &gl_name);
                        name = 0;
                    }
                }
            }
        }

        static constexpr int kWalkDepthCap = 64;
        static constexpr int kChildCap = 4096;

        bool IsVisible(vgui2::VPANEL panel) const
        {
            if (panel == 0 || panel_.is_visible == nullptr)
            {
                return panel != 0;
            }
            return panel_.is_visible(PanelKey(panel), panel_.user) != 0;
        }

        bool IsPopup(vgui2::VPANEL panel) const
        {
            if (panel == 0 || panel_.is_popup == nullptr)
            {
                return false;
            }
            return panel_.is_popup(PanelKey(panel), panel_.user) != 0;
        }

        void PaintCursor()
        {
            cursor_quad_ = {};
            if (cursor_locked_ || cursor_ <= vgui2::dc_none || cursor_ >= vgui2::dc_blank)
            {
                return;
            }
            if (cursor_ >= static_cast<int>(cursor_texture_.size()))
            {
                return;
            }
            const int texture_id = cursor_texture_[static_cast<size_t>(cursor_)];
            const Texture* texture = TextureSlot(texture_id);
            if (texture == nullptr)
            {
                return;
            }
            cursor_quad_.drawn = true;
            cursor_quad_.x = cursor_x_;
            cursor_quad_.y = cursor_y_;
            cursor_quad_.wide = texture->wide;
            cursor_quad_.tall = texture->tall;
            cursor_quad_.texture = texture_id;
            const int previous = current_texture_;
            current_texture_ = texture_id;
            DrawTexturedRect(cursor_x_, cursor_y_, cursor_x_ + texture->wide, cursor_y_ + texture->tall);
            current_texture_ = previous;
        }

        bool PaintNode(vgui2::VPANEL panel) const
        {
            if (panel == 0 || panel_.paint == nullptr || !IsVisible(panel))
            {
                return false;
            }
            panel_.paint(PanelKey(panel), 1, 1, panel_.user);
            return true;
        }

        void SolveNode(vgui2::VPANEL panel, bool force, int depth)
        {
            if (panel == 0 || depth > kWalkDepthCap || !IsVisible(panel))
            {
                return;
            }
            if (force && panel_.apply_scheme != nullptr)
            {
                panel_.apply_scheme(PanelKey(panel), panel_.user);
            }
            if (panel_.solve != nullptr)
            {
                panel_.solve(PanelKey(panel), panel_.user);
            }
            if (panel_.child_count == nullptr || panel_.child_at == nullptr)
            {
                return;
            }
            int count = panel_.child_count(PanelKey(panel), panel_.user);
            if (count < 0)
            {
                count = 0;
            }
            if (count > kChildCap)
            {
                count = kChildCap;
            }
            for (int index = 0; index < count; ++index)
            {
                void* child = panel_.child_at(PanelKey(panel), index, panel_.user);
                if (child == nullptr)
                {
                    continue;
                }
                SolveNode(static_cast<vgui2::VPANEL>(reinterpret_cast<uintptr_t>(child)), force, depth + 1);
            }
        }

        void DispatchInput()
        {
            if (input_.dispatch != nullptr)
            {
                input_.dispatch(input_.user);
            }
        }

        void ErasePopup(vgui2::VPANEL panel)
        {
            for (auto it = popups_.begin(); it != popups_.end(); ++it)
            {
                if (*it == panel)
                {
                    popups_.erase(it);
                    return;
                }
            }
        }

        static XashSurface* current_;

        csr_surface_host_t host_{};
        csr_surface_panel_fns_t panel_{};
        csr_surface_input_fns_t input_{};
        csr_surface_file_fns_t file_{};
        std::vector<SurfaceFrame> stack_;
        std::vector<Texture> textures_;
        // Faces close before FreeType shuts down: fonts_ is declared after faces_.
        FontFaces faces_;
        std::vector<Font> fonts_;
        GlyphAtlas atlas_;
        std::vector<unsigned> page_gl_;
        std::string language_;
        int screen_wide_{};
        int screen_tall_{};
        int draw_r_{255};
        int draw_g_{255};
        int draw_b_{255};
        int draw_a_{};
        int text_r_{255};
        int text_g_{255};
        int text_b_{255};
        int text_a_{};
        int text_x_{};
        int text_y_{};
        int cursor_x_{};
        int cursor_y_{};
        int button_{};
        int gl_error_{};
        int current_texture_{};
        vgui2::HFont text_font_{};
        vgui2::HCursor cursor_{};
        std::vector<int> cursor_texture_{};
        struct CursorQuad
        {
            bool drawn{};
            int x{};
            int y{};
            int wide{};
            int tall{};
            int texture{};
        } cursor_quad_{};
        vgui2::VPANEL embedded_{};
        int push_count_ = 0;
        std::vector<vgui2::VPANEL> popups_;
        vgui2::VPANEL modal_{};
        vgui2::VPANEL restricted_{};
        bool button_down_{};
        bool cursor_locked_{};
        bool translate_keys_{};
    };

    XashSurface* XashSurface::current_ = nullptr;

} // namespace xash

namespace
{

    xash::XashSurface* As(csr_surface_t* surface)
    {
        return reinterpret_cast<xash::XashSurface*>(surface);
    }

    const xash::XashSurface* As(const csr_surface_t* surface)
    {
        return reinterpret_cast<const xash::XashSurface*>(surface);
    }

} // namespace

extern "C" IBaseInterface* CreateInterface(const char* name, int* code);

extern "C" CSR_SURFACE_API void* csr_surface_create_interface(const char* name, int* code)
{
    return CreateInterface(name, code);
}

extern "C" IBaseInterface* CreateInterface(const char* name, int* code)
{
    if (name != nullptr && std::strcmp(name, VGUI_SURFACE_INTERFACE_VERSION_GS) == 0 && xash::XashSurface::current() != nullptr)
    {
        if (code != nullptr)
        {
            *code = IFACE_OK;
        }
        return xash::XashSurface::current();
    }
    if (code != nullptr)
    {
        *code = IFACE_FAILED;
    }
    return nullptr;
}

csr_surface_t* csr_surface_create(const csr_surface_host_t* host)
{
    auto* surface = new xash::XashSurface(host);
    xash::XashSurface::set_current(surface);
    return reinterpret_cast<csr_surface_t*>(surface);
}

void csr_surface_destroy(csr_surface_t* surface)
{
    delete As(surface);
}

void csr_surface_begin_frame(csr_surface_t* surface, int wide, int tall)
{
    if (surface != nullptr)
    {
        As(surface)->BeginFrame(wide, tall);
    }
}

void csr_surface_end_frame(csr_surface_t* surface)
{
    if (surface != nullptr)
    {
        As(surface)->EndFrame();
    }
}

int csr_surface_gl_error(const csr_surface_t* surface)
{
    if (surface == nullptr)
    {
        return 0;
    }
    return As(surface)->gl_error();
}

void csr_surface_input_move(csr_surface_t* surface, int x, int y)
{
    if (surface != nullptr)
    {
        As(surface)->MoveCursor(x, y);
    }
}

void csr_surface_input_button(csr_surface_t* surface, int button, int down)
{
    if (surface != nullptr)
    {
        As(surface)->SetButton(button, down);
    }
}

void csr_surface_set_panel(csr_surface_t* surface, const csr_surface_panel_fns_t* panel)
{
    if (surface != nullptr)
    {
        As(surface)->SetPanelFns(panel);
    }
}

void csr_surface_set_input(csr_surface_t* surface, const csr_surface_input_fns_t* input)
{
    if (surface != nullptr)
    {
        As(surface)->SetInputFns(input);
    }
}

void csr_surface_set_file(csr_surface_t* surface, const csr_surface_file_fns_t* file)
{
    if (surface != nullptr)
    {
        As(surface)->SetFileFns(file);
    }
}

void csr_surface_set_cursor_texture(csr_surface_t* surface, int cursor, int texture)
{
    if (surface != nullptr)
    {
        As(surface)->SetCursorTexture(cursor, texture);
    }
}

int csr_surface_push_count(const csr_surface_t* surface)
{
    if (surface == nullptr)
    {
        return 0;
    }
    return As(surface)->PushCount();
}

int csr_surface_cursor_quad(const csr_surface_t* surface, int* x, int* y, int* wide, int* tall, int* texture)
{
    if (surface == nullptr)
    {
        return 0;
    }
    return As(surface)->CursorQuad(x, y, wide, tall, texture);
}

void* csr_surface_vgui(csr_surface_t* surface)
{
    return As(surface);
}

namespace
{

    void ResetTexture(void* object)
    {
        static_cast<vgui2::ISurface*>(object)->DrawSetTexture(0);
    }

    void SetTextColor(void* object, int r, int g, int b)
    {
        // VGUI alpha 0 is opaque on this surface.
        static_cast<vgui2::ISurface*>(object)->DrawSetTextColor(r, g, b, 0);
    }

} // namespace

void csr_surface_engine_hooks(csr_surface_t* surface, csr_xash_engine_surface_t* out)
{
    if (out == nullptr)
    {
        return;
    }
    *out = {};
    if (surface == nullptr)
    {
        return;
    }
    out->object = csr_surface_vgui(surface);
    if (out->object == nullptr)
    {
        return;
    }
    // This surface has no separate draw-init entry. Reset and text color are the calls.
    out->reset_texture = ResetTexture;
    out->set_text_color = SetTextColor;
}

#include "font_face.h"

#include "xash_surface/xash_surface.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cctype>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#if defined(__linux__)
#include <dlfcn.h>
#endif

namespace xash
{
    namespace
    {

        std::string Lower(const char* text)
        {
            std::string out;
            if (text == nullptr)
            {
                return out;
            }
            for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text); *cursor != 0; ++cursor)
            {
                out.push_back(static_cast<char>(std::tolower(*cursor)));
            }
            return out;
        }

        unsigned GlyphIndex(FT_Face face, unsigned codepoint)
        {
            FT_UInt index = FT_Get_Char_Index(face, codepoint);
            // Marlett and other symbol faces live in the private-use cmap.
            if (index == 0 && codepoint < 0x100)
            {
                index = FT_Get_Char_Index(face, codepoint + 0xF000);
            }
            return index;
        }

    } // namespace

    struct Face::Data
    {
        FT_Face face{};
        int ascent{};
        std::vector<unsigned char> alpha;
    };

    Face::~Face()
    {
        if (data_ != nullptr && data_->face != nullptr)
        {
            FT_Done_Face(data_->face);
        }
        delete data_;
        data_ = nullptr;
    }

    Face::Face(Face&& other) noexcept :
        data_(other.data_)
    {
        other.data_ = nullptr;
    }

    Face& Face::operator=(Face&& other) noexcept
    {
        if (this != &other)
        {
            if (data_ != nullptr && data_->face != nullptr)
            {
                FT_Done_Face(data_->face);
            }
            delete data_;
            data_ = other.data_;
            other.data_ = nullptr;
        }
        return *this;
    }

    int Face::ascent() const
    {
        if (data_ == nullptr)
        {
            return 0;
        }
        return data_->ascent;
    }

    bool Face::HasChar(unsigned codepoint) const
    {
        if (data_ == nullptr || data_->face == nullptr)
        {
            return false;
        }
        return GlyphIndex(data_->face, codepoint) != 0;
    }

    bool Face::Load(unsigned codepoint, GlyphInk* out)
    {
        if (out == nullptr || data_ == nullptr || data_->face == nullptr)
        {
            return false;
        }
        const unsigned index = GlyphIndex(data_->face, codepoint);
        if (FT_Load_Glyph(data_->face, index, FT_LOAD_RENDER) != 0)
        {
            return false;
        }
        const FT_GlyphSlot slot = data_->face->glyph;
        int advance = static_cast<int>((slot->advance.x + 32) >> 6);
        if (advance < 0)
        {
            advance = 0;
        }
        const FT_Bitmap& bitmap = slot->bitmap;
        const int wide = static_cast<int>(bitmap.width);
        const int tall = static_cast<int>(bitmap.rows);
        data_->alpha.assign(static_cast<size_t>(wide) * static_cast<size_t>(tall), 0);
        if (wide > 0 && tall > 0 && bitmap.buffer != nullptr && bitmap.pixel_mode == FT_PIXEL_MODE_GRAY)
        {
            const int pitch = bitmap.pitch;
            const int row_bytes = pitch < 0 ? -pitch : pitch;
            for (int row = 0; row < tall; ++row)
            {
                const int source_row = pitch < 0 ? tall - 1 - row : row;
                const unsigned char* src = bitmap.buffer + static_cast<size_t>(source_row) * static_cast<size_t>(row_bytes);
                unsigned char* dest = data_->alpha.data() + static_cast<size_t>(row) * static_cast<size_t>(wide);
                const int copy = wide < row_bytes ? wide : row_bytes;
                for (int column = 0; column < copy; ++column)
                {
                    dest[column] = src[column];
                }
            }
        }
        out->advance = advance;
        out->left = slot->bitmap_left;
        out->top = slot->bitmap_top;
        out->width = wide;
        out->height = tall;
        out->alpha = data_->alpha.empty() ? nullptr : data_->alpha.data();
        return true;
    }

    FontFaces::FontFaces()
    {
        FT_Library library = nullptr;
        if (FT_Init_FreeType(&library) != 0)
        {
            return;
        }
        library_ = library;
    }

    FontFaces::~FontFaces()
    {
        if (library_ != nullptr)
        {
            FT_Done_FreeType(static_cast<FT_Library>(library_));
            library_ = nullptr;
        }
    }

    Face FontFaces::Open(const std::string& path, int pixel_tall)
    {
        Face face;
        if (library_ == nullptr || path.empty() || pixel_tall <= 0)
        {
            return face;
        }
        FT_Face ft_face = nullptr;
        if (FT_New_Face(static_cast<FT_Library>(library_), path.c_str(), 0, &ft_face) != 0)
        {
            return face;
        }
        if (FT_Set_Pixel_Sizes(ft_face, 0, static_cast<FT_UInt>(pixel_tall)) != 0)
        {
            FT_Done_Face(ft_face);
            return face;
        }
        if (FT_Select_Charmap(ft_face, FT_ENCODING_UNICODE) != 0)
        {
            FT_Select_Charmap(ft_face, FT_ENCODING_MS_SYMBOL);
        }
        auto* data = new Face::Data();
        data->face = ft_face;
        const int ascent = static_cast<int>(FT_MulFix(ft_face->ascender, ft_face->size->metrics.y_scale) >> 6);
        data->ascent = ascent > 0 ? ascent : 0;
        face.data_ = data;
        return face;
    }

    const char* FontFaces::ShippedFile(const char* windows_name, int weight)
    {
        const std::string name = Lower(windows_name);
        if (name.empty())
        {
            return nullptr;
        }
        const bool bold = weight >= 600 || name.find("bold") != std::string::npos;
        if (name.find("arial") != std::string::npos || name.find("liberation") != std::string::npos)
        {
            return bold ? "LiberationSans-Bold.ttf" : "LiberationSans-Regular.ttf";
        }
        if (name.find("verdana") != std::string::npos || name.find("tahoma") != std::string::npos ||
            name.find("dejavu") != std::string::npos)
        {
            return bold ? "DejaVuSans-Bold.ttf" : "DejaVuSans.ttf";
        }
        return nullptr;
    }

    std::string FontFaces::DirectoryBesideModule()
    {
#if defined(__linux__)
        Dl_info info{};
        if (dladdr(reinterpret_cast<void*>(csr_surface_create), &info) == 0 || info.dli_fname == nullptr)
        {
            return {};
        }
        std::string path = info.dli_fname;
        const auto slash = path.find_last_of('/');
        if (slash == std::string::npos)
        {
            return {};
        }
        path.resize(slash);
        return path;
#elif defined(_WIN32)
        HMODULE module = nullptr;
        if (!GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(csr_surface_create),
                &module
            ))
        {
            return {};
        }
        char path[MAX_PATH];
        const DWORD length = GetModuleFileNameA(module, path, MAX_PATH);
        if (length == 0 || length >= MAX_PATH)
        {
            return {};
        }
        std::string file(path, length);
        const auto slash = file.find_last_of("\\/");
        if (slash == std::string::npos)
        {
            return {};
        }
        file.resize(slash);
        return file;
#else
        return {};
#endif
    }

    bool FontFileReadable(const std::string& path)
    {
        if (path.empty())
        {
            return false;
        }
#if defined(_WIN32)
        const DWORD attributes = GetFileAttributesA(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
        return access(path.c_str(), R_OK) == 0;
#endif
    }

} // namespace xash

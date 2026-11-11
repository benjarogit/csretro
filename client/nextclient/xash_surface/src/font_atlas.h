#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace xash
{

    // Rows of rectangles inside one square page. The caller adds padding.
    class ShelfPage
    {
    public:
        explicit ShelfPage(int size) :
            size_(size)
        {}

        bool Place(int wide, int tall, int* x, int* y)
        {
            if (x == nullptr || y == nullptr || wide <= 0 || tall <= 0 || wide > size_ || tall > size_)
            {
                return false;
            }
            if (x_ + wide > size_)
            {
                x_ = 0;
                y_ += row_tall_;
                row_tall_ = 0;
            }
            if (y_ + tall > size_)
            {
                return false;
            }
            *x = x_;
            *y = y_;
            x_ += wide;
            row_tall_ = std::max(row_tall_, tall);
            return true;
        }

        int size() const
        {
            return size_;
        }

    private:
        int size_{};
        int x_{};
        int y_{};
        int row_tall_{};
    };

    struct AtlasGlyph
    {
        int page{-1};
        float u0{};
        float v0{};
        float u1{};
        float v1{};
        bool placed{};
    };

    // One shared atlas for every face. A glyph is white RGB with coverage in alpha.
    class GlyphAtlas
    {
    public:
        static constexpr int kPageSize = 1024;
        static constexpr int kPageCap = 4;

        bool Pack(int wide, int tall, const unsigned char* alpha, AtlasGlyph* out)
        {
            if (out == nullptr || alpha == nullptr || wide <= 0 || tall <= 0)
            {
                return false;
            }
            const int pad_wide = wide + 2;
            const int pad_tall = tall + 2;
            if (pages_.empty())
            {
                pages_.emplace_back();
            }
            for (int attempt = 0; attempt < 2; ++attempt)
            {
                for (int page = 0; page < static_cast<int>(pages_.size()); ++page)
                {
                    int x = 0;
                    int y = 0;
                    if (!pages_[static_cast<size_t>(page)].shelf.Place(pad_wide, pad_tall, &x, &y))
                    {
                        continue;
                    }
                    Blit(pages_[static_cast<size_t>(page)], x + 1, y + 1, wide, tall, alpha);
                    pages_[static_cast<size_t>(page)].dirty = true;
                    out->placed = true;
                    out->page = page;
                    const float size = static_cast<float>(kPageSize);
                    out->u0 = static_cast<float>(x + 1) / size;
                    out->v0 = static_cast<float>(y + 1) / size;
                    out->u1 = static_cast<float>(x + 1 + wide) / size;
                    out->v1 = static_cast<float>(y + 1 + tall) / size;
                    return true;
                }
                if (static_cast<int>(pages_.size()) >= kPageCap)
                {
                    return false;
                }
                pages_.emplace_back();
            }
            return false;
        }

        int pages() const
        {
            return static_cast<int>(pages_.size());
        }

        const unsigned char* page_pixels(int page) const
        {
            if (page < 0 || page >= pages())
            {
                return nullptr;
            }
            return pages_[static_cast<size_t>(page)].rgba.data();
        }

        bool page_dirty(int page) const
        {
            if (page < 0 || page >= pages())
            {
                return false;
            }
            return pages_[static_cast<size_t>(page)].dirty;
        }

        void clear_dirty(int page)
        {
            if (page < 0 || page >= pages())
            {
                return;
            }
            pages_[static_cast<size_t>(page)].dirty = false;
        }

    private:
        struct Page
        {
            ShelfPage shelf{kPageSize};
            std::vector<unsigned char> rgba;
            bool dirty{false};

            Page()
                : rgba(static_cast<size_t>(kPageSize) * static_cast<size_t>(kPageSize) * 4)
            {
            }
        };

        static void Blit(Page& page, int origin_x, int origin_y, int wide, int tall, const unsigned char* alpha)
        {
            for (int row = 0; row < tall; ++row)
            {
                unsigned char* dest =
                    page.rgba.data() +
                    (static_cast<size_t>(origin_y + row) * static_cast<size_t>(kPageSize) + static_cast<size_t>(origin_x)) * 4;
                const unsigned char* src = alpha + static_cast<size_t>(row) * static_cast<size_t>(wide);
                for (int column = 0; column < wide; ++column)
                {
                    dest[column * 4 + 0] = 255;
                    dest[column * 4 + 1] = 255;
                    dest[column * 4 + 2] = 255;
                    dest[column * 4 + 3] = src[column];
                }
            }
        }

        std::vector<Page> pages_;
    };

} // namespace xash

#pragma once

#include <string>

namespace xash
{

    struct GlyphInk
    {
        int advance{};
        int left{};
        int top{};
        int width{};
        int height{};
        const unsigned char* alpha{};
    };

    // One FreeType face at a fixed pixel height. The alpha pointer is valid
    // until the next Load on the same face.
    class Face
    {
    public:
        Face() = default;
        ~Face();

        Face(const Face&) = delete;
        Face& operator=(const Face&) = delete;
        Face(Face&& other) noexcept;
        Face& operator=(Face&& other) noexcept;

        explicit operator bool() const
        {
            return data_ != nullptr;
        }

        int ascent() const;

        bool HasChar(unsigned codepoint) const;
        bool Load(unsigned codepoint, GlyphInk* out);

    private:
        friend class FontFaces;
        struct Data;
        Data* data_{};
    };

    class FontFaces
    {
    public:
        FontFaces();
        ~FontFaces();

        FontFaces(const FontFaces&) = delete;
        FontFaces& operator=(const FontFaces&) = delete;

        Face Open(const std::string& path, int pixel_tall);

        // Filename of a shipped substitute, or null when the host must resolve it.
        static const char* ShippedFile(const char* windows_name, int weight);
        static std::string DirectoryBesideModule();

    private:
        void* library_{};
    };

    bool FontFileReadable(const std::string& path);

} // namespace xash

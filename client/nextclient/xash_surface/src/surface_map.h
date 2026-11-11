#pragma once

#include <algorithm>

namespace xash
{

    struct SurfaceFrame
    {
        int origin_x{};
        int origin_y{};
        bool clip{};
        int clip_x0{};
        int clip_y0{};
        int clip_x1{};
        int clip_y1{};
    };

    struct MappedQuad
    {
        float x0{};
        float y0{};
        float x1{};
        float y1{};
        float u0{};
        float v0{};
        float u1{};
        float v1{};
    };

    // VGUI_Surface026 treats alpha 0 as opaque. GL treats 0 as transparent.
    inline int VguiAlphaToGl(int vgui_alpha)
    {
        int clamped = vgui_alpha;
        if (clamped < 0)
        {
            clamped = 0;
        }
        if (clamped > 255)
        {
            clamped = 255;
        }
        return 255 - clamped;
    }

    // Parent-relative rect plus the push origin, clipped to absolute pixels.
    // UVs shrink with the clip so a partial quad samples the visible texels.
    inline bool MapQuad(const SurfaceFrame& frame, int x0, int y0, int x1, int y1, MappedQuad* out)
    {
        float u0 = 0.0f;
        float v0 = 0.0f;
        float u1 = 1.0f;
        float v1 = 1.0f;
        if (x0 > x1)
        {
            std::swap(x0, x1);
            std::swap(u0, u1);
        }
        if (y0 > y1)
        {
            std::swap(y0, y1);
            std::swap(v0, v1);
        }

        const int ax0 = x0 + frame.origin_x;
        const int ay0 = y0 + frame.origin_y;
        const int ax1 = x1 + frame.origin_x;
        const int ay1 = y1 + frame.origin_y;
        int vx0 = ax0;
        int vy0 = ay0;
        int vx1 = ax1;
        int vy1 = ay1;
        if (frame.clip)
        {
            vx0 = std::max(vx0, frame.clip_x0);
            vy0 = std::max(vy0, frame.clip_y0);
            vx1 = std::min(vx1, frame.clip_x1);
            vy1 = std::min(vy1, frame.clip_y1);
        }
        const float width = static_cast<float>(ax1 - ax0);
        const float height = static_cast<float>(ay1 - ay0);
        if (vx0 >= vx1 || vy0 >= vy1 || width <= 0.0f || height <= 0.0f)
        {
            return false;
        }

        out->x0 = static_cast<float>(vx0);
        out->y0 = static_cast<float>(vy0);
        out->x1 = static_cast<float>(vx1);
        out->y1 = static_cast<float>(vy1);
        out->u0 = u0 + (u1 - u0) * (static_cast<float>(vx0 - ax0) / width);
        out->v0 = v0 + (v1 - v0) * (static_cast<float>(vy0 - ay0) / height);
        out->u1 = u0 + (u1 - u0) * (static_cast<float>(vx1 - ax0) / width);
        out->v1 = v0 + (v1 - v0) * (static_cast<float>(vy1 - ay0) / height);
        return true;
    }

} // namespace xash

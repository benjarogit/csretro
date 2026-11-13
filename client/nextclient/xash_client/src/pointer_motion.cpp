#include "pointer_motion.h"
#include <algorithm>
#include <cstdint>

namespace csr::client
{
    void PointerMotion::Update(int wide, int tall, const PointerHooks& hooks)
    {
        if (wide <= 0 || tall <= 0) return;
        int absolute_x{}, absolute_y{};
        hooks.absolute(hooks.owner, &absolute_x, &absolute_y);
        const bool relative = hooks.relative_mode(hooks.owner) != 0;
        if (!relative_ || !relative)
        {
            x_ = std::clamp(absolute_x, 0, wide - 1);
            y_ = std::clamp(absolute_y, 0, tall - 1);
        }
        x_ = std::clamp(x_, 0, wide - 1);
        y_ = std::clamp(y_, 0, tall - 1);
        hooks.move(hooks.owner, x_, y_);
        if (hooks.frame) hooks.frame(hooks.owner);
        if (!relative || !hooks.visible(hooks.owner))
        {
            relative_ = false;
            return;
        }
        relative_ = true;
        int dx{}, dy{};
        hooks.relative_delta(hooks.owner, &dx, &dy);
        x_ = static_cast<int>(std::clamp<int64_t>(int64_t(x_) + dx, 0, wide - 1));
        y_ = static_cast<int>(std::clamp<int64_t>(int64_t(y_) + dy, 0, tall - 1));
        if (dx || dy) hooks.move(hooks.owner, x_, y_);
    }
}

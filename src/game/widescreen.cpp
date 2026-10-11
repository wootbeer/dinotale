// True widescreen ("Expand" aspect ratio): the game's own view is made wider
// so it streams and draws more of each level, instead of the picture being
// stretched. See STATUS.md rounds 10, 13, 18 and 19.
//
// The wider view is set up when a level starts (its background layers and
// camera are created with it), so it's fixed for the whole level: the width
// is worked out once, when the first background layer is created, and kept
// until the last one is freed. While a level that started without Expand is
// running, Expand can't be chosen in the menu.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

#include <SDL.h>

#include "ultramodern/config.hpp"
#include "recompui/config.h"

// src/main/main.cpp
extern SDL_Window* window;

namespace {
    // The game's view is 320 pixels wide for a 4:3, 240-line screen.
    constexpr int32_t original_view_width = 320;
    constexpr double original_aspect = 4.0 / 3.0;
    // Widest view for now: 16:9. Each background layer's ring buffer is
    // (view width + 16) x 256 bytes on the game's heap.
    constexpr double max_aspect = 16.0 / 9.0;

    // Background layers currently alive, and the view width the level that
    // created them uses. All touched on the game thread only.
    int live_bg_views = 0;
    int32_t level_view_width = original_view_width;

    bool expand_enabled() {
        return ultramodern::renderer::get_graphics_config().ar_option == ultramodern::renderer::AspectRatio::Expand;
    }

    double window_aspect() {
        int w = 0, h = 0;
        if (window != nullptr) {
            SDL_GetWindowSize(window, &w, &h);
        }
        if ((w <= 0) || (h <= 0)) {
            return max_aspect;
        }
        return double(w) / double(h);
    }

    // The view width for a level starting now.
    int32_t wanted_view_width() {
        if (!expand_enabled()) {
            return original_view_width;
        }

        const double aspect = std::clamp(window_aspect(), original_aspect, max_aspect);
        // Whole 16-pixel tile columns, so the view stays centred on the
        // original one with the same number of pixels on each side.
        const int32_t wide = int32_t(std::ceil(original_view_width * aspect / original_aspect / 16.0)) * 16;
        return std::max(wide, original_view_width);
    }

    // The view width in effect: the running level's, or what a level
    // starting now would get.
    int32_t current_view_width() {
        return (live_bg_views > 0) ? level_view_width : wanted_view_width();
    }

    void update_expand_available() {
        // Outside a level anything goes; inside one, Expand only if the level
        // was started with it (switching away and back is fine then).
        const bool available = (live_bg_views == 0) || (level_view_width != original_view_width);
        recompui::config::graphics::set_expand_available(available);
    }

    uint32_t float_bits(float value) {
        uint32_t bits;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
}

// The width of a level's view in pixels, for a background layer whose view
// is `width` pixels wide on the N64 (320). Called as each layer is created.
extern "C" int32_t dinotale_bg_view_width(int32_t width) {
    if (live_bg_views == 0) {
        level_view_width = wanted_view_width();
    }
    live_bg_views++;
    update_expand_available();

    if (width != original_view_width) {
        return width;
    }
    return level_view_width;
}

// Called as each background layer is freed (func_8004F38C).
extern "C" void dinotale_bg_view_freed() {
    if (live_bg_views > 0) {
        live_bg_views--;
    }
    update_expand_available();
}

// The camera's view width (func_8005A9BC sets it to 320.0 when the camera is
// created). Returns the float bits for the $at register.
extern "C" uint32_t dinotale_camera_width_bits() {
    return float_bits(float(current_view_width()));
}

// Half the view width, for the places that use 160.0: the camera's horizontal
// wall probe (func_8002A148) and the conversions from the camera's left edge
// to its centre (func_8004FEC8, func_80058CC0). Returns the float bits for
// the $at register.
extern "C" uint32_t dinotale_camera_half_width_bits() {
    return float_bits(float(current_view_width()) / 2.0f);
}

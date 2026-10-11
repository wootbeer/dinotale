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
#include <vector>

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

    // The view width in effect: the running level's. Outside a level
    // (menus, course select, the storybook) everything stays as on the N64.
    int32_t current_view_width() {
        return (live_bg_views > 0) ? level_view_width : original_view_width;
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
extern "C" int32_t dinotale_bg_view_width(uint8_t* rdram, int32_t width) {
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

// Called with each background view func_8004EEEC just created. Its ring
// buffer (+0x60, aligned, +0x5C bytes) comes from the heap uncleared; the
// game streams in every column a 320-wide view can show, but a wider view at
// a level's ends reaches columns the level has no art for, which would show
// whatever an earlier level left there. Clear it, so they're black.
extern "C" void dinotale_bg_view_created(uint8_t* rdram, uint32_t view) {
    if ((view & 0xFF000000) != 0x80000000) {
        return;
    }

    const uint32_t buffer = *reinterpret_cast<uint32_t*>(rdram + (view + 0x60 - 0x80000000));
    const uint32_t size = *reinterpret_cast<uint32_t*>(rdram + (view + 0x5C - 0x80000000));
    if (((buffer & 0xFF000000) == 0x80000000) && (size > 0) && ((buffer - 0x80000000) + size <= 0x800000)) {
        std::memset(rdram + (buffer - 0x80000000), 0, size);
    }
}

// Called as each background layer is freed (func_8004F38C, layer in $a0; its
// view is at +0x1B8).
extern "C" void dinotale_bg_view_freed(uint8_t* rdram, uint32_t layer) {
    (void)rdram;
    (void)layer;

    if (live_bg_views > 0) {
        live_bg_views--;
    }
    update_expand_available();
}

// Half the view width, for the places that use 160.0: the camera's
// horizontal wall probe (func_8002A148) and the conversions from the camera's
// left edge to its centre (func_8004FEC8, func_80058CC0). Returns the float
// bits for the $at register.
extern "C" uint32_t dinotale_camera_half_width_bits() {
    return float_bits(float(current_view_width()) / 2.0f);
}

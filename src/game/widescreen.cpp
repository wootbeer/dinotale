// True widescreen ("Expand" aspect ratio): the game's own view is made wider
// so it streams and draws more of each level, instead of the picture being
// stretched. See STATUS.md rounds 10 and 13.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include <SDL.h>

#include "ultramodern/config.hpp"

// src/main/main.cpp
extern SDL_Window* window;

namespace {
    // The game's view is 320 pixels wide for a 4:3, 240-line screen.
    constexpr int32_t original_view_width = 320;
    constexpr double original_aspect = 4.0 / 3.0;
    // Widest view for now: 16:9. Each background layer's ring buffer is
    // (view width + 16) x 256 bytes on the game's heap.
    constexpr double max_aspect = 16.0 / 9.0;

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
}

// The width of a level's view in pixels, for a background layer whose view
// is `width` pixels wide on the N64 (320). Called when a level loads.
extern "C" int32_t dinotale_bg_view_width(int32_t width) {
    if (!expand_enabled() || (width != original_view_width)) {
        return width;
    }

    const double aspect = std::clamp(window_aspect(), original_aspect, max_aspect);
    // Whole 16-pixel tile columns, so the view stays centred on the
    // original one with the same number of pixels on each side.
    const int32_t wide = int32_t(std::ceil(original_view_width * aspect / original_aspect / 16.0)) * 16;
    return std::max(wide, original_view_width);
}

// The camera stops scrolling where a probe at its centre +/- half the view
// width hits a camera-wall tile in the level data (func_8002A148 for
// horizontal movement, func_8002A630 for vertical). The game uses 160, half
// of 320; with a wider view the probe has to reach the wider edges, or the
// view runs past a level's ends. Returns the float bits for the $at register.
extern "C" uint32_t dinotale_camera_half_width_bits() {
    const float half = float(dinotale_bg_view_width(original_view_width)) / 2.0f;
    uint32_t bits;
    static_assert(sizeof(bits) == sizeof(half));
    std::memcpy(&bits, &half, sizeof(bits));
    return bits;
}

// The camera's view width (func_8005A9BC sets it to 320.0 when the camera is
// created). Returns the float bits for the $at register.
extern "C" uint32_t dinotale_camera_width_bits() {
    const float width = float(dinotale_bg_view_width(original_view_width));
    uint32_t bits;
    std::memcpy(&bits, &width, sizeof(bits));
    return bits;
}

// True widescreen ("Expand" aspect ratio): the game's own view is made wider
// so it streams and draws more of each level, instead of the picture being
// stretched. See STATUS.md rounds 10 and 13.

#include <algorithm>
#include <cmath>
#include <cstdint>

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

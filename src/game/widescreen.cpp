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
#include "recomp.h"

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

    // When a background layer was last created or drawn (SDL ticks). A level
    // draws its layers every frame, paused or not, so if a new layer is
    // created after none were for a few seconds, the last level has ended;
    // start counting afresh in case a way out of it didn't free all its
    // layers.
    uint64_t last_bg_ticks = 0;
    constexpr uint64_t level_ended_ticks = 3000;

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

    // The width the HUD and the camera's edge stop use: the level's, but only
    // while Expand is selected. Switching to another mode in a level that
    // started in Expand keeps the level's wider background (it's set up when
    // the level starts), but the HUD and the camera go back to the 4:3 screen
    // at once, so nothing ends up outside it.
    int32_t active_view_width() {
        return expand_enabled() ? current_view_width() : original_view_width;
    }

    // The width the HUD is laid out for: as above, but kept to the middle 4:3
    // part of the screen when HUD Placement is Original. (The view is at most
    // 16:9, so the 16:9 and Expand placements are the same.)
    int32_t hud_view_width() {
        if (ultramodern::renderer::get_graphics_config().hr_option == ultramodern::renderer::HUDRatioMode::Original) {
            return original_view_width;
        }
        return active_view_width();
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
    const uint64_t now = SDL_GetTicks64();
    if ((live_bg_views > 0) && (now - last_bg_ticks > level_ended_ticks)) {
        live_bg_views = 0;
    }
    last_bg_ticks = now;
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
    return float_bits(float(active_view_width()) / 2.0f);
}

// Our RT64 patch (rt64_gbi_s2dex.cpp): columns at the left of a background
// image to cover with black when it's drawn.
extern "C" void rt64_dinotale_set_bg_left_mask(uint32_t address, int32_t columns);

// Called just before a background layer is drawn (func_8005F5D0 and
// func_8005F690, before their func_8007DB34 call), with the layer's render
// target and view object.
//
// At a level's left end the wider view reaches left of the layer's art. The
// game doesn't stream art there, so the layer would show whatever its ring
// buffer last held in those columns, which looks like junk. Tell the
// renderer to cover that strip of this layer with black. Layers are drawn
// back to front and the far ones reach further left, so each covers only its
// own junk.
//
// View object +0x18: the view's left x in the layer (float, unwrapped).
// Render target +0x0: the ring buffer, the image the layer is drawn from.
extern "C" void dinotale_bg_cover_outside(uint8_t* rdram, uint32_t rt, uint32_t obj) {
    auto word = [&](uint32_t address) -> uint32_t& {
        return *reinterpret_cast<uint32_t*>(rdram + (address - 0x80000000));
    };
    auto valid = [](uint32_t address) {
        return (address & 0xFF800000) == 0x80000000;
    };

    last_bg_ticks = SDL_GetTicks64();

    if (!valid(rt) || !valid(obj)) {
        return;
    }

    const uint32_t buffer = word(rt + 0x0);
    if (!valid(buffer)) {
        return;
    }

    int32_t columns = 0;
    if (current_view_width() != original_view_width) {
        const float left = *reinterpret_cast<float*>(&word(obj + 0x18));
        if (left < 0.0f) {
            columns = std::min(int32_t(std::ceil(-left)), current_view_width());
        }
    }

    rt64_dinotale_set_bg_left_mask(buffer, columns);
}

// The fruit frame: 30 sprites around the edge of the screen that fill with
// the fruit Yoshi eats (ovl_128, func_80579F18(a0, frame, scale)). It reads
// each slot's position from the frame object, x at +0x48 and y at +0xC0
// (floats, 30 each, relative to the middle of the screen), and writes the
// sprites. In Expand, spread
// the slots across the wider screen just for that call: centres move away
// from the middle in proportion, so the frame keeps its shape around the
// edge of the window.
namespace {
    uint32_t fruit_frame = 0;
    float fruit_frame_x[30];
}

extern "C" void dinotale_fruit_frame_begin(uint8_t* rdram, uint32_t frame) {
    fruit_frame = 0;
    const int32_t width = hud_view_width();
    if ((width == original_view_width) || ((frame & 0xFF800000) != 0x80000000)) {
        return;
    }

    const float scale = float(width) / float(original_view_width);
    const float half_slot = 12.0f;
    for (int i = 0; i < 30; i++) {
        float &x = *reinterpret_cast<float*>(rdram + (frame + 0x48 + 4 * i - 0x80000000));
        fruit_frame_x[i] = x;
        // The slots' positions are relative to the middle of the screen.
        const float middle = 0.0f;
        const float centre = x + half_slot - middle;
        x = middle + centre * scale - half_slot;
    }
    fruit_frame = frame;
}

extern "C" void dinotale_fruit_frame_end(uint8_t* rdram) {
    if (fruit_frame == 0) {
        return;
    }

    for (int i = 0; i < 30; i++) {
        *reinterpret_cast<float*>(rdram + (fruit_frame + 0x48 + 4 * i - 0x80000000)) = fruit_frame_x[i];
    }
    fruit_frame = 0;
}

// The health flower in the top left corner: two actors (ids 0x4010 and
// 0x4011, the u16 at +0xC) that keep themselves 60 pixels right of the
// camera's left edge. func_8004733C(&x, &y, actor) turns an actor's
// position into a screen position (x = (actor +0x84 - camera left +
// actor +0xB8) * actor +0xB4). In Expand, move the flower out to the left
// edge of the wider screen by the extra margin.
extern "C" void dinotale_hud_actor_screen_pos(uint8_t* rdram, recomp_context* ctx) {
    const int32_t width = hud_view_width();
    if (width == original_view_width) {
        return;
    }

    const uint32_t actor = uint32_t(ctx->r6);
    const uint32_t x_address = *reinterpret_cast<uint32_t*>(rdram + (uint32_t(ctx->r29) + 0x18 - 0x80000000));
    if (((actor & 0xFF800000) != 0x80000000) || ((x_address & 0xFF800000) != 0x80000000)) {
        return;
    }

    const uint16_t id = *reinterpret_cast<uint16_t*>(rdram + ((actor + 0xC) ^ 2) - 0x80000000);
    if ((id != 0x4010) && (id != 0x4011)) {
        return;
    }

    const float scale = *reinterpret_cast<float*>(rdram + (actor + 0xB4 - 0x80000000));
    *reinterpret_cast<float*>(rdram + (x_address - 0x80000000)) -= float(width - original_view_width) / 2.0f * scale;
}

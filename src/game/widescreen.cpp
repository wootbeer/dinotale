// True widescreen ("Expand" aspect ratio): the game's own view is made wider
// so it streams and draws more of each level, instead of the picture being
// stretched. See STATUS.md (Expand rounds, from 10 on).
//
// The wider view is set up when a level starts (its background layers and
// camera are created with it), so it's fixed for the whole level: the width
// is worked out once, when the first background layer is created, and kept
// until the last one is freed. While a level that started without Expand is
// running, Expand can't be chosen in the menu.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

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

    // A layer's view width scaled from the 320-wide screen to the level's view
    // width, rounded up to whole 16-pixel tile columns.
    int32_t scale_to_level(int32_t width) {
        const int32_t scaled = (width * level_view_width + original_view_width * 16 - 1) / (original_view_width * 16);
        return std::max(scaled * 16, width);
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

    // Wider layers (the zoomed-out areas of some levels, e.g. 4-1's treehouse,
    // draw a 480-wide layer at 70%) grow in proportion, in whole tile columns.
    return scale_to_level(width);
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

// The number of 16-pixel tile columns in a background row request
// (func_8004FB98), for a level whose data gives `columns` (20 for the 320-wide
// screen). The request starts at the wider view's left column, so in Expand
// it needs the view's extra columns too, or rows streamed in as the camera
// moves up or down stop short of the right side and leave old tiles there.
extern "C" int32_t dinotale_bg_row_columns(int32_t columns) {
    // 20 for a 320-wide layer, 30 for a 480-wide one; widened as in
    // dinotale_bg_view_width.
    const int32_t width = current_view_width();
    return (columns * width + original_view_width - 1) / original_view_width;
}

// Keeping Yoshi on screen. func_800233B4 checks his screen x (actor +0x34,
// in the 320-wide view's pixels) less/plus half his width against 16 and
// 304, and past either calls func_80023310, which puts him back at the
// camera's centre -/+ 144 (divided by his scale, actor +0xB4): a wall 16 px
// inside the screen's edge. In Expand both the check and the spot he's put
// back at move out to 16 px inside the wider view's edge, so he walks up to
// the visible edge where the camera stops (camera walls, level ends).
//
// Except on the left at a level's start: the camera sits with the 320-wide
// view's left edge near the level's (world x 0), and the wider view reaches
// past the level, where there's no ground. There the left limit stays where
// it is on the N64, world x 16 (but never further in than 16 px).
namespace {
    // Extra screen pixels each side of the 320-wide view.
    float extra_screen_width() {
        return float(active_view_width() - original_view_width) / 2.0f;
    }

    // The left limit, in the 320-wide view's screen pixels (16 on the N64).
    float left_screen_limit(uint8_t* rdram, uint32_t actor) {
        const float extra = extra_screen_width();
        if ((extra <= 0.0f) || ((actor & 0xFF800000) != 0x80000000)) {
            return 16.0f;
        }
        const uint32_t camera = *reinterpret_cast<uint32_t*>(rdram + (0x800F8318 - 0x80000000));
        if ((camera & 0xFF800000) != 0x80000000) {
            return 16.0f - extra;
        }
        const float centre = *reinterpret_cast<float*>(rdram + (camera + 0x30 - 0x80000000));
        const float scale = *reinterpret_cast<float*>(rdram + (actor + 0xB4 - 0x80000000));
        if (!std::isfinite(centre) || !std::isfinite(scale) || (scale <= 0.0f)) {
            return 16.0f - extra;
        }
        const float level_start = (16.0f - centre) * scale + float(original_view_width) / 2.0f;
        return std::min(16.0f, std::max(16.0f - extra, level_start));
    }
}

// func_800233B4's left check, `(x - half width) < 16`, with that value in
// $t7 / $t1: shift it so the check tests the left limit instead.
extern "C" int32_t dinotale_screen_left_test(uint8_t* rdram, uint32_t actor, int32_t value) {
    return value + int32_t(16.0f - std::floor(left_screen_limit(rdram, actor)));
}

// func_800233B4's right check, `(x + half width) < 304`, value in $t3 / $t0.
extern "C" int32_t dinotale_screen_right_test(int32_t value) {
    return value - int32_t(extra_screen_width());
}

// The distance from the camera's centre func_80023310 puts Yoshi back at
// (before dividing by his scale), as float bits for $at, negative on the left.
extern "C" uint32_t dinotale_screen_margin_bits(uint8_t* rdram, uint32_t actor, int32_t side) {
    if (side < 0) {
        const float limit = std::floor(left_screen_limit(rdram, actor));
        return float_bits(limit - float(original_view_width) / 2.0f);
    }
    return float_bits(float(original_view_width) / 2.0f - 16.0f + extra_screen_width());
}

// Many actors test their screen x (actor +0x34, in the 320-wide view's pixels)
// against bounds at or past the screen's edges to decide whether they're on
// screen: whether to draw extra parts, start a timer, despawn. In Expand the
// visible screen is wider, so give such a test the x it would see if the
// screen were: inside the 320-wide view unchanged, in the extra strips the
// nearest on-screen column (0 or 319), past them moved in by the strips'
// width. A test against bounds outside 0..319 then shifts with the wider
// view's edges. Only for loads used just for such a test.
extern "C" int32_t dinotale_actor_screen_x(int32_t x) {
    const int32_t extra = int32_t(extra_screen_width());
    if (extra <= 0) {
        return x;
    }
    if (x < 0) {
        return std::min(0, x + extra);
    }
    if (x >= original_view_width) {
        return std::max(original_view_width - 1, x - extra);
    }
    return x;
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
// sprites. In Expand, spread the slots across the wider screen just for that
// call: centres move away from the middle in proportion, so the frame keeps
// its shape around the edge of the window.
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
        x = (x + half_slot) * scale - half_slot;
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
    // This runs for every actor drawn, so check for the flower first.
    const uint32_t actor = uint32_t(ctx->r6);
    const uint32_t x_address = *reinterpret_cast<uint32_t*>(rdram + (uint32_t(ctx->r29) + 0x18 - 0x80000000));
    if (((actor & 0xFF800000) != 0x80000000) || ((x_address & 0xFF800000) != 0x80000000)) {
        return;
    }

    const uint16_t id = *reinterpret_cast<uint16_t*>(rdram + ((actor + 0xC) ^ 2) - 0x80000000);
    if ((id != 0x4010) && (id != 0x4011)) {
        return;
    }

    const int32_t width = hud_view_width();
    if (width == original_view_width) {
        return;
    }

    const float scale = *reinterpret_cast<float*>(rdram + (actor + 0xB4 - 0x80000000));
    *reinterpret_cast<float*>(rdram + (x_address - 0x80000000)) -= float(width - original_view_width) / 2.0f * scale;
}

#ifndef DT_RECOMP_HOOKS_H
#define DT_RECOMP_HOOKS_H

// Force-included into every file in RecompiledFuncs/ (see CMakeLists.txt):
// declarations the [[patches.hook]] text in yoshisstory.us.toml uses beyond
// what N64Recomp's recomp.h provides.

#include "recomp.h"

// librecomp/src/overlays.cpp
void load_overlays(uint32_t rom, int32_t ram_addr, uint32_t size);
void unload_overlays(int32_t ram_addr, uint32_t size);

// librecomp/src/ultra_translation.cpp
void osAiSetNextBuffer_recomp(uint8_t* rdram, recomp_context* ctx);

// src/main/crash_handler.cpp
void dinotale_crash_note_rdram(uint8_t* rdram);

// lib/RecompFrontend/recompui/src/api/ui_api_events.cpp
void recomp_run_ui_callbacks(uint8_t* rdram, recomp_context* ctx);

// src/game/widescreen.cpp
int32_t dinotale_bg_view_width(uint8_t* rdram, int32_t width);
uint32_t dinotale_camera_half_width_bits(void);
void dinotale_bg_view_created(uint8_t* rdram, uint32_t view);
void dinotale_bg_cover_outside(uint8_t* rdram, uint32_t rt, uint32_t obj);
void dinotale_fruit_frame_begin(uint8_t* rdram, uint32_t frame);
void dinotale_fruit_frame_end(uint8_t* rdram);
void dinotale_hud_actor_screen_pos(uint8_t* rdram, recomp_context* ctx);
int32_t dinotale_bg_row_columns(int32_t columns);
int32_t dinotale_bg_fill_columns(uint8_t* rdram, uint32_t view, int32_t columns);
uint32_t dinotale_screen_margin_bits(uint8_t* rdram, uint32_t actor, int32_t side);
int32_t dinotale_screen_left_test(uint8_t* rdram, uint32_t actor, int32_t value);
int32_t dinotale_screen_right_test(int32_t value);
int32_t dinotale_actor_screen_x(int32_t x);
void dinotale_bg_view_freed(uint8_t* rdram, uint32_t layer);

#endif // DT_RECOMP_HOOKS_H

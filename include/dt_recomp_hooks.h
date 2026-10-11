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
void dinotale_bg_view_freed(uint8_t* rdram, uint32_t layer);

#endif // DT_RECOMP_HOOKS_H

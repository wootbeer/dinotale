#ifndef DT_RECOMP_HOOKS_H
#define DT_RECOMP_HOOKS_H

// Force-included into every file in RecompiledFuncs/ (see CMakeLists.txt):
// declarations the [[patches.hook]] text in yoshisstory.us.toml uses beyond
// what N64Recomp's recomp.h provides.

#include "recomp.h"

// librecomp/src/overlays.cpp
void load_overlays(uint32_t rom, int32_t ram_addr, uint32_t size);
void unload_overlays(int32_t ram_addr, uint32_t size);

// lib/RecompFrontend/recompui/src/api/ui_api_events.cpp
void recomp_run_ui_callbacks(uint8_t* rdram, recomp_context* ctx);

#endif // DT_RECOMP_HOOKS_H

// libultra functions the game uses that N64Recomp leaves to the runtime
// (its ignored_funcs list renames them to *_recomp) but that the stock
// N64ModernRuntime doesn't provide. Found as the link errors of the first
// full build (STATUS.md round 2).

#include <cstdint>

#include "ultramodern/ultra64.h"
#include "ultramodern/ultramodern.hpp"
#include "recomp.h"

// u64 __d_to_ull(double): the compiler helper for double -> unsigned long
// long casts. Argument in $f12, result in $v0:$v1 (high:low), like the
// runtime's own __f_to_ll (librecomp/src/math_routines.cpp).
extern "C" void __d_to_ull_recomp(uint8_t*, recomp_context* ctx) {
    uint64_t ret = (uint64_t)ctx->f12.d;
    ctx->r2 = (int32_t)(ret >> 32);
    ctx->r3 = (int32_t)(ret >> 0);
}

// void osDpGetCounters(u32* counters): reads the RDP's clock/busy/pipe/tmem
// counters into counters[0..3]. There's no RDP here; report zeros (it's
// only used for performance meters).
extern "C" void osDpGetCounters_recomp(uint8_t* rdram, recomp_context* ctx) {
    gpr counters = ctx->r4;
    for (int i = 0; i < 4; i++) {
        MEM_W(i * 4, counters) = 0;
    }
}

// Controller Pak internals, reached only through the game's own Controller
// Pak file system code (the runtime implements the public osPfs* calls
// itself, and rumble through osMotor*). The game saves to EEPROM, and no
// Controller Pak is ever reported, so these answer like an empty slot.
// PFS_ERR_NOPACK = 1 (libultra os_cont.h).
extern "C" void __osContRamRead_recomp(uint8_t*, recomp_context* ctx) {
    ctx->r2 = 1;
}

extern "C" void __osContRamWrite_recomp(uint8_t*, recomp_context* ctx) {
    ctx->r2 = 1;
}

extern "C" void __osPfsSelectBank_recomp(uint8_t*, recomp_context* ctx) {
    ctx->r2 = 1;
}

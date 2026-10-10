#ifndef DT_PATCHES_PATCH_HELPERS_H
#define DT_PATCHES_PATCH_HELPERS_H

// DECLARE_FUNC gives one declaration that's valid on both sides of the
// patches build:
//  - Compiled as real MIPS (this directory's own Makefile, -DMIPS): a
//    plain C prototype matching the function's real signature. A call
//    site becomes an ordinary `jal`, which N64Recomp recompiles back into
//    a genuine function call (threading rdram/ctx automatically) when it
//    processes patches.elf.
//  - Compiled as host C/C++ (anything that includes this header without
//    -DMIPS, e.g. recompui's own sources): the real recompiled-function
//    ABI, (uint8_t* rdram, recomp_context* ctx).
#ifdef MIPS
#include "ultra64.h"
#else
#include "recomp.h"
#endif

#ifdef __cplusplus
#   define EXTERNC extern "C"
#else
#   define EXTERNC
#endif

#ifdef MIPS
#    define DECLARE_FUNC(type, name, ...) \
        EXTERNC type name(__VA_ARGS__)
#else // MIPS
#    define DECLARE_FUNC(type, name, ...) \
        EXTERNC void name(uint8_t* rdram, recomp_context* ctx)
#endif

#endif // DT_PATCHES_PATCH_HELPERS_H

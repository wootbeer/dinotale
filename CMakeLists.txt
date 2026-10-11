cmake_minimum_required(VERSION 3.20)

# Windows: build with the 64-bit-hosted clang-cl.exe (VC\Tools\Llvm\x64\bin),
# passed by full path; see BUILDING.md.

if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum OS X deployment version")
endif()

project(DinoTale LANGUAGES C CXX)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-everything /W4")
endif()

# Keep the build machine's folders (e.g. the Windows user name) out of the
# binaries: source paths that end up in them, through __FILE__ in asserts and
# log messages or in debug info, are made relative to the repo root. Both
# slash styles are mapped, since Windows tools pass either.
file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}" DT_SOURCE_DIR_NATIVE)
foreach (prefix IN ITEMS "${CMAKE_SOURCE_DIR}" "${DT_SOURCE_DIR_NATIVE}")
    if (CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
        add_compile_options("/clang:-ffile-prefix-map=${prefix}=.")
    elseif (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        add_compile_options("-ffile-prefix-map=${prefix}=.")
    endif()
endforeach()

# Opt out of constexpr mutex constructor on windows to prevent vcredist issues
if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR")
endif()

if (APPLE)
    enable_language(OBJC OBJCXX)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    option(RECOMP_FLATPAK "Configure the build for Flatpak compatibility." OFF)
endif()



# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if (WIN32)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/lib/")
endif()

set(RT64_STATIC TRUE)
set(RT64_SDL_WINDOW_VULKAN TRUE)
add_compile_definitions(
    HLSL_CPU
    __PRFCHWINTRIN_H
)

if (RECOMP_FLATPAK)
    add_compile_definitions(RECOMP_FLATPAK)
endif()

# Local fixes to RT64 and N64ModernRuntime (lib-patches/), applied to the
# submodules at configure time.
include(${CMAKE_SOURCE_DIR}/cmake/ApplyLibPatches.cmake)
dinotale_apply_lib_patches(rt64)
dinotale_apply_lib_patches(N64ModernRuntime)
dinotale_apply_lib_patches(RecompFrontend)

add_subdirectory(${CMAKE_SOURCE_DIR}/lib/rt64 ${CMAKE_BINARY_DIR}/rt64)

set(BUILD_SHARED_LIBS OFF)

add_subdirectory(${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime)


target_include_directories(rt64 PRIVATE ${CMAKE_BINARY_DIR}/rt64/src)

# RecompiledFuncs - Library containing the primary recompiler output
add_library(RecompiledFuncs STATIC)

# clang-cl silently drops bare GNU-style "-include <file>" (two tokens) with just a warning,
# then misparses the orphaned path token as a second input source file, which is a hard error
# ("cannot specify /Fo... when compiling multiple source files") rather than a missing include.
# The MSVC-native spelling is a single "/FI<file>" token. -falign-functions has no MSVC/clang-cl
# equivalent and is dropped too, but only as a (harmless, alignment-only) warning, so it's left
# as-is rather than special-cased.
if(MSVC)
    set(DT_RECOMP_HOOKS_FORCE_INCLUDE "SHELL:/FI${CMAKE_SOURCE_DIR}/include/dt_recomp_hooks.h")
else()
    set(DT_RECOMP_HOOKS_FORCE_INCLUDE "SHELL:-include ${CMAKE_SOURCE_DIR}/include/dt_recomp_hooks.h")
endif()
target_compile_options(RecompiledFuncs PRIVATE
    # -Wno-unused-but-set-variable
    -fno-strict-aliasing
    -Wno-unused-variable
    -Wno-implicit-function-declaration
    -falign-functions=16
    # Declarations for the [[patches.hook]] text in yoshisstory.us.toml.
    ${DT_RECOMP_HOOKS_FORCE_INCLUDE}
)

target_include_directories(RecompiledFuncs PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/ultramodern/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/librecomp/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/include
)

# CONFIGURE_DEPENDS: regenerating can add a new funcs_N.c (more functions
# spill into another file), which a plain configure-time glob never sees --
# the build then links against a library missing that file's functions.
file(GLOB FUNC_C_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/RecompiledFuncs/*.c)
file(GLOB FUNC_CXX_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/RecompiledFuncs/*.cpp)

if (NOT FUNC_C_SOURCES AND NOT FUNC_CXX_SOURCES)
    # The recompiled game code is generated from the ROM, which this repository cannot ship, so
    # a fresh clone has none of it yet. Keep the target valid with an empty placeholder so the
    # renderer, runtime, frontend and patches still configure and build; only the game executable
    # needs the generated sources. Run N64Recomp yoshisstory.us.toml to produce them.
    message(STATUS "RecompiledFuncs/ is empty - building without the recompiled game code.")
    file(WRITE ${CMAKE_BINARY_DIR}/recompiled_funcs_placeholder.c "")
    set(FUNC_C_SOURCES ${CMAKE_BINARY_DIR}/recompiled_funcs_placeholder.c)
endif()

target_sources(RecompiledFuncs PRIVATE ${FUNC_C_SOURCES} ${FUNC_CXX_SOURCES})

# Main executable
add_executable(DinoTale)

# The LZMA SDK's 7z decoder (public domain), for reading the game from a .7z
# (src/main/rom_archive.cpp). Only the files its own 7z example builds.
set(LZMA_SDK_DIR ${CMAKE_SOURCE_DIR}/thirdparty/lzma-sdk/C)
add_library(lzma_sdk_7z STATIC
    ${LZMA_SDK_DIR}/7zAlloc.c ${LZMA_SDK_DIR}/7zArcIn.c ${LZMA_SDK_DIR}/7zBuf.c
    ${LZMA_SDK_DIR}/7zBuf2.c ${LZMA_SDK_DIR}/7zCrc.c ${LZMA_SDK_DIR}/7zCrcOpt.c
    ${LZMA_SDK_DIR}/7zDec.c ${LZMA_SDK_DIR}/7zStream.c ${LZMA_SDK_DIR}/Bcj2.c
    ${LZMA_SDK_DIR}/Bra.c ${LZMA_SDK_DIR}/Bra86.c ${LZMA_SDK_DIR}/BraIA64.c
    ${LZMA_SDK_DIR}/CpuArch.c ${LZMA_SDK_DIR}/Delta.c ${LZMA_SDK_DIR}/Lzma2Dec.c
    ${LZMA_SDK_DIR}/LzmaDec.c ${LZMA_SDK_DIR}/Ppmd7.c ${LZMA_SDK_DIR}/Ppmd7Dec.c
)
target_compile_definitions(lzma_sdk_7z PRIVATE Z7_PPMD_SUPPORT Z7_EXTRACT_ONLY)
target_include_directories(lzma_sdk_7z PUBLIC ${LZMA_SDK_DIR})
target_link_libraries(DinoTale PRIVATE lzma_sdk_7z)

# The frontend (src/main) and the game-specific fixes (src/game). The RSP
# audio microcode (rsp/) is generated from the ROM and added further down.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/main/*.cpp
    ${CMAKE_SOURCE_DIR}/src/game/*.cpp
)

# src/main/register_overlays.cpp #includes RecompiledFuncs/recomp_overlays.inl,
# which a fresh clone doesn't have until N64Recomp has been run.
if (EXISTS ${CMAKE_SOURCE_DIR}/RecompiledFuncs/recomp_overlays.inl)
    target_compile_definitions(DinoTale PRIVATE DT_HAS_RECOMPILED_FUNCS)
endif()

if (NOT SOURCES)
    message(STATUS "src/ is empty - building DinoTale as a placeholder only.")
    file(WRITE ${CMAKE_BINARY_DIR}/dinotale_placeholder.cpp
        "int main() { return 0; } // No game source found -- see BUILDING.md.\n")
    set(SOURCES ${CMAKE_BINARY_DIR}/dinotale_placeholder.cpp)
endif()

target_include_directories(DinoTale PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/thirdparty/sse2neon
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/hlslpp/include
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/dxc/inc
    ${CMAKE_SOURCE_DIR}/lib/rt64/src
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/rhi
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/render
    ${CMAKE_SOURCE_DIR}/lib/RecompFrontend/recompinput/include
    ${CMAKE_SOURCE_DIR}/lib/RecompFrontend/recompui/include
    ${CMAKE_SOURCE_DIR}/lib/RecompFrontend/recompui/src
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/nativefiledialog-extended/src/include
    ${CMAKE_SOURCE_DIR}/lib/SlotMap
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Generate icon_bytes.c from the app icon PNG (the window icon, set in
# create_window in src/main/main.cpp).
if (EXISTS ${CMAKE_SOURCE_DIR}/icons/app.png)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.c ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.h
        COMMAND file_to_c ${CMAKE_SOURCE_DIR}/icons/app.png icon_bytes ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.c ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.h
        DEPENDS ${CMAKE_SOURCE_DIR}/icons/app.png
    )
    target_sources(DinoTale PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.c)
    target_compile_definitions(DinoTale PRIVATE DT_HAS_APP_ICON)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    target_compile_options(DinoTale PRIVATE
        -march=nehalem
        -fno-strict-aliasing
        -fms-extensions
    )
else()
    target_compile_options(DinoTale PRIVATE
        -fno-strict-aliasing
        -fms-extensions
    )
endif()

if (MSVC)
    # Disable identical code folding, since this breaks mod function patching as multiple functions can get merged into one.
    target_link_options(DinoTale PRIVATE /OPT:NOICF)
elseif (APPLE)
    # Use a wrapper around ld64 that respects segprot's `max_prot` value in order
    # to make our executable memory writable (required for mod function patching)
    target_link_options(DinoTale PRIVATE
        "-fuse-ld=${CMAKE_SOURCE_DIR}/.github/macos/ld64"
    )
endif()

if (WIN32)
    include(FetchContent)

    if (DEFINED ENV{SDL2_VERSION})
        set(SDL2_VERSION $ENV{SDL2_VERSION})
    else()
        set(SDL2_VERSION "2.30.3")
    endif()

    # Fetch SDL2 on windows
    FetchContent_Declare(
        sdl2
        URL https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-devel-${SDL2_VERSION}-VC.zip
    )
    FetchContent_MakeAvailable(sdl2)
    target_include_directories(DinoTale PRIVATE
        ${sdl2_SOURCE_DIR}/include
    )
    target_link_directories(DinoTale PRIVATE
        ${sdl2_SOURCE_DIR}/lib/x64
    )

    # Copy SDL2 and dxc DLLs to output folder as post build step
    add_custom_command(TARGET DinoTale POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${sdl2_SOURCE_DIR}/lib/x64/SDL2.dll"
            "${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxil.dll"
            "${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxcompiler.dll"
            $<TARGET_FILE_DIR:DinoTale>)

    set_target_properties(
        DinoTale
        PROPERTIES
            LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE"
            LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
            LINK_FLAGS_RELWITHDEBINFO  "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
            LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
    )

    # The .exe icon: icons/app.res, a precompiled resource (icons/app.ico as
    # icon 1) handed straight to the linker, which accepts .res files. This
    # skips CMake's clang-cl + rc.exe resource rule, which fails here.
    # Regenerate with: llvm-rc /FO app.res <script containing: 1 ICON "app.ico">
    if (EXISTS ${CMAKE_SOURCE_DIR}/icons/app.res)
        target_link_libraries(DinoTale PRIVATE ${CMAKE_SOURCE_DIR}/icons/app.res)
    endif()
    target_link_libraries(DinoTale PRIVATE SDL2 Winmm.lib)
endif()

if (APPLE)
    find_package(SDL2 REQUIRED)
    target_include_directories(DinoTale PRIVATE ${SDL2_INCLUDE_DIRS})

    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)

    target_link_libraries(DinoTale PRIVATE ${CMAKE_DL_LIBS} Threads::Threads SDL2::SDL2)

    include(${CMAKE_SOURCE_DIR}/.github/macos/apple_bundle.cmake)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(SDL2 REQUIRED)

    add_compile_definitions("PLUME_SDL_VULKAN_ENABLED")
    add_compile_definitions("RT64_SDL_WINDOW_VULKAN")

    message(STATUS "SDL2_FOUND = ${SDL2_FOUND}")
    message(STATUS "SDL2_INCLUDE_DIRS = ${SDL2_INCLUDE_DIRS}")

    target_include_directories(DinoTale PRIVATE ${SDL2_INCLUDE_DIRS})

    find_package(Freetype REQUIRED)

    message(STATUS "FREETYPE_FOUND = ${FREETYPE_FOUND}")
    message(STATUS "FREETYPE_INCLUDE_DIRS = ${FREETYPE_INCLUDE_DIRS}")
    message(STATUS "FREETYPE_LIBRARIES = ${FREETYPE_LIBRARIES}")

    include_directories(${FREETYPE_LIBRARIES})
    target_link_libraries(DinoTale PRIVATE ${FREETYPE_LIBRARIES} SDL2::SDL2 X11)

    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)

    target_link_libraries(DinoTale PRIVATE "-latomic -static-libstdc++" ${CMAKE_DL_LIBS} Threads::Threads)
endif()

# TODO fix the rt64 CMake script so that this doesn't need to be duplicated here
# For DXC
set (DXC_COMMON_OPTS "-I${PROJECT_SOURCE_DIR}/src")
set (DXC_DXIL_OPTS "-Wno-ignored-attributes")
set (DXC_SPV_OPTS "-spirv" "-fspv-target-env=vulkan1.0" "-fvk-use-dx-layout")
set (DXC_PS_OPTS "${DXC_COMMON_OPTS}" "-E" "PSMain" "-T ps_6_0" "-D DYNAMIC_RENDER_PARAMS")
set (DXC_VS_OPTS "${DXC_COMMON_OPTS}" "-E" "VSMain" "-T vs_6_0" "-D DYNAMIC_RENDER_PARAMS" "-fvk-invert-y")
set (DXC_CS_OPTS "${DXC_COMMON_OPTS}" "-E" "CSMain" "-T cs_6_0")
set (DXC_GS_OPTS "${DXC_COMMON_OPTS}" "-E" "GSMain" "-T gs_6_0")
set (DXC_RT_OPTS "${DXC_COMMON_OPTS}" "-D" "RT_SHADER" "-T" "lib_6_3" "-fspv-target-env=vulkan1.1spirv1.4" "-fspv-extension=SPV_KHR_ray_tracing" "-fspv-extension=SPV_EXT_descriptor_indexing")

if (${WIN32})
    set (DXC "${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxc.exe")
    add_compile_definitions(NOMINMAX)
else()
    if (APPLE)
        # Apple's binary is universal, so it'll work on both x86_64 and arm64
        set (DXC "DYLD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/lib/arm64" "${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/arm64/dxc-macos")
        if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
            set(SPIRVCROSS "DYLD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/spirv-cross/lib/x64" "${PROJECT_SOURCE_DIR}/lib/rt64//src/contrib/spirv-cross/bin/x64/spirv-cross")
        else()
            set(SPIRVCROSS "DYLD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/spirv-cross/lib/arm64" "${PROJECT_SOURCE_DIR}/lib/rt64//src/contrib/spirv-cross/bin/x64/spirv-cross")
        endif()
    else()
        if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
            set (DXC "LD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/lib/x64" "${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxc-linux")
        else()
            set (DXC "LD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/lib/arm64" "${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/arm64/dxc-linux")
        endif()
    endif()
endif()

set(RECOMP_FRONTEND_N64MODERNRUNTIME_PATH ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime)
set(RECOMP_FRONTEND_RT64_PATH ${CMAKE_SOURCE_DIR}/lib/rt64)

add_subdirectory(${CMAKE_SOURCE_DIR}/lib/RecompFrontend)

# Upstream RmlUi 6.0 uses uint32_t and friends without including <cstdint>, which clang with a
# current libstdc++ rejects, and recompui includes those headers. Add the include from here
# rather than patching the submodule. clang-cl (MSVC frontend variant, incl. Windows) needs the
# native /FI spelling instead of GNU -include: the latter's quote-style header lookup fails to
# resolve bare STL header names (e.g. "cstdint") when clang-cl is generating a precompiled header,
# such as RmlUi's own cmake_pch.cxx, even though INCLUDE-based system search works everywhere else.
if(MSVC)
    set(DT_CSTDINT_FORCE_INCLUDE "/FI cstdint")
else()
    set(DT_CSTDINT_FORCE_INCLUDE "-include cstdint")
endif()
foreach(_btx_rml_target rmlui_core rmlui_debugger recompui)
    if(TARGET ${_btx_rml_target})
        target_compile_options(${_btx_rml_target} PRIVATE "SHELL:$<$<COMPILE_LANGUAGE:CXX>:${DT_CSTDINT_FORCE_INCLUDE}>")
    endif()
endforeach()

# These libraries reference each other's symbols in both directions (e.g.
# recompinput's player-assignment code calls into recompui's
# ConfigPageControls, RecompiledFuncs calls into librecomp/ultramodern's
# os*_recomp reimplementations) without CMake knowing about the cycle at
# the target level, so a single left-to-right archive scan can leave real,
# resolvable symbols undefined depending on which .o a given archive
# happens to pull in first. --start-group/--end-group makes the linker
# keep re-scanning this whole set until nothing new resolves, same fix as
# for any other circular static-library dependency. This has to be a single
# target_link_libraries call with the group flags as literal arguments --
# CMake collects link *options* (target_link_options) and link
# *libraries* into separate lists and concatenates them at fixed positions
# in the final command line regardless of call order, so two separate
# target_link_options calls do NOT end up wrapping the libraries below.
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT APPLE)
    set(DT_LINK_GROUP_START "-Wl,--start-group")
    set(DT_LINK_GROUP_END "-Wl,--end-group")
else()
    set(DT_LINK_GROUP_START "")
    set(DT_LINK_GROUP_END "")
endif()

target_link_libraries(DinoTale PRIVATE
    ${DT_LINK_GROUP_START}
    RecompiledFuncs
    recompui
    recompinput
    librecomp
    ultramodern
    rt64
    nfd
    ${DT_LINK_GROUP_END}
)


# Embed all .nrm files in the "mods" directory
file(GLOB NRM_FILES "${CMAKE_SOURCE_DIR}/mods/*.nrm")

set(GENERATED_NRM_SOURCES "")

foreach(NRM_FILE ${NRM_FILES})
    get_filename_component(NRM_NAME ${NRM_FILE} NAME_WE)
    set(OUT_C "${CMAKE_CURRENT_BINARY_DIR}/mods/${NRM_NAME}.c")
    set(OUT_H "${CMAKE_CURRENT_BINARY_DIR}/mods/${NRM_NAME}.h")

    add_custom_command(
        OUTPUT ${OUT_C} ${OUT_H}
        COMMAND file_to_c ${NRM_FILE} ${NRM_NAME} ${OUT_C} ${OUT_H}
        DEPENDS ${NRM_FILE}
    )

    list(APPEND GENERATED_NRM_SOURCES ${OUT_C})
endforeach()

target_sources(DinoTale PRIVATE ${GENERATED_NRM_SOURCES})

# RSP audio microcode: rsp/aspMain.cpp is generated from the ROM by RSPRecomp
# (aspMain.us.toml), so it's gitignored and only built when the ROM is present.
set(DT_ROM_PATH "${CMAKE_SOURCE_DIR}/yoshisstory.us.z64")
if (EXISTS "${DT_ROM_PATH}")
    add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/rsp/aspMain.cpp
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/rsp
        COMMAND $<TARGET_FILE:RSPRecomp> aspMain.us.toml
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS ${CMAKE_SOURCE_DIR}/aspMain.us.toml RSPRecomp
    )
    target_sources(DinoTale PRIVATE ${CMAKE_SOURCE_DIR}/rsp/aspMain.cpp)
    target_compile_definitions(DinoTale PRIVATE DT_HAS_RSP_AUDIO)
endif()

target_sources(DinoTale PRIVATE ${SOURCES})

set_property(TARGET DinoTale PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# A copy of assets/ next to the exe, so it runs from the build folder too
# (Visual Studio's F5, double-clicking the exe). See also main.cpp, which
# switches to the exe's folder when the working directory has no assets/.
add_custom_command(TARGET DinoTale POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:DinoTale>/assets)

# Lossless Scaling frame generation for a Vulkan renderer (see README.md).
#
# Before add_subdirectory(), tell it where the host's Vulkan pieces are:
#   LSFG_VULKAN_TARGET     a target whose include directories have volk.h and
#                          vulkan/vulkan_core.h, and that links volk (RT64: plume)
#   LSFG_SPIRV_INCLUDE_DIR a folder with spirv/unified1/spirv.hpp (SPIRV-Headers)
# Targets: lsfg (link this) and lsfg_dxbc (the shader translator, used by lsfg).

if (NOT LSFG_VULKAN_TARGET)
    message(FATAL_ERROR "lsfg: set LSFG_VULKAN_TARGET to a target providing volk and the Vulkan headers")
endif()
if (NOT LSFG_SPIRV_INCLUDE_DIR)
    message(FATAL_ERROR "lsfg: set LSFG_SPIRV_INCLUDE_DIR to SPIRV-Headers' include folder")
endif()

# Standard headers the upstream code doesn't include itself (see std_includes.h).
if (MSVC)
    set(LSFG_STD_INCLUDES "/FI${CMAKE_CURRENT_SOURCE_DIR}/std_includes.h")
else()
    set(LSFG_STD_INCLUDES -include "${CMAKE_CURRENT_SOURCE_DIR}/std_includes.h")
endif()

# DXVK's DXBC to SPIR-V compiler (v2.6.2, zlib; trimmed, see README.md).
file(GLOB LSFG_DXBC_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/dxbc/dxbc/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dxbc/spirv/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dxbc/util/util_string.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dxbc/util/log/log_debug.cpp")
add_library(lsfg_dxbc STATIC ${LSFG_DXBC_SOURCES})
set_target_properties(lsfg_dxbc PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_include_directories(lsfg_dxbc PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/dxbc"
    "${LSFG_SPIRV_INCLUDE_DIR}")
target_include_directories(lsfg_dxbc PRIVATE $<TARGET_PROPERTY:${LSFG_VULKAN_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(lsfg_dxbc PUBLIC VK_NO_PROTOTYPES)
if (MSVC)
    target_compile_options(lsfg_dxbc PRIVATE /EHsc /w)
else()
    target_compile_options(lsfg_dxbc PRIVATE -w)
endif()
target_compile_options(lsfg_dxbc PRIVATE ${LSFG_STD_INCLUDES})

# lsfg-vk's frame generation (v1.0.0, MIT; reworked to run on the host's
# device, see README.md) and this library's interface (include/lsfg/lsfg.hpp).
file(GLOB LSFG_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/framegen/src/*/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/framegen/v3.1_src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/framegen/v3.1_src/shaders/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/framegen/v3.1p_src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/framegen/v3.1p_src/shaders/*.cpp")
add_library(lsfg STATIC ${LSFG_SOURCES})
set_target_properties(lsfg PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(lsfg PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(lsfg PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/framegen/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/framegen/v3.1_include"
    "${CMAKE_CURRENT_SOURCE_DIR}/framegen/v3.1p_include")
target_link_libraries(lsfg PUBLIC ${LSFG_VULKAN_TARGET} PRIVATE lsfg_dxbc)
if (MSVC)
    target_compile_options(lsfg PRIVATE /EHsc)
endif()
target_compile_options(lsfg PRIVATE ${LSFG_STD_INCLUDES})

# The test tool (test/lsfg_test.cpp): frame generation on a device of its own.
option(LSFG_BUILD_TEST "Build lsfg_test" OFF)
if (LSFG_BUILD_TEST)
    add_executable(lsfg_test "${CMAKE_CURRENT_SOURCE_DIR}/test/lsfg_test.cpp")
    set_target_properties(lsfg_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    # LSFG_TEST_LIBS: anything else LSFG_VULKAN_TARGET needs to link (plume: SDL2).
    target_link_libraries(lsfg_test PRIVATE lsfg ${LSFG_VULKAN_TARGET} ${LSFG_TEST_LIBS} ${CMAKE_DL_LIBS})
endif()

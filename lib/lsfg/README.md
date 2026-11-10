# lsfg: Lossless Scaling frame generation for a Vulkan renderer

Frame generation from [Lossless Scaling](https://store.steampowered.com/app/993090/Lossless_Scaling/)
(LSFG 3.1, quality and performance modes), built into a renderer instead of
running as a separate app or a Vulkan layer. The player supplies their own
`Lossless.dll`; nothing from Lossless Scaling is in this folder, the repo or a
release.

Written for BattleTanx: Global Assault Recompiled, to carry over to other
RT64-based ports and other Vulkan renderers.

## Where it comes from

| Folder | What | From | License |
| --- | --- | --- | --- |
| `framegen/` | the frame generation passes | [lsfg-vk](https://github.com/PancakeTAS/lsfg-vk) v1.0.0 `framegen/` | MIT, `LICENSE-lsfg-vk.md` |
| `dxbc/` | DXBC to SPIR-V shader compiler | [DXVK](https://github.com/doitsujin/dxvk) v2.6.2 `src/dxbc`, `src/spirv`, part of `src/util` | zlib, `LICENSE-dxvk.txt` |
| `src/`, `include/`, `rt64/`, `test/` | this library's interface, shader loading, RT64 side, test | new | the repo's |

Changes from lsfg-vk's `framegen/`:
- It runs on the host's `VkDevice` (`core/device.hpp` wraps it) instead of
  creating its own instance and device.
- It records into the host's command buffer (`core/commandbuffer.hpp`)
  instead of submitting with semaphores and fences.
- It owns its input and output images instead of importing them from Linux
  file descriptors.
- Barriers are `vkCmdPipelineBarrier`, so `synchronization2` isn't needed.
- `lsfg.cpp`, the instance, command pool, fence, semaphore and DDS upload
  code are gone. The contexts are `record()`, `nextInput()` and `outputs()`.

Changes from DXVK: the DXBC compiler returns its SPIR-V instead of a DXVK
shader object. Its options are the defaults (as lsfg-vk used them). DXVK's
logger is a stub (`util/log/log.h`), and `dxvk/dxvk_shader.h` holds only the
two types the compiler uses. The Windows wide-string helpers are gone.

## How it works

1. `lsfg::load_shaders(dll, cache)` reads the frame generation shaders out of
   `Lossless.dll`, a few seconds the first time. They're RCDATA resources 255
   to 302, numbered as lsfg-vk's `extract.cpp`. It translates them to SPIR-V,
   renumbering bindings as lsfg-vk's `trans.cpp`, and caches them; later
   loads read the cache.
2. `lsfg::FrameGenerator::create(physicalDevice, device, shaders, settings,
   size)` builds the pipelines and images (RGBA8).
3. Per real frame, in a command buffer of the host's:
   - `record_input(cmd, image, size)` blits the frame in;
   - after the GPU has done that, `scene_cut()` says whether the frame cut
     to another scene (`scene_difference()`, from a thumbnail, 0.15 or
     more), or is the frame after one: the host should show the new frame
     alone then, since frames generated across a cut morph one picture into
     the other, and the motion estimate uses the last three frames;
   - `record_generate(cmd)` makes `generated_frames` frames between the
     previous real frame and this one;
   - `record_output(cmd, i, image, offset, size)` blits generated frame `i`
     out, for example to a swap chain image.
4. The host presents the generated frames, spaced over the real frame
   interval, then the real frame. That adds one real frame of delay: the
   generated frames need the next real frame.

Requirements for the host's device: `vulkanMemoryModel` and
`shaderStorageImageWriteWithoutFormat` enabled (`lsfg::check_device` says if
the GPU has them). Vulkan functions are called through volk's globals: the
host must have done `volkInitialize` and `volkLoadInstance`.

## Using it in an RT64 port

What this repo does, to copy:

1. Copy `lib/lsfg/` (this folder).
2. Apply the RT64 and plume patches:
   - `lib-patches/rt64/0022-frame-generation.patch`: the present queue calls
     `rt64/rt64_frame_generation.cpp` for each new game frame.
   - `lib-patches/plume/0005-vulkan-memory-model.patch`: plume's Vulkan device
     enables `vulkanMemoryModel` where supported.
3. In CMake, after `add_subdirectory(lib/rt64)`:
   ```cmake
   target_sources(rt64 PRIVATE ${CMAKE_SOURCE_DIR}/lib/lsfg/rt64/rt64_frame_generation.cpp)
   target_include_directories(rt64 PUBLIC ${CMAKE_SOURCE_DIR}/lib/lsfg/rt64)
   if (NOT APPLE)
       set(LSFG_VULKAN_TARGET plume)
       set(LSFG_SPIRV_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/re-spirv/external/SPIRV-Headers/include)
       add_subdirectory(${CMAKE_SOURCE_DIR}/lib/lsfg ${CMAKE_BINARY_DIR}/lsfg)
       target_link_libraries(rt64 lsfg)
   endif()
   ```
   With RecompFrontend, `recompui` also includes RT64's headers by folder:
   `target_include_directories(recompui PRIVATE ${CMAKE_SOURCE_DIR}/lib/lsfg/rt64)`.
4. Settings: `src/main/frame_generation_config.cpp`. It's the Graphics tab's
   Frame Generation, Lossless Scaling Multiplier and Mode, and the Load
   Lossless.dll button. It calls `RT64::setFrameGeneration()` and needs only
   recompui, RT64 and this library. Call its `add_frame_generation_options()`
   with the Graphics tab's config.

RT64 must run on Vulkan: the Graphics API option
(`lib-patches/RecompFrontend/0016-show-graphics-api.patch` shows it), which
the settings switch to Vulkan when Lossless Scaling is chosen. The RT64 side
reports why it isn't running, such as no DLL or D3D12, through
`RT64::getFrameGenerationStatus()` and stderr.

## Test

`test/lsfg_test.cpp` makes its own Vulkan device. It sends five frames
through with a square moving 40 pixels a frame, and checks each generated
frame has the square halfway:

```
cmake -DLSFG_BUILD_TEST=ON ...   (builds lsfg_test next to the game)
lsfg_test path/to/Lossless.dll [cache file] [performance]
```

On a software Vulkan driver (lavapipe), both modes put the square within half
a pixel of halfway.

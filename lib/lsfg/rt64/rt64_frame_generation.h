//
// Lossless Scaling frame generation in RT64's present queue (lib/lsfg).
//
// The host sets the settings (RT64::setFrameGeneration, any thread). With them
// on and RT64 running on Vulkan, the present queue draws each new game frame
// into an image of this class's, which makes frames between it and the last
// one; the queue presents those, spaced over the game frame, then the real
// one (rt64_present_queue.cpp, lib-patches/rt64/0022-frame-generation.patch).
//

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "common/rt64_plume.h"

namespace RT64 {
    struct FrameGenerationSettings {
        bool enabled = false;
        // Frames made between two real ones: 1 doubles the frame rate.
        uint32_t generatedFrames = 1;
        // Lossless Scaling's performance mode.
        bool performance = false;
        // Lossless.dll, and where to keep its translated shaders.
        std::filesystem::path dllPath;
        std::filesystem::path cachePath;
    };

    // Takes effect on the next game frame.
    void setFrameGeneration(const FrameGenerationSettings &settings);

    // Why frame generation isn't running although it's on (no DLL, not Vulkan,
    // unsupported GPU...); empty when it's off or running.
    std::string getFrameGenerationStatus();

    // Used by the present queue's thread only.
    class FrameGeneration {
    public:
        FrameGeneration();
        ~FrameGeneration();

        // Whether a new game frame is to go through frame generation; sets up
        // what's needed for the swap chain's size and format.
        bool prepare(RenderDevice *device, RenderSwapChain *swapChain, bool vulkan);

        // Where the game frame is to be drawn (COLOR_WRITE).
        RenderFramebuffer *sourceFramebuffer() const;
        RenderTexture *sourceTexture() const;

        // Records taking the game frame in (sourceTexture() in COPY_SOURCE)
        // and generating; the number of generated frames to show (0 for the
        // first frame).
        uint32_t record(RenderCommandList *commandList);

        // After the GPU has done record()'s work: whether the game frame cut to
        // another picture (a scene change rather than motion), or is the one
        // after such a cut. Frames generated across a cut would morph one
        // picture into the other, so the game frame is shown alone then.
        bool sceneCut();

        // Records copying generated frame `index` to dst (COPY_DEST, swap chain size).
        void recordOutput(RenderCommandList *commandList, uint32_t index, RenderTexture *dst);

        // Time between generated frames: the game frame interval measured so
        // far, divided by the frames shown per game frame. Call once per game frame.
        std::chrono::nanoseconds markGameFrame(uint32_t framesShown);

        // Drops the generator (the GPU must be idle).
        void release();

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
};

//
// Lossless Scaling frame generation in RT64's present queue. See the header.
//

#include "rt64_frame_generation.h"

#include <algorithm>
#include <cstdio>
#include <mutex>

#if !defined(__APPLE__)
#   include "plume_vulkan.h"
#   include "lsfg/lsfg.hpp"
#   define RT64_FRAME_GENERATION
#endif

namespace RT64 {
    namespace {
        std::mutex settingsMutex;
        FrameGenerationSettings pendingSettings;
        uint64_t settingsVersion = 0;
        std::string status;
    }

    void setFrameGeneration(const FrameGenerationSettings &settings) {
        std::scoped_lock lock(settingsMutex);
        pendingSettings = settings;
        settingsVersion++;
        if (!settings.enabled) {
            status.clear();
        }
    }

    std::string getFrameGenerationStatus() {
        std::scoped_lock lock(settingsMutex);
        return status;
    }

    static void setStatus(const std::string &newStatus) {
        std::scoped_lock lock(settingsMutex);
        if (status != newStatus) {
            status = newStatus;
            if (!status.empty()) {
                fprintf(stderr, "Frame generation: %s\n", status.c_str());
            }
        }
    }

    struct FrameGeneration::Impl {
        FrameGenerationSettings settings;
        uint64_t appliedVersion = UINT64_MAX;
        bool failed = false;
#   ifdef RT64_FRAME_GENERATION
        lsfg::ShaderSet shaders;
        std::filesystem::path shadersDll;
        std::unique_ptr<lsfg::FrameGenerator> generator;
#   endif
        RenderDevice *device = nullptr;
        std::unique_ptr<RenderTexture> source;
        std::unique_ptr<RenderFramebuffer> sourceFramebuffer;
        uint32_t width = 0;
        uint32_t height = 0;
        RenderFormat format = RenderFormat::UNKNOWN;

        // Game frame timing, for spacing the generated frames.
        std::chrono::steady_clock::time_point lastGameFrame;
        double intervalNs = 0.0;
    };

    FrameGeneration::FrameGeneration() : impl(std::make_unique<Impl>()) {}

    FrameGeneration::~FrameGeneration() = default;

    void FrameGeneration::release() {
#   ifdef RT64_FRAME_GENERATION
        impl->generator.reset();
#   endif
        impl->sourceFramebuffer.reset();
        impl->source.reset();
        impl->width = 0;
        impl->height = 0;
    }

    bool FrameGeneration::prepare(RenderDevice *device, RenderSwapChain *swapChain, bool vulkan) {
        {
            std::scoped_lock lock(settingsMutex);
            if (impl->appliedVersion != settingsVersion) {
                const bool changed = (pendingSettings.generatedFrames != impl->settings.generatedFrames) ||
                    (pendingSettings.performance != impl->settings.performance) || !pendingSettings.enabled;
                impl->settings = pendingSettings;
                impl->appliedVersion = settingsVersion;
                impl->failed = false;
                if (changed) {
#               ifdef RT64_FRAME_GENERATION
                    impl->generator.reset();
#               endif
                }
            }
        }

        if (!impl->settings.enabled || impl->failed) {
            return false;
        }

#   ifdef RT64_FRAME_GENERATION
        if (!vulkan) {
            impl->failed = true;
            setStatus("Lossless Scaling frame generation needs the Vulkan graphics API.");
            return false;
        }

        VulkanDevice *vulkanDevice = static_cast<VulkanDevice *>(device);
        const uint32_t width = swapChain->getWidth();
        const uint32_t height = swapChain->getHeight();
        const RenderFormat format = static_cast<VulkanTexture *>(swapChain->getTexture(0))->desc.format;
        if ((width == 0) || (height == 0)) {
            return false;
        }

        if (!vulkanDevice->vulkanMemoryModelSupported) {
            std::string reason;
            impl->failed = true;
            setStatus(lsfg::check_device(vulkanDevice->physicalDevice, reason) ? "The Vulkan memory model wasn't enabled." : reason);
            return false;
        }

        if (impl->shaders.empty() || (impl->shadersDll != impl->settings.dllPath)) {
            std::string error;
            impl->shaders.clear();
            if (impl->settings.dllPath.empty()) {
                error = "Load Lossless.dll first.";
            }
            else {
                lsfg::load_shaders(impl->settings.dllPath, impl->settings.cachePath, impl->shaders, error);
            }

            if (impl->shaders.empty()) {
                impl->failed = true;
                setStatus(error);
                return false;
            }

            impl->shadersDll = impl->settings.dllPath;
        }

        // The game frame is drawn into an image of the swap chain's size and format.
        if ((impl->source == nullptr) || (impl->device != device) || (impl->width != width) || (impl->height != height) || (impl->format != format)) {
            impl->generator.reset();
            impl->sourceFramebuffer.reset();
            impl->source = device->createTexture(RenderTextureDesc::ColorTarget(width, height, format));
            const RenderTexture *sourceTexture = impl->source.get();
            impl->sourceFramebuffer = device->createFramebuffer(RenderFramebufferDesc(&sourceTexture, 1));
            impl->device = device;
            impl->width = width;
            impl->height = height;
            impl->format = format;
        }

        if (impl->generator == nullptr) {
            lsfg::Settings settings;
            settings.generated_frames = std::clamp(impl->settings.generatedFrames, 1U, 7U);
            settings.performance = impl->settings.performance;
            settings.flow_scale = lsfg::default_flow_scale(height);
            std::string error;
            impl->generator = lsfg::FrameGenerator::create(vulkanDevice->physicalDevice, vulkanDevice->vk, impl->shaders, settings, { width, height }, error);
            if (impl->generator == nullptr) {
                impl->failed = true;
                setStatus(error);
                return false;
            }

            impl->lastGameFrame = {};
            impl->intervalNs = 0.0;
        }

        setStatus(std::string());
        return true;
#   else
        (void)(device); (void)(swapChain); (void)(vulkan);
        impl->failed = true;
        setStatus("Lossless Scaling frame generation isn't available on this platform.");
        return false;
#   endif
    }

    RenderFramebuffer *FrameGeneration::sourceFramebuffer() const {
        return impl->sourceFramebuffer.get();
    }

    RenderTexture *FrameGeneration::sourceTexture() const {
        return impl->source.get();
    }

    uint32_t FrameGeneration::record(RenderCommandList *commandList) {
#   ifdef RT64_FRAME_GENERATION
        VkCommandBuffer cmd = static_cast<VulkanCommandList *>(commandList)->vk;
        VkImage sourceImage = static_cast<VulkanTexture *>(impl->source.get())->vk;
        impl->generator->record_input(cmd, sourceImage, { impl->width, impl->height });
        return impl->generator->record_generate(cmd) ? impl->generator->settings().generated_frames : 0;
#   else
        (void)(commandList);
        return 0;
#   endif
    }

    bool FrameGeneration::sceneCut() {
#   ifdef RT64_FRAME_GENERATION
        // The real frame that cut to another picture, and the one after it.
        return impl->generator->scene_cut();
#   else
        return false;
#   endif
    }

    void FrameGeneration::recordOutput(RenderCommandList *commandList, uint32_t index, RenderTexture *dst) {
#   ifdef RT64_FRAME_GENERATION
        VkCommandBuffer cmd = static_cast<VulkanCommandList *>(commandList)->vk;
        VkImage dstImage = static_cast<VulkanTexture *>(dst)->vk;
        impl->generator->record_output(cmd, index, dstImage, { 0, 0 }, { impl->width, impl->height });
#   else
        (void)(commandList); (void)(index); (void)(dst);
#   endif
    }

    std::chrono::nanoseconds FrameGeneration::markGameFrame(uint32_t framesShown) {
        const auto now = std::chrono::steady_clock::now();
        if (impl->lastGameFrame != std::chrono::steady_clock::time_point()) {
            const double interval = double(std::chrono::duration_cast<std::chrono::nanoseconds>(now - impl->lastGameFrame).count());
            // Past 250 ms it's a hitch or a pause, not the frame rate.
            if (interval < 250'000'000.0) {
                impl->intervalNs = (impl->intervalNs > 0.0) ? (impl->intervalNs * 0.75 + interval * 0.25) : interval;
            }
        }

        impl->lastGameFrame = now;
        if ((impl->intervalNs <= 0.0) || (framesShown == 0)) {
            return std::chrono::nanoseconds(0);
        }

        return std::chrono::nanoseconds(int64_t(impl->intervalNs / double(framesShown)));
    }
};

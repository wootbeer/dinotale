#pragma once

// Lossless Scaling frame generation for a Vulkan renderer.
//
// The frame generation is lsfg-vk's (https://github.com/PancakeTAS/lsfg-vk,
// v1.0.0, MIT): Lossless Scaling's own compute shaders, read out of the
// player's copy of Lossless.dll and translated from DXBC to SPIR-V with
// DXVK's shader compiler (zlib). Nothing from Lossless Scaling is part of
// this library.
//
// Unlike lsfg-vk, which is a Vulkan layer with a device of its own, this runs
// on the host renderer's device and records into its command buffers: the
// host copies each real frame in, records generation, and copies the
// generated frames out to wherever it presents them. See README.md.
//
// Vulkan functions are called through volk's globals, so the host must have
// loaded volk for its instance (volkInitialize + volkLoadInstance) and the
// device it passes must have been created with:
//   - vulkanMemoryModel (VkPhysicalDeviceVulkanMemoryModelFeatures), and
//   - shaderStorageImageWriteWithoutFormat (core feature).
// check_device() says whether the GPU supports them.

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace lsfg {

    // SPIR-V by lsfg-vk's shader names ("mipmaps", "alpha[0]", ... "generate",
    // and the performance versions, "p_mipmaps" ... "p_generate").
    using ShaderSet = std::map<std::string, std::vector<uint8_t>>;

    // Reads the frame generation shaders out of Lossless.dll and translates
    // them. False, with a reason, if the file isn't a Lossless.dll with them.
    bool translate_shaders(const std::vector<uint8_t>& dll, ShaderSet& shaders, std::string& error);

    // As translate_shaders, from a file, with a cache: the translated shaders
    // are kept in cache_path (with a hash of the DLL) and used as long as the
    // DLL doesn't change. Translation takes a few seconds; the cache, none.
    bool load_shaders(const std::filesystem::path& dll_path, const std::filesystem::path& cache_path,
        ShaderSet& shaders, std::string& error);

    // Whether the GPU has the features the shaders need (see above).
    bool check_device(VkPhysicalDevice physical_device, std::string& reason);

    struct Settings {
        // Frames generated between two real ones: 1 doubles the frame rate,
        // 2 triples it, and so on.
        uint32_t generated_frames = 1;
        // Lossless Scaling's performance mode (LSFG 3.1 performance).
        bool performance = false;
        // Resolution of the motion estimate relative to the frame, 0.25-1.
        // Lossless Scaling suggests 1 up to 1080p and less above: see
        // default_flow_scale().
        float flow_scale = 1.0f;
        // The frames are HDR (not supported by this host interface yet).
        bool hdr = false;
    };

    // Lossless Scaling's suggested flow scale for a frame height.
    float default_flow_scale(uint32_t height);

    class FrameGenerator {
    public:
        // The images are R8G8B8A8_UNORM at the given size. Null, with a
        // reason, on failure. Creating one is slow (pipelines): do it once
        // per size, not per frame.
        static std::unique_ptr<FrameGenerator> create(VkPhysicalDevice physical_device, VkDevice device,
            const ShaderSet& shaders, const Settings& settings, VkExtent2D extent, std::string& error);

        // The GPU must be done with all recorded work first.
        ~FrameGenerator();

        VkExtent2D extent() const;
        const Settings& settings() const;

        // Records copying the next real frame in. src_image must be in
        // TRANSFER_SRC_OPTIMAL layout and support blitting; it's scaled to
        // extent() if its size differs.
        void record_input(VkCommandBuffer cmd, VkImage src_image, VkExtent2D src_extent);

        // How much the picture changed between the last two real frames put in,
        // 0 (the same) to 1 (black to white), from a thumbnail of each: a cut
        // to another scene, rather than motion, is about 0.15 or more (game
        // play, camera turns included, stays under about 0.05 at 30 fps). Call
        // after the GPU has done the work record_input() recorded; -1 before
        // there are two frames.
        float scene_difference();

        // Whether the frames to be generated this time would show a cut: frames
        // generated across one morph one picture into the other, so a host
        // should show the new real frame alone then. True for the real frame
        // that cut (scene_difference() >= threshold, or 0.02 from or to a
        // nearly black screen, where a cut changes less) and the one after it: the
        // motion estimate uses the last three real frames, so that one's
        // generated frames still draw on the picture before the cut. Call once
        // per real frame, as scene_difference().
        bool scene_cut(float threshold = 0.15f);

        // Records generating the frames between the previous real frame and
        // the one just copied in. False when there's no previous frame yet
        // (the first frame after creation), when there's nothing to show.
        bool record_generate(VkCommandBuffer cmd);

        // Records copying generated frame `index` (0 is the earliest, nearest
        // the previous real frame) to dst_image, which must be in
        // TRANSFER_DST_OPTIMAL layout; scaled to dst_extent at dst_offset.
        void record_output(VkCommandBuffer cmd, uint32_t index, VkImage dst_image,
            VkOffset2D dst_offset, VkExtent2D dst_extent);

        FrameGenerator(const FrameGenerator&) = delete;
        FrameGenerator& operator=(const FrameGenerator&) = delete;

    private:
        struct Impl;
        explicit FrameGenerator(std::unique_ptr<Impl> impl);
        std::unique_ptr<Impl> impl;
    };

}

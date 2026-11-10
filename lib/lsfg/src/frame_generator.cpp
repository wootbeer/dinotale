// lsfg::FrameGenerator: lsfg-vk's frame generation contexts on the host's
// device, with the copies in and out the host needs. See lsfg/lsfg.hpp.
#include <volk.h>

#include "lsfg/lsfg.hpp"

#include "common/exception.hpp"
#include "common/utils.hpp"
#include "core/descriptorpool.hpp"
#include "core/device.hpp"
#include "core/image.hpp"
#include "v3_1/context.hpp"
#include "v3_1p/context.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>
#include <exception>
#include <optional>

namespace lsfg {

    namespace {
        constexpr VkFormat kFormat = VK_FORMAT_R8G8B8A8_UNORM;

        // Scene cut detection: the frame shrunk to a 64x36 thumbnail, compared
        // with the last one as 8x6 cells of 8x6 pixels' average color.
        constexpr uint32_t kThumbWidth = 64, kThumbHeight = 36;
        constexpr uint32_t kCellsX = 8, kCellsY = 6;

        // Moves one of the generator's images between compute use (GENERAL)
        // and a transfer, keeping the layout it tracks up to date.
        void transition(VkCommandBuffer cmd, LSFG::Core::Image& image,
                VkPipelineStageFlags src_stage, VkAccessFlags src_access,
                VkPipelineStageFlags dst_stage, VkAccessFlags dst_access, VkImageLayout layout) {
            const VkImageMemoryBarrier barrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = src_access,
                .dstAccessMask = dst_access,
                .oldLayout = image.getLayout(),
                .newLayout = layout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image.handle(),
                .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
            };
            vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            image.setLayout(layout);
        }

        void blit(VkCommandBuffer cmd, VkImage src, VkImageLayout src_layout, VkExtent2D src_extent,
                VkImage dst, VkImageLayout dst_layout, VkOffset2D dst_offset, VkExtent2D dst_extent) {
            const VkImageBlit region{
                .srcSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1 },
                .srcOffsets = { { 0, 0, 0 }, { int32_t(src_extent.width), int32_t(src_extent.height), 1 } },
                .dstSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1 },
                .dstOffsets = { { dst_offset.x, dst_offset.y, 0 },
                    { dst_offset.x + int32_t(dst_extent.width), dst_offset.y + int32_t(dst_extent.height), 1 } }
            };
            const bool same_size = (src_extent.width == dst_extent.width) && (src_extent.height == dst_extent.height);
            vkCmdBlitImage(cmd, src, src_layout, dst, dst_layout, 1, &region,
                same_size ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);
        }
    }

    struct FrameGenerator::Impl {
        Settings settings;
        VkExtent2D extent{};
        // Before the contexts: their descriptor sets come from its pool.
        LSFG::Vulkan vk;
        std::optional<LSFG_3_1::Context> context;
        std::optional<LSFG_3_1P::Context> performance_context;

        // Scene cut detection: a thumbnail of each real frame, read back.
        VkDevice device = VK_NULL_HANDLE;
        LSFG::Core::Image thumbnail;
        VkBuffer readback = VK_NULL_HANDLE;
        VkDeviceMemory readback_memory = VK_NULL_HANDLE;
        const uint8_t* readback_data = nullptr;
        bool thumbnail_pending = false;
        std::vector<float> last_cells;
        float last_difference = -1.0f;
        // Mean brightness (0-1) of the last two thumbnails.
        float brightness = -1.0f, last_brightness = -1.0f;
        // Real frames put in, and the one that last cut to another picture.
        uint64_t inputs = 0;
        uint64_t last_cut = UINT64_MAX;

        LSFG::Core::Image& next_input() {
            return context ? context->nextInput() : performance_context->nextInput();
        }
        std::vector<LSFG::Core::Image>& outputs() {
            return context ? context->outputs() : performance_context->outputs();
        }
        uint64_t frame_count() const {
            return context ? context->frameCount() : performance_context->frameCount();
        }
    };

    float default_flow_scale(uint32_t height) {
        if (height <= 1080) {
            return 1.0f;
        }
        return std::clamp(1080.0f / float(height), 0.25f, 1.0f);
    }

    bool check_device(VkPhysicalDevice physical_device, std::string& reason) {
        VkPhysicalDeviceVulkanMemoryModelFeatures memory_model{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES
        };
        VkPhysicalDeviceFeatures2 features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &memory_model
        };
        vkGetPhysicalDeviceFeatures2(physical_device, &features);
        if (!memory_model.vulkanMemoryModel) {
            reason = "The GPU doesn't support the Vulkan memory model.";
            return false;
        }
        if (!features.features.shaderStorageImageWriteWithoutFormat) {
            reason = "The GPU doesn't support storage image writes without a format.";
            return false;
        }
        VkFormatProperties format{};
        vkGetPhysicalDeviceFormatProperties(physical_device, kFormat, &format);
        const VkFormatFeatureFlags needed = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
            VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
        if ((format.optimalTilingFeatures & needed) != needed) {
            reason = "The GPU can't use RGBA8 images for frame generation.";
            return false;
        }
        return true;
    }

    std::unique_ptr<FrameGenerator> FrameGenerator::create(VkPhysicalDevice physical_device, VkDevice device,
            const ShaderSet& shaders, const Settings& settings, VkExtent2D extent, std::string& error) {
        if (settings.generated_frames < 1 || extent.width == 0 || extent.height == 0) {
            error = "Invalid frame generation settings.";
            return nullptr;
        }
        try {
            auto impl = std::make_unique<Impl>();
            impl->settings = settings;
            impl->extent = extent;
            const LSFG::Core::Device core_device(physical_device, device);
            impl->vk = LSFG::Vulkan{
                .device = core_device,
                .descriptorPool = LSFG::Core::DescriptorPool(core_device),
                .generationCount = settings.generated_frames,
                .flowScale = std::clamp(settings.flow_scale, 0.25f, 1.0f),
                .isHdr = settings.hdr,
                .shaders = LSFG::Pool::ShaderPool([&shaders](const std::string& name) {
                    auto it = shaders.find(name);
                    return (it != shaders.end()) ? it->second : std::vector<uint8_t>();
                }),
                .resources = LSFG::Pool::ResourcePool(settings.hdr, std::clamp(settings.flow_scale, 0.25f, 1.0f))
            };
            if (settings.performance) {
                impl->performance_context.emplace(impl->vk, extent, kFormat);
            }
            else {
                impl->context.emplace(impl->vk, extent, kFormat);
            }
            // The shaders are compiled into pipelines by now; the set isn't kept.
            impl->vk.shaders = LSFG::Pool::ShaderPool();

            // Scene cut detection: the thumbnail and a mapped buffer to read it back.
            impl->device = device;
            impl->thumbnail = LSFG::Core::Image(core_device, { kThumbWidth, kThumbHeight }, kFormat,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT);     // sampled: every Core::Image gets a view
            const VkBufferCreateInfo buffer_info{
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = VkDeviceSize(kThumbWidth) * kThumbHeight * 4,
                .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };
            if (vkCreateBuffer(device, &buffer_info, nullptr, &impl->readback) != VK_SUCCESS) {
                throw std::runtime_error("Couldn't create the scene detection buffer.");
            }
            VkMemoryRequirements requirements;
            vkGetBufferMemoryRequirements(device, impl->readback, &requirements);
            VkPhysicalDeviceMemoryProperties memory;
            vkGetPhysicalDeviceMemoryProperties(physical_device, &memory);
            const VkMemoryPropertyFlags wanted = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            uint32_t type = UINT32_MAX;
            for (uint32_t i = 0; i < memory.memoryTypeCount; i++) {
                if ((requirements.memoryTypeBits & (1u << i)) && ((memory.memoryTypes[i].propertyFlags & wanted) == wanted)) {
                    type = i;
                    break;
                }
            }
            const VkMemoryAllocateInfo allocate_info{
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = requirements.size,
                .memoryTypeIndex = type
            };
            void* mapped = nullptr;
            if ((type == UINT32_MAX) || (vkAllocateMemory(device, &allocate_info, nullptr, &impl->readback_memory) != VK_SUCCESS) ||
                (vkBindBufferMemory(device, impl->readback, impl->readback_memory, 0) != VK_SUCCESS) ||
                (vkMapMemory(device, impl->readback_memory, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS)) {
                throw std::runtime_error("Couldn't set up the scene detection buffer.");
            }
            impl->readback_data = static_cast<const uint8_t*>(mapped);
            return std::unique_ptr<FrameGenerator>(new FrameGenerator(std::move(impl)));
        }
        catch (const std::exception& e) {
            error = std::string("Frame generation failed to start: ") + e.what();
            return nullptr;
        }
    }

    FrameGenerator::FrameGenerator(std::unique_ptr<Impl> impl) : impl(std::move(impl)) {}

    FrameGenerator::~FrameGenerator() {
        if (impl) {
            // The contexts' objects go before the pool they come from.
            impl->context.reset();
            impl->performance_context.reset();
            if (impl->readback != VK_NULL_HANDLE) {
                vkDestroyBuffer(impl->device, impl->readback, nullptr);
            }
            if (impl->readback_memory != VK_NULL_HANDLE) {
                vkFreeMemory(impl->device, impl->readback_memory, nullptr);
            }
        }
    }

    VkExtent2D FrameGenerator::extent() const {
        return impl->extent;
    }

    const Settings& FrameGenerator::settings() const {
        return impl->settings;
    }

    void FrameGenerator::record_input(VkCommandBuffer cmd, VkImage src_image, VkExtent2D src_extent) {
        LSFG::Core::Image& input = impl->next_input();
        transition(cmd, input, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        blit(cmd, src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, src_extent,
            input.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { 0, 0 }, impl->extent);
        transition(cmd, input, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL);

        // The thumbnail for scene cut detection, copied to the read back buffer.
        LSFG::Core::Image& thumbnail = impl->thumbnail;
        transition(cmd, thumbnail, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        blit(cmd, src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, src_extent,
            thumbnail.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { 0, 0 }, { kThumbWidth, kThumbHeight });
        transition(cmd, thumbnail, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        const VkBufferImageCopy region{
            .imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1 },
            .imageExtent = { kThumbWidth, kThumbHeight, 1 }
        };
        vkCmdCopyImageToBuffer(cmd, thumbnail.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, impl->readback, 1, &region);
        const VkBufferMemoryBarrier to_host{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = impl->readback,
            .size = VK_WHOLE_SIZE
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &to_host, 0, nullptr);
        impl->thumbnail_pending = true;
        impl->inputs++;
    }

    float FrameGenerator::scene_difference() {
        if (!impl->thumbnail_pending) {
            return impl->last_difference;
        }

        impl->thumbnail_pending = false;
        std::vector<float> cells(kCellsX * kCellsY * 3, 0.0f);
        const uint32_t cell_w = kThumbWidth / kCellsX, cell_h = kThumbHeight / kCellsY;
        for (uint32_t y = 0; y < kThumbHeight; y++) {
            for (uint32_t x = 0; x < kThumbWidth; x++) {
                const uint8_t* p = impl->readback_data + (size_t(y) * kThumbWidth + x) * 4;
                float* cell = &cells[((y / cell_h) * kCellsX + (x / cell_w)) * 3];
                cell[0] += p[0];
                cell[1] += p[1];
                cell[2] += p[2];
            }
        }
        for (float& c : cells) {
            c /= float(cell_w * cell_h) * 255.0f;
        }

        float brightness = 0.0f;
        for (float c : cells) {
            brightness += c;
        }
        impl->last_brightness = impl->brightness;
        impl->brightness = brightness / float(cells.size());

        if (impl->last_cells.size() == cells.size()) {
            float sum = 0.0f;
            for (size_t i = 0; i < cells.size(); i++) {
                sum += std::fabs(cells[i] - impl->last_cells[i]);
            }
            impl->last_difference = sum / float(cells.size());
        }

        impl->last_cells = std::move(cells);
        return impl->last_difference;
    }

    bool FrameGenerator::scene_cut(float threshold) {
        // A cut from or to a (nearly) black screen changes little of the
        // picture when the other is mostly dark too (black to a logo on black:
        // 0.03-0.09), but it's still a cut. Game play stays far brighter
        // (0.16 or more in the owner's clips).
        constexpr float kDark = 0.03f, kDarkCut = 0.02f;
        const float difference = scene_difference();
        const bool dark = (impl->last_brightness >= 0.0f) && (std::min(impl->brightness, impl->last_brightness) < kDark);
        if ((difference >= threshold) || (dark && (difference >= kDarkCut))) {
            impl->last_cut = impl->inputs;
        }
        return (impl->last_cut != UINT64_MAX) && (impl->inputs - impl->last_cut <= 1);
    }

    bool FrameGenerator::record_generate(VkCommandBuffer cmd) {
        const bool has_previous = impl->frame_count() > 0;
        const LSFG::Core::CommandBuffer buffer(cmd);
        if (impl->context) {
            impl->context->record(buffer, impl->settings.generated_frames);
        }
        else {
            impl->performance_context->record(buffer, impl->settings.generated_frames);
        }
        return has_previous;
    }

    void FrameGenerator::record_output(VkCommandBuffer cmd, uint32_t index, VkImage dst_image,
            VkOffset2D dst_offset, VkExtent2D dst_extent) {
        std::vector<LSFG::Core::Image>& outputs = impl->outputs();
        if (index >= outputs.size()) {
            return;
        }
        LSFG::Core::Image& output = outputs[index];
        transition(cmd, output, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        blit(cmd, output.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, impl->extent,
            dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dst_offset, dst_extent);
        transition(cmd, output, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);
    }

}

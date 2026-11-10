#include <volk.h>
#include <vulkan/vulkan_core.h>

#include "common/utils.hpp"
#include "core/image.hpp"

using namespace LSFG;
using namespace LSFG::Utils;

namespace {
    VkImageMemoryBarrier makeBarrier(Core::Image& image, VkAccessFlags src, VkAccessFlags dst) {
        const VkImageMemoryBarrier barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = src,
            .dstAccessMask = dst,
            .oldLayout = image.getLayout(),
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image.handle(),
            .subresourceRange = {
                .aspectMask = image.getAspectFlags(),
                .levelCount = 1,
                .layerCount = 1
            }
        };
        image.setLayout(VK_IMAGE_LAYOUT_GENERAL);
        return barrier;
    }
}

BarrierBuilder& BarrierBuilder::addR2W(Core::Image& image) {
    this->barriers.emplace_back(makeBarrier(image, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT));
    return *this;
}

BarrierBuilder& BarrierBuilder::addW2R(Core::Image& image) {
    this->barriers.emplace_back(makeBarrier(image, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT));
    return *this;
}

void BarrierBuilder::build() const {
    vkCmdPipelineBarrier(this->commandBuffer->handle(),
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 0, nullptr,
        static_cast<uint32_t>(this->barriers.size()), this->barriers.data());
}

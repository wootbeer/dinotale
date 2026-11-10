#pragma once

#include "core/commandbuffer.hpp"
#include "core/descriptorpool.hpp"
#include "core/image.hpp"
#include "core/device.hpp"
#include "pool/resourcepool.hpp"
#include "pool/shaderpool.hpp"

#include <vulkan/vulkan_core.h>

#include <optional>
#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>

namespace LSFG::Utils {

    ///
    /// Insert memory barriers between compute passes.
    ///
    /// (BattleTanx GA port: plain vkCmdPipelineBarrier, so the host device
    /// needn't enable synchronization2.)
    ///
    class BarrierBuilder {
    public:
        BarrierBuilder(const Core::CommandBuffer& buffer)
                : commandBuffer(&buffer) {
            this->barriers.reserve(16); // this is performance critical
        }

        /// Add a resource that is read before and written after.
        BarrierBuilder& addR2W(Core::Image& image);
        /// Add a resource that is written before and read after.
        BarrierBuilder& addW2R(Core::Image& image);

        BarrierBuilder& addR2W(std::optional<Core::Image>& image) {
            if (image.has_value()) this->addR2W(*image); return *this; }
        BarrierBuilder& addW2R(std::optional<Core::Image>& image) {
            if (image.has_value()) this->addW2R(*image); return *this; }
        BarrierBuilder& addR2W(std::vector<Core::Image>& images) {
            for (auto& image : images) this->addR2W(image); return *this; }
        BarrierBuilder& addW2R(std::vector<Core::Image>& images) {
            for (auto& image : images) this->addW2R(image); return *this; }
        template<std::size_t N>
        BarrierBuilder& addR2W(std::array<Core::Image, N>& images) {
            for (auto& image : images) this->addR2W(image); return *this; }
        template<std::size_t N>
        BarrierBuilder& addW2R(std::array<Core::Image, N>& images) {
            for (auto& image : images) this->addW2R(image); return *this; }

        /// Insert the barriers.
        void build() const;
    private:
        const Core::CommandBuffer* commandBuffer;
        std::vector<VkImageMemoryBarrier> barriers;
    };

}

namespace LSFG {
    struct Vulkan {
        Core::Device device;
        Core::DescriptorPool descriptorPool;

        uint64_t generationCount;
        float flowScale;
        bool isHdr;

        Pool::ShaderPool shaders;
        Pool::ResourcePool resources;
    };
}

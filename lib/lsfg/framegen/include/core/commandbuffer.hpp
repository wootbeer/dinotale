#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>

namespace LSFG::Core {

    ///
    /// A command buffer the host is recording (BattleTanx GA port: frame
    /// generation records into the renderer's command list). Not owned.
    ///
    class CommandBuffer {
    public:
        CommandBuffer() noexcept = default;
        explicit CommandBuffer(VkCommandBuffer commandBuffer)
            : commandBuffer(commandBuffer) {}

        void dispatch(uint32_t x, uint32_t y, uint32_t z) const;

        [[nodiscard]] VkCommandBuffer handle() const { return this->commandBuffer; }
    private:
        VkCommandBuffer commandBuffer{};
    };

}

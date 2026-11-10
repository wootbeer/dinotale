#include <volk.h>
#include <vulkan/vulkan_core.h>

#include "core/commandbuffer.hpp"

#include <cstdint>

using namespace LSFG::Core;

void CommandBuffer::dispatch(uint32_t x, uint32_t y, uint32_t z) const {
    vkCmdDispatch(this->commandBuffer, x, y, z);
}

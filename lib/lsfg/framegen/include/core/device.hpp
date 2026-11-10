#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>

namespace LSFG::Core {

    ///
    /// The host's Vulkan device (BattleTanx GA port: frame generation runs on the
    /// renderer's own device instead of creating one). Not owned.
    ///
    class Device {
    public:
        Device() noexcept = default;
        Device(VkPhysicalDevice physicalDevice, VkDevice device)
            : device(device), physicalDevice(physicalDevice) {}

        [[nodiscard]] VkDevice handle() const { return this->device; }
        [[nodiscard]] VkPhysicalDevice getPhysicalDevice() const { return this->physicalDevice; }
    private:
        VkDevice device{};
        VkPhysicalDevice physicalDevice{};
    };

}

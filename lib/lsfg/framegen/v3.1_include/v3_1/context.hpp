#pragma once

#include "core/image.hpp"
#include "core/commandbuffer.hpp"
#include "shaders/alpha.hpp"
#include "shaders/beta.hpp"
#include "shaders/delta.hpp"
#include "shaders/gamma.hpp"
#include "shaders/generate.hpp"
#include "shaders/mipmaps.hpp"
#include "common/utils.hpp"

#include <vulkan/vulkan_core.h>

#include <vector>
#include <cstdint>
#include <array>

namespace LSFG_3_1 {
    using namespace LSFG;

    ///
    /// Frame generation for one image size (BattleTanx GA port: owns its input
    /// and output images and records into the host's command buffer, instead
    /// of importing images and submitting work itself).
    ///
    class Context {
    public:
        ///
        /// Create a context.
        ///
        /// @param vk The Vulkan objects to use.
        /// @param extent The size of the images.
        /// @param format The format of the images.
        ///
        /// @throws LSFG::vulkan_error if the context fails to initialize.
        ///
        Context(Vulkan& vk, VkExtent2D extent, VkFormat format);

        /// The input image the next real frame is to be written to.
        [[nodiscard]] Core::Image& nextInput() { return (this->frameIdx % 2 == 0) ? this->inImg_0 : this->inImg_1; }

        /// The generated frames, oldest first.
        [[nodiscard]] std::vector<Core::Image>& outputs() { return this->generate.getOutImages(); }

        /// Real frames seen so far.
        [[nodiscard]] uint64_t frameCount() const { return this->frameIdx; }

        ///
        /// Record frame generation between the previous real frame and the one
        /// in nextInput(), then move on to the next input.
        ///
        void record(const Core::CommandBuffer& buf, uint64_t generationCount);

        // Trivially copyable, moveable and destructible
        Context(const Context&) = default;
        Context& operator=(const Context&) = default;
        Context(Context&&) = default;
        Context& operator=(Context&&) = default;
        ~Context() = default;
    private:
        Core::Image inImg_0, inImg_1; // inImg_0 is next when fc % 2 == 0
        uint64_t frameIdx{0};

        Shaders::Mipmaps mipmaps;
        std::array<Shaders::Alpha, 7> alpha;
        Shaders::Beta beta;
        std::array<Shaders::Gamma, 7> gamma;
        std::array<Shaders::Delta, 3> delta;
        Shaders::Generate generate;
    };

}

#pragma once

#include "core/buffer.hpp"
#include "core/commandbuffer.hpp"
#include "core/descriptorset.hpp"
#include "core/image.hpp"
#include "core/pipeline.hpp"
#include "core/sampler.hpp"
#include "core/shadermodule.hpp"
#include "common/utils.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace LSFG_3_1P::Shaders {

    using namespace LSFG;

    ///
    /// Gamma shader.
    ///
    class Gamma {
    public:
        Gamma() = default;

        ///
        /// Initialize the shaderchain.
        ///
        /// @param inImgs1 Three sets of two RGBA images, corresponding to a frame count % 3.
        /// @param inImg2 Second Input image
        /// @param optImg Optional image for non-first passes.
        ///
        /// @throws LSFG::vulkan_error if resource creation fails.
        ///
        Gamma(Vulkan& vk, std::array<std::array<Core::Image, 2>, 3> inImgs1,
            Core::Image inImg2, std::optional<Core::Image> optImg);

        ///
        /// Dispatch the shaderchain.
        ///
        void Dispatch(const Core::CommandBuffer& buf, uint64_t frameCount, uint64_t pass_idx);

        /// Get the output image
        [[nodiscard]] const auto& getOutImage() const { return this->outImg; }

        /// Trivially copyable, moveable and destructible
        Gamma(const Gamma&) noexcept = default;
        Gamma& operator=(const Gamma&) noexcept = default;
        Gamma(Gamma&&) noexcept = default;
        Gamma& operator=(Gamma&&) noexcept = default;
        ~Gamma() = default;
    private:
        std::array<Core::ShaderModule, 5> shaderModules;
        std::array<Core::Pipeline, 5> pipelines;
        std::array<Core::Sampler, 3> samplers;
        struct GammaPass {
            Core::Buffer buffer;
            std::array<Core::DescriptorSet, 3> firstDescriptorSet;
            std::array<Core::DescriptorSet, 4> descriptorSets;
        };
        std::vector<GammaPass> passes;

        std::array<std::array<Core::Image, 2>, 3> inImgs1;
        Core::Image inImg2;
        std::optional<Core::Image> optImg;
        /// Stands in for absent optional images (the first pass has no level
        /// before it): Vulkan wants a valid image in every binding the shader
        /// declares; the shader leaves it unused (firstIter), and it reads as
        /// zero, as Direct3D's null view does. (BattleTanx GA port.)
        std::optional<Core::Image> blank;
        bool blankCleared{false};
        std::array<Core::Image, 3> tempImgs1;
        std::array<Core::Image, 2> tempImgs2;
        Core::Image outImg;
    };

}

#include <volk.h>
#include <vulkan/vulkan_core.h>

#include "v3_1p/context.hpp"
#include "common/utils.hpp"

#include <vector>
#include <cstddef>
#include <algorithm>
#include <optional>
#include <cstdint>

using namespace LSFG;
using namespace LSFG_3_1P;

Context::Context(Vulkan& vk, VkExtent2D extent, VkFormat format) {
    // input images, written by the host
    this->inImg_0 = Core::Image(vk.device, extent, format,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);
    this->inImg_1 = Core::Image(vk.device, extent, format,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    // create shader chains
    this->mipmaps = Shaders::Mipmaps(vk, this->inImg_0, this->inImg_1);
    for (size_t i = 0; i < 7; i++)
        this->alpha.at(i) = Shaders::Alpha(vk, this->mipmaps.getOutImages().at(i));
    this->beta = Shaders::Beta(vk, this->alpha.at(0).getOutImages());
    for (size_t i = 0; i < 7; i++) {
        this->gamma.at(i) = Shaders::Gamma(vk,
            this->alpha.at(6 - i).getOutImages(),
            this->beta.getOutImages().at(std::min<size_t>(6 - i, 5)),
            (i == 0) ? std::nullopt : std::make_optional(this->gamma.at(i - 1).getOutImage()));
        if (i < 4) continue;

        this->delta.at(i - 4) = Shaders::Delta(vk,
            this->alpha.at(6 - i).getOutImages(),
            this->beta.getOutImages().at(6 - i),
            (i == 4) ? std::nullopt : std::make_optional(this->gamma.at(i - 1).getOutImage()),
            (i == 4) ? std::nullopt : std::make_optional(this->delta.at(i - 5).getOutImage1()),
            (i == 4) ? std::nullopt : std::make_optional(this->delta.at(i - 5).getOutImage2()));
    }
    this->generate = Shaders::Generate(vk,
        this->inImg_0, this->inImg_1,
        this->gamma.at(6).getOutImage(),
        this->delta.at(2).getOutImage1(),
        this->delta.at(2).getOutImage2(),
        format);
}

void Context::record(const Core::CommandBuffer& buf, uint64_t generationCount) {
    // 1. create mipmaps and process input image
    this->mipmaps.Dispatch(buf, this->frameIdx);
    for (size_t i = 0; i < 7; i++)
        this->alpha.at(6 - i).Dispatch(buf, this->frameIdx);
    this->beta.Dispatch(buf, this->frameIdx);

    // 2. generate intermediary frames
    for (size_t pass = 0; pass < generationCount; pass++) {
        for (size_t i = 0; i < 7; i++) {
            this->gamma.at(i).Dispatch(buf, this->frameIdx, pass);
            if (i >= 4)
                this->delta.at(i - 4).Dispatch(buf, this->frameIdx, pass);
        }
        this->generate.Dispatch(buf, this->frameIdx, pass);
    }

    this->frameIdx++;
}

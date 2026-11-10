#pragma once
// Stand-in for the parts of DXVK's dxvk_pipelayout.h / dxvk_shader.h that the
// DXBC compiler uses (BattleTanx GA port: the compiler is used on its own, to
// turn DXBC compute shaders into SPIR-V). Copied from DXVK v2.6.2.
#include <cstdint>
#include <optional>
#include <vector>
#include <vulkan/vulkan_core.h>
#include "../util/util_flags.h"
#include "../util/util_error.h"
#include "../util/rc/util_rc.h"
#include "../util/rc/util_rc_ptr.h"
#include "../util/util_small_vector.h"
#include "../spirv/spirv_module.h"

namespace dxvk {

  struct DxvkAccessOp {
    enum OpType : uint16_t {
      None      = 0x0u,
      Or        = 0x1u,
      And       = 0x2u,
      Xor       = 0x3u,
      Add       = 0x4u,
      IMin      = 0x5u,
      IMax      = 0x6u,
      UMin      = 0x7u,
      UMax      = 0x8u,

      StoreF    = 0xdu,
      StoreUi   = 0xeu,
      StoreSi   = 0xfu,
    };

    DxvkAccessOp() = default;
    DxvkAccessOp(OpType t)
    : op(uint16_t(t)) { }

    DxvkAccessOp(OpType t, uint16_t constant)
    : op(uint16_t(t) | (constant << 4u)) { }

    uint16_t op = 0u;

    bool operator == (const DxvkAccessOp& t) const { return op == t.op; }
    bool operator != (const DxvkAccessOp& t) const { return op != t.op; }

    template<typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
    explicit operator T() const { return op; }
  };

  static_assert(sizeof(DxvkAccessOp) == sizeof(uint16_t));

  struct DxvkBindingInfo {
    VkDescriptorType      descriptorType  = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    uint32_t              resourceBinding = 0u;
    VkImageViewType       viewType        = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    VkShaderStageFlagBits stage           = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    VkAccessFlags         access          = 0u;
    DxvkAccessOp          accessOp        = DxvkAccessOp::None;
    bool                  uboSet          = false;
    bool                  isMultisampled  = false;
  };

}

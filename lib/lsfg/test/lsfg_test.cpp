// lsfg test: frame generation on a Vulkan device of its own, no renderer.
//
//   lsfg_test <Lossless.dll> [cache file] [performance]
//
// Five 256x128 frames go in with a white square moving 40 pixels a frame
// (x 20, 60, 100...); each frame generated between two should have it about
// halfway. Prints where it is and exits 0 if each is within 12 pixels.
#include <volk.h>

#include "lsfg/lsfg.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
    constexpr uint32_t W = 256, H = 128, SQUARE = 24;

    VkInstance instance;
    VkPhysicalDevice physical;
    VkDevice device;
    VkQueue queue;
    uint32_t family = 0;
    VkCommandPool pool;

    uint32_t memory_type(uint32_t bits, VkMemoryPropertyFlags flags) {
        VkPhysicalDeviceMemoryProperties props;
        vkGetPhysicalDeviceMemoryProperties(physical, &props);
        for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
            if ((bits & (1u << i)) && (props.memoryTypes[i].propertyFlags & flags) == flags) {
                return i;
            }
        }
        return 0;
    }

    struct Buffer { VkBuffer buffer; VkDeviceMemory memory; void* data; };
    Buffer make_buffer(VkDeviceSize size) {
        Buffer b{};
        VkBufferCreateInfo info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        info.size = size;
        info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        vkCreateBuffer(device, &info, nullptr, &b.buffer);
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(device, b.buffer, &req);
        VkMemoryAllocateInfo alloc{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device, &alloc, nullptr, &b.memory);
        vkBindBufferMemory(device, b.buffer, b.memory, 0);
        vkMapMemory(device, b.memory, 0, size, 0, &b.data);
        return b;
    }

    struct Image { VkImage image; VkDeviceMemory memory; };
    Image make_image() {
        Image im{};
        VkImageCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = VK_FORMAT_B8G8R8A8_UNORM; // like a swap chain
        info.extent = { W, H, 1 };
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        vkCreateImage(device, &info, nullptr, &im.image);
        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(device, im.image, &req);
        VkMemoryAllocateInfo alloc{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(device, &alloc, nullptr, &im.memory);
        vkBindImageMemory(device, im.image, im.memory, 0);
        return im;
    }

    void layout(VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to) {
        VkImageMemoryBarrier b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        b.oldLayout = from;
        b.newLayout = to;
        b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = image;
        b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
    }

    VkCommandBuffer begin() {
        VkCommandBufferAllocateInfo info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        info.commandPool = pool;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = 1;
        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(device, &info, &cmd);
        VkCommandBufferBeginInfo begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        vkBeginCommandBuffer(cmd, &begin);
        return cmd;
    }

    void submit(VkCommandBuffer cmd) {
        vkEndCommandBuffer(cmd);
        VkSubmitInfo info{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        info.commandBufferCount = 1;
        info.pCommandBuffers = &cmd;
        vkQueueSubmit(queue, 1, &info, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);
        vkFreeCommandBuffers(device, pool, 1, &cmd);
    }

    // A frame with the square at x, BGRA.
    void draw(uint8_t* pixels, uint32_t x) {
        std::memset(pixels, 0, W * H * 4);
        for (uint32_t y = (H - SQUARE) / 2; y < (H + SQUARE) / 2; y++) {
            for (uint32_t i = x; i < x + SQUARE; i++) {
                std::memset(pixels + (y * W + i) * 4, 255, 4);
            }
        }
    }

    // Where the square's middle is in a frame: the mean x of bright pixels.
    double find(const uint8_t* pixels) {
        double sum = 0, count = 0;
        for (uint32_t y = 0; y < H; y++) {
            for (uint32_t x = 0; x < W; x++) {
                const uint8_t* p = pixels + (y * W + x) * 4;
                if (p[0] > 128 && p[1] > 128 && p[2] > 128) {
                    sum += x;
                    count++;
                }
            }
        }
        return count > 0 ? sum / count : -1.0;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: lsfg_test <Lossless.dll> [cache file] [performance]\n");
        return 2;
    }
    if (volkInitialize() != VK_SUCCESS) {
        fprintf(stderr, "no Vulkan\n");
        return 1;
    }
    VkApplicationInfo app{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo instance_info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instance_info.pApplicationInfo = &app;
    if (vkCreateInstance(&instance_info, nullptr, &instance) != VK_SUCCESS) {
        fprintf(stderr, "vkCreateInstance failed\n");
        return 1;
    }
    volkLoadInstance(instance);
    uint32_t count = 1;
    vkEnumeratePhysicalDevices(instance, &count, &physical);
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical, &props);
    printf("device: %s\n", props.deviceName);

    std::string reason;
    if (!lsfg::check_device(physical, reason)) {
        fprintf(stderr, "unsupported: %s\n", reason.c_str());
        return 1;
    }

    // Queue family with graphics or compute.
    uint32_t families = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &families, nullptr);
    std::vector<VkQueueFamilyProperties> family_props(families);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &families, family_props.data());
    for (uint32_t i = 0; i < families; i++) {
        if (family_props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            family = i;
            break;
        }
    }
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    queue_info.queueFamilyIndex = family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;
    VkPhysicalDeviceVulkanMemoryModelFeatures memory_model{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES };
    memory_model.vulkanMemoryModel = VK_TRUE;
    VkPhysicalDeviceFeatures features{};
    features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
    VkDeviceCreateInfo device_info{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    device_info.pNext = &memory_model;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.pEnabledFeatures = &features;
    if (vkCreateDevice(physical, &device_info, nullptr, &device) != VK_SUCCESS) {
        fprintf(stderr, "vkCreateDevice failed\n");
        return 1;
    }
    vkGetDeviceQueue(device, family, 0, &queue);
    VkCommandPoolCreateInfo pool_info{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    pool_info.queueFamilyIndex = family;
    vkCreateCommandPool(device, &pool_info, nullptr, &pool);

    auto t0 = std::chrono::steady_clock::now();
    lsfg::ShaderSet shaders;
    std::string error;
    if (!lsfg::load_shaders(argv[1], argc > 2 ? argv[2] : "", shaders, error)) {
        fprintf(stderr, "shaders: %s\n", error.c_str());
        return 1;
    }
    auto t1 = std::chrono::steady_clock::now();
    printf("shaders: %zu in %.2f s\n", shaders.size(), std::chrono::duration<double>(t1 - t0).count());

    lsfg::Settings settings;
    settings.generated_frames = 1;
    settings.performance = argc > 3 && std::string(argv[3]) == "performance";
    auto generator = lsfg::FrameGenerator::create(physical, device, shaders, settings, { W, H }, error);
    if (!generator) {
        fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    auto t2 = std::chrono::steady_clock::now();
    printf("generator (%s): %.2f s\n", settings.performance ? "performance" : "quality",
        std::chrono::duration<double>(t2 - t1).count());

    Buffer staging = make_buffer(W * H * 4);
    Image frame = make_image();
    Image out = make_image();
    const uint32_t positions[] = { 20, 60, 100, 140, 180 };
    bool all_ok = true;
    for (int f = 0; f < 5; f++) {
        double found = -1;
        draw(static_cast<uint8_t*>(staging.data), positions[f]);
        VkCommandBuffer cmd = begin();
        layout(cmd, frame.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy region{};
        region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        region.imageExtent = { W, H, 1 };
        vkCmdCopyBufferToImage(cmd, staging.buffer, frame.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        layout(cmd, frame.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        generator->record_input(cmd, frame.image, { W, H });
        const bool generated = generator->record_generate(cmd);
        if (generated) {
            layout(cmd, out.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            generator->record_output(cmd, 0, out.image, { 0, 0 }, { W, H });
            layout(cmd, out.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            vkCmdCopyImageToBuffer(cmd, out.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.buffer, 1, &region);
        }
        auto s = std::chrono::steady_clock::now();
        submit(cmd);
        printf("frame %d: generated %d, %.1f ms\n", f, generated,
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - s).count());
        if (generated) {
            found = find(static_cast<const uint8_t*>(staging.data));
            const double expected = (positions[f - 1] + positions[f]) / 2.0 + SQUARE / 2.0 - 0.5;
            printf("  square middle: generated %.1f, expected about %.1f\n", found, expected);
            all_ok = all_ok && found > expected - 12 && found < expected + 12;
            if (const char* dump = getenv("LSFG_TEST_DUMP")) { // the generated frame, as a PPM
                FILE* file = fopen(dump, "wb");
                fprintf(file, "P6 %u %u 255\n", W, H);
                const uint8_t* p = static_cast<const uint8_t*>(staging.data);
                for (uint32_t i = 0; i < W * H; i++) {
                    const uint8_t rgb[3] = { p[i * 4 + 2], p[i * 4 + 1], p[i * 4 + 0] };
                    fwrite(rgb, 1, 3, file);
                }
                fclose(file);
            }
        }
    }
    generator.reset();
    vkDeviceWaitIdle(device);
    return all_ok ? 0 : 1;
}

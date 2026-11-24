#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include <cstddef>
#include <cstdint>

struct SDL_Window;

namespace gfx {

#define VK_CHECK(expr)                                                          \
    do {                                                                        \
        VkResult vkCheckResult_ = (expr);                                       \
        if (vkCheckResult_ != VK_SUCCESS)                                       \
            ::gfx::vkFail(#expr, vkCheckResult_, __FILE__, __LINE__);           \
    } while (0)

[[noreturn]] void vkFail(const char* expr, VkResult result, const char* file, int line);

struct Image {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
};

struct ImageDesc {
    VkExtent2D extent{};
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags usage = 0;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    // Attachement jamais relu : la mémoire reste dans les tuiles du GPU.
    bool transient = false;
    const char* name = nullptr;
};

struct Buffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;
    void* mapped = nullptr;
    VkDeviceSize size = 0;
};

// Instance, périphérique, file et allocateur. Un seul par processus.
struct Gpu {
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t queueFamily = 0;
    VkQueue queue = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties{};
    bool validation = false;
    uint32_t validationErrors = 0;

    // window peut être nul : rendu hors écran, sans surface ni swapchain.
    void init(SDL_Window* window, bool wantValidation);
    void shutdown();

    void createSurface(SDL_Window* window);
    void destroySurface();

    Image createImage(const ImageDesc& desc);
    void destroyImage(Image& image);

    // Texture à partir de pixels en mémoire, prête à être lue par un shader.
    Image createTexture(VkExtent2D extent, VkFormat format, const void* pixels,
                        VkDeviceSize sizeBytes, const char* name = nullptr);

    Buffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, bool hostVisible,
                        const char* name = nullptr);
    void destroyBuffer(Buffer& buffer);

    VkShaderModule createShader(const uint32_t* code, size_t sizeBytes);

    void setName(VkObjectType type, uint64_t handle, const char* name);
};

VkImageAspectFlags aspectOf(VkFormat format);

// Transition de disposition d'image. Les étapes et accès sont déduits des deux
// dispositions, ce qui suffit pour les cas utilisés par le moteur.
void transition(VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to,
                VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);

} // namespace gfx

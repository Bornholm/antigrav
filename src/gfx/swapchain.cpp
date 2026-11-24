#include "gfx/swapchain.h"

#include "core/log.h"

#include <algorithm>

namespace gfx {

bool Swapchain::create(Gpu& gpu, uint32_t width, uint32_t height, bool vsync) {
    auto lost = [&] {
        destroy(gpu);
        surfaceLost = true;
        return false;
    };
    surfaceLost = false;
    VkSurfaceCapabilitiesKHR caps;
    VkResult queried = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu.physical, gpu.surface, &caps);
    if (queried == VK_ERROR_SURFACE_LOST_KHR) return lost();
    if (queried != VK_SUCCESS)
        vkFail("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", queried, __FILE__, __LINE__);

    VkExtent2D size = caps.currentExtent;
    if (size.width == UINT32_MAX) {
        // Wayland laisse l'application choisir la taille.
        size.width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
        size.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    if (size.width == 0 || size.height == 0) return false;

    // currentExtent est exprimé dans l'orientation courante. L'image physique
    // garde l'orientation native de l'écran, donc ses dimensions sont
    // échangées quand l'écran est tourné d'un quart de tour.
    VkSurfaceTransformFlagBitsKHR transform = caps.currentTransform;
    int newRotation = 0;
    switch (transform) {
    case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
        newRotation = 90;
        break;
    case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:
        newRotation = 180;
        break;
    case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
        newRotation = 270;
        break;
    default:
        if (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        break;
    }
    VkExtent2D physicalSize = size;
    if (newRotation == 90 || newRotation == 270)
        std::swap(physicalSize.width, physicalSize.height);

    // Format sRGB : le GPU encode la sortie linéaire du tone mapping.
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.physical, gpu.surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.physical, gpu.surface, &formatCount,
                                         formats.data());
    VkSurfaceFormatKHR chosen = formats[0];
    for (const auto& f : formats) {
        if (f.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) continue;
        if (f.format == VK_FORMAT_R8G8B8A8_SRGB || f.format == VK_FORMAT_B8G8R8A8_SRGB) {
            chosen = f;
            break;
        }
    }
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (!vsync) {
        uint32_t modeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.physical, gpu.surface, &modeCount,
                                                  nullptr);
        std::vector<VkPresentModeKHR> modes(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.physical, gpu.surface, &modeCount,
                                                  modes.data());
        for (VkPresentModeKHR m : modes)
            if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) presentMode = m;
        for (VkPresentModeKHR m : modes)
            if (m == VK_PRESENT_MODE_MAILBOX_KHR) presentMode = m;
    }

    VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (!(caps.supportedCompositeAlpha & alpha)) alpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    VkSwapchainKHR old = handle;
    VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface = gpu.surface;
    sci.minImageCount = imageCount;
    sci.imageFormat = chosen.format;
    sci.imageColorSpace = chosen.colorSpace;
    sci.imageExtent = physicalSize;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = transform;
    sci.compositeAlpha = alpha;
    sci.presentMode = presentMode;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = old;
    VkSwapchainKHR created = VK_NULL_HANDLE;
    VkResult result = vkCreateSwapchainKHR(gpu.device, &sci, nullptr, &created);
    if (result == VK_ERROR_SURFACE_LOST_KHR || result == VK_ERROR_NATIVE_WINDOW_IN_USE_KHR)
        return lost();
    if (result != VK_SUCCESS) vkFail("vkCreateSwapchainKHR", result, __FILE__, __LINE__);

    // L'ancienne chaîne sert à créer la nouvelle, puis disparaît avec ses vues.
    destroy(gpu);
    handle = created;
    format = chosen.format;
    extent = physicalSize;
    logicalExtent = size;
    rotation = newRotation;

    uint32_t count = 0;
    vkGetSwapchainImagesKHR(gpu.device, handle, &count, nullptr);
    images.resize(count);
    vkGetSwapchainImagesKHR(gpu.device, handle, &count, images.data());

    views.resize(count);
    renderDone.resize(count);
    for (uint32_t i = 0; i < count; i++) {
        VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image = images[i];
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = format;
        vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VK_CHECK(vkCreateImageView(gpu.device, &vci, nullptr, &views[i]));

        VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_CHECK(vkCreateSemaphore(gpu.device, &semInfo, nullptr, &renderDone[i]));
    }

    logInfo("Swapchain: %ux%u, rotation %d, %u images, format %d", extent.width, extent.height,
            rotation, count, static_cast<int>(format));
    return true;
}

void Swapchain::destroy(Gpu& gpu) {
    for (VkImageView view : views) vkDestroyImageView(gpu.device, view, nullptr);
    for (VkSemaphore sem : renderDone) vkDestroySemaphore(gpu.device, sem, nullptr);
    if (handle) vkDestroySwapchainKHR(gpu.device, handle, nullptr);
    *this = Swapchain{};
}

} // namespace gfx

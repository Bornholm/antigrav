#include "gfx/gpu.h"

#include "core/log.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <cstring>
#include <vector>

namespace gfx {

void vkFail(const char* expr, VkResult result, const char* file, int line) {
    fatal("Vulkan: %s a échoué avec le code %d (%s:%d)", expr, static_cast<int>(result), file,
          line);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void* user) {
    auto* gpu = static_cast<Gpu*>(user);
    // Un seul shader de sommets sert tous les matériaux, donc certaines de ses
    // sorties restent sans lecteur. C'est voulu.
    if (data->pMessageIdName && std::strstr(data->pMessageIdName, "OutputNotConsumed"))
        return VK_FALSE;
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        gpu->validationErrors++;
        logWarn("[validation] ERREUR %s", data->pMessage);
    } else {
        logWarn("[validation] %s: %s", data->pMessageIdName ? data->pMessageIdName : "?", data->pMessage);
    }
    return VK_FALSE;
}

static bool hasLayer(const char* name) {
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    for (const auto& layer : layers)
        if (std::strcmp(layer.layerName, name) == 0) return true;
    return false;
}

void Gpu::init(SDL_Window* window, bool wantValidation) {
    if (window) {
        // SDL a déjà chargé la bibliothèque Vulkan pour créer la fenêtre.
        auto getProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
            SDL_Vulkan_GetVkGetInstanceProcAddr());
        if (!getProc) fatal("SDL ne fournit pas vkGetInstanceProcAddr: %s", SDL_GetError());
        volkInitializeCustom(getProc);
    } else if (volkInitialize() != VK_SUCCESS) {
        fatal("Bibliothèque Vulkan introuvable");
    }

    const char* validationLayer = "VK_LAYER_KHRONOS_validation";
    validation = wantValidation && hasLayer(validationLayer);
    if (wantValidation && !validation)
        logWarn("Couches de validation Vulkan absentes, rendu non vérifié");

    std::vector<const char*> extensions;
    if (window) {
        uint32_t count = 0;
        const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&count);
        if (!sdlExts) fatal("SDL_Vulkan_GetInstanceExtensions: %s", SDL_GetError());
        extensions.assign(sdlExts, sdlExts + count);
    }
    if (validation) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "antigrav";
    app.pEngineName = "antigrav";
    app.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    ici.ppEnabledExtensionNames = extensions.data();
    if (validation) {
        ici.enabledLayerCount = 1;
        ici.ppEnabledLayerNames = &validationLayer;
    }
    VK_CHECK(vkCreateInstance(&ici, nullptr, &instance));
    volkLoadInstance(instance);

    if (validation) {
        VkDebugUtilsMessengerCreateInfoEXT dci{
            VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        dci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dci.pfnUserCallback = debugCallback;
        dci.pUserData = this;
        VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &dci, nullptr, &debugMessenger));
    }

    if (window) createSurface(window);

    // Premier périphérique en Vulkan 1.1 avec une file graphique capable de
    // présenter sur la surface.
    uint32_t physCount = 0;
    vkEnumeratePhysicalDevices(instance, &physCount, nullptr);
    std::vector<VkPhysicalDevice> physicals(physCount);
    vkEnumeratePhysicalDevices(instance, &physCount, physicals.data());

    VkPhysicalDeviceProperties tooOld{};
    for (VkPhysicalDevice candidate : physicals) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(candidate, &props);
        if (props.apiVersion < VK_API_VERSION_1_1) {
            tooOld = props;
            continue;
        }

        uint32_t famCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &famCount, nullptr);
        std::vector<VkQueueFamilyProperties> fams(famCount);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &famCount, fams.data());
        for (uint32_t i = 0; i < famCount; i++) {
            if (!(fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
            if (surface) {
                VkBool32 canPresent = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface, &canPresent);
                if (!canPresent) continue;
            }
            physical = candidate;
            queueFamily = i;
            properties = props;
            break;
        }
        if (physical) break;
    }
    if (!physical && tooOld.apiVersion)
        fatal("Ce jeu demande Vulkan 1.1. Le GPU de cet appareil, %s, s'arrête à Vulkan %u.%u.",
              tooOld.deviceName, VK_API_VERSION_MAJOR(tooOld.apiVersion),
              VK_API_VERSION_MINOR(tooOld.apiVersion));
    if (!physical) fatal("Aucun GPU compatible Vulkan 1.1 trouvé");
    logInfo("GPU: %s, Vulkan %u.%u.%u", properties.deviceName,
            VK_API_VERSION_MAJOR(properties.apiVersion),
            VK_API_VERSION_MINOR(properties.apiVersion),
            VK_API_VERSION_PATCH(properties.apiVersion));

    float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = queueFamily;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;

    std::vector<const char*> deviceExts;
    if (surface) deviceExts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = static_cast<uint32_t>(deviceExts.size());
    dci.ppEnabledExtensionNames = deviceExts.data();
    VK_CHECK(vkCreateDevice(physical, &dci, nullptr, &device));
    volkLoadDevice(device);
    vkGetDeviceQueue(device, queueFamily, 0, &queue);

    VmaVulkanFunctions vmaFuncs{};
    VmaAllocatorCreateInfo aci{};
    aci.physicalDevice = physical;
    aci.device = device;
    aci.instance = instance;
    aci.vulkanApiVersion = VK_API_VERSION_1_1;
    VK_CHECK(vmaImportVulkanFunctionsFromVolk(&aci, &vmaFuncs));
    aci.pVulkanFunctions = &vmaFuncs;
    VK_CHECK(vmaCreateAllocator(&aci, &allocator));
}

void Gpu::shutdown() {
    if (allocator) vmaDestroyAllocator(allocator);
    if (device) vkDestroyDevice(device, nullptr);
    destroySurface();
    if (debugMessenger) vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    if (instance) vkDestroyInstance(instance, nullptr);
    *this = Gpu{};
}

void Gpu::createSurface(SDL_Window* window) {
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface))
        fatal("SDL_Vulkan_CreateSurface: %s", SDL_GetError());
}

void Gpu::destroySurface() {
    if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
    surface = VK_NULL_HANDLE;
}

VkImageAspectFlags aspectOf(VkFormat format) {
    switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

Image Gpu::createImage(const ImageDesc& desc) {
    Image out;
    out.format = desc.format;
    out.extent = desc.extent;
    out.samples = desc.samples;

    VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = desc.format;
    ici.extent = {desc.extent.width, desc.extent.height, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = desc.samples;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = desc.usage;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO;
    if (desc.transient) {
        ici.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
        aci.preferredFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    }
    VK_CHECK(vmaCreateImage(allocator, &ici, &aci, &out.image, &out.alloc, nullptr));

    VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vci.image = out.image;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = desc.format;
    vci.subresourceRange = {aspectOf(desc.format), 0, 1, 0, 1};
    VK_CHECK(vkCreateImageView(device, &vci, nullptr, &out.view));

    if (desc.name) {
        setName(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(out.image), desc.name);
        setName(VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(out.view), desc.name);
    }
    return out;
}

void Gpu::destroyImage(Image& image) {
    if (image.view) vkDestroyImageView(device, image.view, nullptr);
    if (image.image) vmaDestroyImage(allocator, image.image, image.alloc);
    image = Image{};
}

Image Gpu::createTexture(VkExtent2D extent, VkFormat format, const void* pixels,
                         VkDeviceSize sizeBytes, const char* name) {
    ImageDesc desc;
    desc.extent = extent;
    desc.format = format;
    desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    desc.name = name;
    Image image = createImage(desc);

    Buffer staging = createBuffer(sizeBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, "staging");
    std::memcpy(staging.mapped, pixels, sizeBytes);
    vmaFlushAllocation(allocator, staging.alloc, 0, VK_WHOLE_SIZE);

    // Copie ponctuelle au chargement : une commande à usage unique suffit.
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pci.queueFamilyIndex = queueFamily;
    VkCommandPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(device, &pci, nullptr, &pool));
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(device, &ai, &cmd));
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &bi));

    transition(cmd, image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {extent.width, extent.height, 1};
    vkCmdCopyBufferToImage(cmd, staging.buffer, image.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    transition(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VK_CHECK(vkEndCommandBuffer(cmd));
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    VK_CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(queue));

    vkDestroyCommandPool(device, pool, nullptr);
    destroyBuffer(staging);
    return image;
}

Buffer Gpu::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, bool hostVisible,
                         const char* name) {
    Buffer out;
    out.size = size;

    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO;
    if (hostVisible) {
        // Mappé une fois pour toute la durée de vie du tampon.
        aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                    VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }
    VmaAllocationInfo info{};
    VK_CHECK(vmaCreateBuffer(allocator, &bci, &aci, &out.buffer, &out.alloc, &info));
    out.mapped = info.pMappedData;
    if (name) setName(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(out.buffer), name);
    return out;
}

void Gpu::destroyBuffer(Buffer& buffer) {
    if (buffer.buffer) vmaDestroyBuffer(allocator, buffer.buffer, buffer.alloc);
    buffer = Buffer{};
}

VkShaderModule Gpu::createShader(const uint32_t* code, size_t sizeBytes) {
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = sizeBytes;
    ci.pCode = code;
    VkShaderModule module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device, &ci, nullptr, &module));
    return module;
}

void Gpu::setName(VkObjectType type, uint64_t handle, const char* name) {
    if (!validation) return;
    VkDebugUtilsObjectNameInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    info.objectType = type;
    info.objectHandle = handle;
    info.pObjectName = name;
    vkSetDebugUtilsObjectNameEXT(device, &info);
}

namespace {

struct LayoutInfo {
    VkPipelineStageFlags stage;
    VkAccessFlags access;
};

LayoutInfo layoutInfo(VkImageLayout layout) {
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        return {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0};
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT};
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT};
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT};
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT};
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT};
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        return {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0};
    default:
        fatal("Disposition d'image non gérée: %d", static_cast<int>(layout));
    }
}

} // namespace

void transition(VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to,
                VkImageAspectFlags aspect) {
    LayoutInfo src = layoutInfo(from);
    LayoutInfo dst = layoutInfo(to);

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = src.access;
    barrier.dstAccessMask = dst.access;
    barrier.oldLayout = from;
    barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {aspect, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmd, src.stage, dst.stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// Premier format de la liste que le GPU accepte pour l'usage demandé.
VkFormat Gpu::pickFormat(std::initializer_list<VkFormat> candidates,
                         VkFormatFeatureFlags features) const {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physical, format, &props);
        if ((props.optimalTilingFeatures & features) == features) return format;
    }
    fatal("Ce GPU ne propose aucun des formats d'image dont le jeu a besoin");
}

} // namespace gfx

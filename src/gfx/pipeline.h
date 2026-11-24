#pragma once

#include "gfx/gpu.h"

#include <span>
#include <vector>

namespace gfx {

enum class Blend {
    Opaque,
    Alpha,    // src * a + dst * (1 - a)
    Additive, // src + dst, pour tout ce qui émet de la lumière
};

struct PipelineDesc {
    std::span<const uint32_t> vert;
    std::span<const uint32_t> frag;
    VkPipelineLayout layout = VK_NULL_HANDLE;

    uint32_t vertexStride = 0;
    std::vector<VkVertexInputAttributeDescription> attributes;

    std::vector<VkFormat> colorFormats;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

    Blend blend = Blend::Opaque;
    bool depthTest = false;
    bool depthWrite = false;
    VkCullModeFlags cull = VK_CULL_MODE_NONE;
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    const char* name = nullptr;
};

// Viewport et scissor sont dynamiques, donc un pipeline survit aux
// changements de taille de fenêtre.
VkPipeline createPipeline(Gpu& gpu, const PipelineDesc& desc);

void setViewport(VkCommandBuffer cmd, VkExtent2D extent);

} // namespace gfx

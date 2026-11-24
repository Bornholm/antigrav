#pragma once

#include "gfx/gpu.h"

#include <span>
#include <vector>

namespace gfx {

enum class Blend {
    Opaque,
    Alpha,    // src * a + dst * (1 - a)
    Additive, // src + dst, pour tout ce qui émet de la lumière
    Premultiplied, // src + dst * (1 - a), pour l'interface
};

struct PipelineDesc {
    std::span<const uint32_t> vert;
    std::span<const uint32_t> frag;
    VkPipelineLayout layout = VK_NULL_HANDLE;

    uint32_t vertexStride = 0;
    std::vector<VkVertexInputAttributeDescription> attributes;

    // Passe dans laquelle le pipeline dessine, ou toute passe compatible.
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

    Blend blend = Blend::Opaque;
    bool depthTest = false;
    bool depthWrite = false;
    VkCullModeFlags cull = VK_CULL_MODE_NONE;
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    const char* name = nullptr;
};

// Passe de rendu à une sous-passe : une couleur, une profondeur facultative et,
// en MSAA, la résolution vers une image simple. Les dispositions d'entrée et de
// sortie sont celles de l'image que les passes suivantes liront, la couleur ou
// sa résolution : la passe fait elle-même les transitions.
struct PassDesc {
    VkFormat color = VK_FORMAT_UNDEFINED;
    VkFormat depth = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    const char* name = nullptr;
};
VkRenderPass createRenderPass(Gpu& gpu, const PassDesc& desc);
// Vues dans l'ordre des attachements : couleur, profondeur, résolution.
VkFramebuffer createFramebuffer(Gpu& gpu, VkRenderPass pass, std::span<const VkImageView> views,
                                VkExtent2D extent);

// Viewport et scissor sont dynamiques, donc un pipeline survit aux
// changements de taille de fenêtre.
VkPipeline createPipeline(Gpu& gpu, const PipelineDesc& desc);

void setViewport(VkCommandBuffer cmd, VkExtent2D extent);

} // namespace gfx

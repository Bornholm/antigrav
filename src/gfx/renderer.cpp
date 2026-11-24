#include "gfx/renderer.h"

#include "gfx/pipeline.h"

namespace gfx {

namespace {

const uint32_t kFullscreenVert[] =
#include "fullscreen_vert.inc"
    ;
const uint32_t kBootFrag[] =
#include "boot_frag.inc"
    ;

struct FullscreenPush {
    float rotation[4];
    float params[4];
};

// Matrice qui fait passer des coordonnées de l'image physique à celles du jeu.
// À confirmer sur le téléphone : le sens dépend de la convention du pilote.
void rotationMatrix(int degrees, float out[4]) {
    switch (degrees) {
    case 90:
        out[0] = 0.0f, out[1] = -1.0f, out[2] = 1.0f, out[3] = 0.0f;
        break;
    case 180:
        out[0] = -1.0f, out[1] = 0.0f, out[2] = 0.0f, out[3] = -1.0f;
        break;
    case 270:
        out[0] = 0.0f, out[1] = 1.0f, out[2] = -1.0f, out[3] = 0.0f;
        break;
    default:
        out[0] = 1.0f, out[1] = 0.0f, out[2] = 0.0f, out[3] = 1.0f;
        break;
    }
}

} // namespace

void Renderer::init(Gpu& gpuRef, VkFormat outputFormat) {
    gpu = &gpuRef;

    VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                              sizeof(FullscreenPush)};
    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges = &range;
    VK_CHECK(vkCreatePipelineLayout(gpu->device, &lci, nullptr, &bootLayout));

    PipelineDesc desc;
    desc.vert = kFullscreenVert;
    desc.frag = kBootFrag;
    desc.layout = bootLayout;
    desc.colorFormats = {outputFormat};
    desc.name = "boot";
    bootPipeline = createPipeline(*gpu, desc);
}

void Renderer::shutdown() {
    vkDestroyPipeline(gpu->device, bootPipeline, nullptr);
    vkDestroyPipelineLayout(gpu->device, bootLayout, nullptr);
    *this = Renderer{};
}

void Renderer::draw(VkCommandBuffer cmd, const FrameTarget& target, const FrameParams& params) {
    VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    color.imageView = target.view;
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // Chaque pixel est réécrit, donc rien à charger depuis la mémoire.
    color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo info{VK_STRUCTURE_TYPE_RENDERING_INFO};
    info.renderArea = {{0, 0}, target.extent};
    info.layerCount = 1;
    info.colorAttachmentCount = 1;
    info.pColorAttachments = &color;

    vkCmdBeginRendering(cmd, &info);
    setViewport(cmd, target.extent);

    FullscreenPush push{};
    rotationMatrix(target.rotation, push.rotation);
    push.params[0] = params.time;
    push.params[1] = static_cast<float>(target.logicalExtent.width) /
                     static_cast<float>(target.logicalExtent.height);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bootPipeline);
    vkCmdPushConstants(cmd, bootLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(push), &push);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRendering(cmd);
}

} // namespace gfx

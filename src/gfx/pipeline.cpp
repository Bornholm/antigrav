#include "gfx/pipeline.h"

namespace gfx {

VkPipeline createPipeline(Gpu& gpu, const PipelineDesc& desc) {
    VkShaderModule vert = gpu.createShader(desc.vert.data(), desc.vert.size_bytes());
    VkShaderModule frag = gpu.createShader(desc.frag.data(), desc.frag.size_bytes());

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding{0, desc.vertexStride, VK_VERTEX_INPUT_RATE_VERTEX};
    VkPipelineVertexInputStateCreateInfo vertexInput{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    if (desc.vertexStride > 0) {
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount =
            static_cast<uint32_t>(desc.attributes.size());
        vertexInput.pVertexAttributeDescriptions = desc.attributes.data();
    }

    VkPipelineInputAssemblyStateCreateInfo assembly{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = desc.topology;

    VkPipelineViewportStateCreateInfo viewport{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = desc.cull;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = desc.samples;

    VkPipelineDepthStencilStateCreateInfo depth{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth.depthTestEnable = desc.depthTest;
    depth.depthWriteEnable = desc.depthWrite;
    // Profondeur inversée : 1 au plan proche, 0 à l'infini.
    depth.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (desc.blend != Blend::Opaque) {
        blendAttachment.blendEnable = VK_TRUE;
        blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        if (desc.blend == Blend::Alpha) {
            blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        } else {
            blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        }
    }
    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(desc.colorFormats.size(),
                                                                      blendAttachment);
    VkPipelineColorBlendStateCreateInfo blend{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = static_cast<uint32_t>(blendAttachments.size());
    blend.pAttachments = blendAttachments.data();

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamicStates;

    VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rendering.colorAttachmentCount = static_cast<uint32_t>(desc.colorFormats.size());
    rendering.pColorAttachmentFormats = desc.colorFormats.data();
    rendering.depthAttachmentFormat = desc.depthFormat;

    VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    ci.pNext = &rendering;
    ci.stageCount = 2;
    ci.pStages = stages;
    ci.pVertexInputState = &vertexInput;
    ci.pInputAssemblyState = &assembly;
    ci.pViewportState = &viewport;
    ci.pRasterizationState = &raster;
    ci.pMultisampleState = &multisample;
    ci.pDepthStencilState = &depth;
    ci.pColorBlendState = &blend;
    ci.pDynamicState = &dynamic;
    ci.layout = desc.layout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(gpu.device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline));
    vkDestroyShaderModule(gpu.device, vert, nullptr);
    vkDestroyShaderModule(gpu.device, frag, nullptr);
    if (desc.name)
        gpu.setName(VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<uint64_t>(pipeline), desc.name);
    return pipeline;
}

void setViewport(VkCommandBuffer cmd, VkExtent2D extent) {
    VkViewport viewport{0.0f, 0.0f, static_cast<float>(extent.width),
                        static_cast<float>(extent.height), 0.0f, 1.0f};
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

} // namespace gfx

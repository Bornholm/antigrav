#include "gfx/renderer.h"

#include "core/log.h"

#include "gfx/geometry.h"
#include "gfx/pipeline.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace gfx {

namespace {

const uint32_t kWorldVert[] =
#include "world_vert.inc"
    ;
const uint32_t kSkyVert[] =
#include "sky_vert.inc"
    ;
const uint32_t kFullscreenVert[] =
#include "fullscreen_vert.inc"
    ;
const uint32_t kRoadFrag[] =
#include "road_frag.inc"
    ;
const uint32_t kNeonFrag[] =
#include "neon_frag.inc"
    ;
const uint32_t kBuildingFrag[] =
#include "building_frag.inc"
    ;
const uint32_t kShipFrag[] =
#include "ship_frag.inc"
    ;
const uint32_t kSkyFrag[] =
#include "sky_frag.inc"
    ;
const uint32_t kTrailFrag[] =
#include "trail_frag.inc"
    ;
const uint32_t kBloomDownFrag[] =
#include "bloom_down_frag.inc"
    ;
const uint32_t kBloomUpFrag[] =
#include "bloom_up_frag.inc"
    ;
const uint32_t kCompositeFrag[] =
#include "composite_frag.inc"
    ;
const uint32_t kHudVert[] =
#include "hud_vert.inc"
    ;
const uint32_t kHudFrag[] =
#include "hud_frag.inc"
    ;

const VkSampleCountFlagBits kSamples = VK_SAMPLE_COUNT_4_BIT;
constexpr float kEqualizerStretch = 0.55f;

// Miroir de shaders/common.glsl, disposition std140.
struct Globals {
    glm::mat4 viewProj;
    glm::vec4 camPos;
    glm::vec4 camRight;
    glm::vec4 camUp;
    glm::vec4 camForward;
    glm::vec4 fog;
    glm::vec4 screen;
    glm::vec4 neonA;
    glm::vec4 neonB;
    glm::vec4 skyZenith;
    glm::vec4 skyMid;
    glm::vec4 skyHaze;
    glm::vec4 bandsLow;
    glm::vec4 bandsHigh;
};

struct ScenePush {
    glm::mat4 model{1.0f};
    glm::vec4 colorA{1.0f};
    glm::vec4 colorB{1.0f};
};

struct PostPush {
    float rotation[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    float params[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

const VkShaderStageFlags kPushStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

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

// Symétrie par rapport au plan passant par point, de normale normal.
glm::mat4 mirrorMatrix(const glm::vec3& point, const glm::vec3& normal) {
    glm::mat4 m{1.0f};
    for (int c = 0; c < 3; c++)
        for (int r = 0; r < 3; r++) m[c][r] = (c == r ? 1.0f : 0.0f) - 2.0f * normal[r] * normal[c];
    m[3] = glm::vec4(2.0f * glm::dot(point, normal) * normal, 1.0f);
    return m;
}

glm::vec3 mirrorVector(const glm::vec3& v, const glm::vec3& normal) {
    return v - 2.0f * glm::dot(v, normal) * normal;
}

// Projection à profondeur inversée et plan lointain à l'infini : 1 au plan
// proche, 0 à l'infini. La précision reste bonne jusqu'à la ville à l'horizon.
glm::mat4 projection(float fovY, float aspect, float nearPlane) {
    float f = 1.0f / std::tan(fovY * 0.5f);
    glm::mat4 p{0.0f};
    p[0][0] = f / aspect;
    p[1][1] = -f;
    p[2][3] = -1.0f;
    p[3][2] = nearPlane;
    return p;
}

void bindMesh(VkCommandBuffer cmd, const Mesh& mesh) {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertices.buffer, &offset);
    vkCmdBindIndexBuffer(cmd, mesh.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
}

void pushScene(VkCommandBuffer cmd, VkPipelineLayout layout, const ScenePush& push) {
    vkCmdPushConstants(cmd, layout, kPushStages, 0, sizeof(push), &push);
}

void beginPass(VkCommandBuffer cmd, VkRenderPass pass, VkFramebuffer framebuffer,
               VkExtent2D extent) {
    // Une valeur par attachement possible : couleur, profondeur, résolution.
    // Profondeur inversée, donc vidée à zéro.
    VkClearValue clears[3]{};
    VkRenderPassBeginInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    info.renderPass = pass;
    info.framebuffer = framebuffer;
    info.renderArea = {{0, 0}, extent};
    info.clearValueCount = 3;
    info.pClearValues = clears;
    vkCmdBeginRenderPass(cmd, &info, VK_SUBPASS_CONTENTS_INLINE);
    setViewport(cmd, extent);
}

} // namespace

glm::mat4 viewProjection(const FrameParams& params, float aspect) {
    glm::vec3 forward = glm::normalize(params.camTarget - params.camPos);
    glm::vec3 right = glm::normalize(glm::cross(forward, params.camUp));
    glm::vec3 up = glm::cross(right, forward);
    return projection(params.fovY, aspect, 0.3f) *
           glm::lookAt(params.camPos, params.camPos + forward, up);
}

void Renderer::init(Gpu& gpuRef, VkFormat output) {
    gpu = &gpuRef;
    outputFormat = output;
    VkDevice dev = gpu->device;

    VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VK_CHECK(vkCreateSampler(dev, &sci, nullptr, &sampler));

    // Flottants 11 et 10 bits sans alpha : moitié moins de mémoire qu'un
    // RGBA16F, ce qui compte sur un GPU limité par sa bande passante. Les
    // seconds choix couvrent les pilotes Vulkan 1.1 les moins complets.
    hdrFormat = gpu->pickFormat(
        {VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_R16G16B16A16_SFLOAT},
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
    depthFormat = gpu->pickFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D16_UNORM},
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    PassDesc pass;
    pass.color = hdrFormat;
    pass.depth = depthFormat;
    pass.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    pass.name = "reflection";
    reflectionPass = createRenderPass(*gpu, pass);
    pass.samples = kSamples;
    pass.name = "scene";
    scenePass = createRenderPass(*gpu, pass);

    pass = PassDesc{};
    pass.color = hdrFormat;
    pass.name = "bloom down";
    bloomDownPass = createRenderPass(*gpu, pass);
    pass.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    pass.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    pass.name = "bloom up";
    bloomUpPass = createRenderPass(*gpu, pass);

    // L'appelant donne l'image de sortie prête à être écrite et la reprend
    // dans le même état, pour la présenter ou la relire.
    pass = PassDesc{};
    pass.color = outputFormat;
    pass.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    pass.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    pass.name = "output";
    outputPass = createRenderPass(*gpu, pass);

    // Jeu 0 de la scène : les données globales de la vue, avec un décalage
    // dynamique pour passer de la vue normale à la vue en miroir.
    VkDescriptorSetLayoutBinding globalsBinding{};
    globalsBinding.binding = 0;
    globalsBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    globalsBinding.descriptorCount = 1;
    globalsBinding.stageFlags = kPushStages;
    VkDescriptorSetLayoutCreateInfo glci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    glci.bindingCount = 1;
    glci.pBindings = &globalsBinding;
    VK_CHECK(vkCreateDescriptorSetLayout(dev, &glci, nullptr, &globalsLayout));

    VkDescriptorSetLayoutBinding texBindings[2]{};
    for (uint32_t i = 0; i < 2; i++) {
        texBindings[i].binding = i;
        texBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        texBindings[i].descriptorCount = 1;
        texBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo tlci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    tlci.bindingCount = 2;
    tlci.pBindings = texBindings;
    VK_CHECK(vkCreateDescriptorSetLayout(dev, &tlci, nullptr, &texturesLayout));

    {
        VkDescriptorSetLayout sets[2] = {globalsLayout, texturesLayout};
        VkPushConstantRange range{kPushStages, 0, sizeof(ScenePush)};
        VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        lci.setLayoutCount = 2;
        lci.pSetLayouts = sets;
        lci.pushConstantRangeCount = 1;
        lci.pPushConstantRanges = &range;
        VK_CHECK(vkCreatePipelineLayout(dev, &lci, nullptr, &sceneLayout));
    }
    {
        VkPushConstantRange range{kPushStages, 0, sizeof(PostPush)};
        VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        lci.setLayoutCount = 1;
        lci.pSetLayouts = &texturesLayout;
        lci.pushConstantRangeCount = 1;
        lci.pPushConstantRanges = &range;
        VK_CHECK(vkCreatePipelineLayout(dev, &lci, nullptr, &postLayout));
    }

    {
        VkDescriptorPoolSize sizes[2] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, kFramesInFlight},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2},
        };
        VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pci.maxSets = kFramesInFlight + 1;
        pci.poolSizeCount = 2;
        pci.pPoolSizes = sizes;
        VK_CHECK(vkCreateDescriptorPool(dev, &pci, nullptr, &staticPool));
    }
    {
        const uint32_t sets = kBloomLevels + 3;
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sets * 2};
        VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pci.maxSets = sets;
        pci.poolSizeCount = 1;
        pci.pPoolSizes = &size;
        VK_CHECK(vkCreateDescriptorPool(dev, &pci, nullptr, &targetPool));
    }

    // Deux vues par image, alignées sur la contrainte du pilote.
    VkDeviceSize align = gpu->properties.limits.minUniformBufferOffsetAlignment;
    globalsStride = static_cast<uint32_t>((sizeof(Globals) + align - 1) / align * align);
    for (int i = 0; i < kFramesInFlight; i++) {
        globalsBuffer[i] = gpu->createBuffer(globalsStride * 2,
                                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true, "globals");
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = staticPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &globalsLayout;
        VK_CHECK(vkAllocateDescriptorSets(dev, &ai, &globalsSet[i]));

        VkDescriptorBufferInfo info{globalsBuffer[i].buffer, 0, sizeof(Globals)};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = globalsSet[i];
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        write.pBufferInfo = &info;
        vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
    }

    // Pipelines. Tous créés ici : aucune compilation pendant une course.
    std::vector<VkVertexInputAttributeDescription> attributes = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)},
        {1, 0, VK_FORMAT_R32_SFLOAT, offsetof(Vertex, height)},
        {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
        {3, 0, VK_FORMAT_R32_SFLOAT, offsetof(Vertex, kind)},
        {4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)},
        {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, color)},
    };

    struct MaterialDef {
        Material material;
        std::span<const uint32_t> vert;
        std::span<const uint32_t> frag;
        bool mesh;
        bool depthWrite;
        Blend blend;
        const char* name;
    };
    const MaterialDef defs[] = {
        {Material::Road, kWorldVert, kRoadFrag, true, true, Blend::Opaque, "road"},
        {Material::Neon, kWorldVert, kNeonFrag, true, true, Blend::Opaque, "neon"},
        {Material::Building, kWorldVert, kBuildingFrag, true, true, Blend::Opaque, "building"},
        {Material::Ship, kWorldVert, kShipFrag, true, true, Blend::Opaque, "ship"},
        {Material::Sky, kSkyVert, kSkyFrag, false, false, Blend::Opaque, "sky"},
        {Material::Trail, kWorldVert, kTrailFrag, true, false, Blend::Additive, "trail"},
    };
    for (const MaterialDef& def : defs) {
        PipelineDesc desc;
        desc.vert = def.vert;
        desc.frag = def.frag;
        desc.layout = sceneLayout;
        if (def.mesh) {
            desc.vertexStride = sizeof(Vertex);
            desc.attributes = attributes;
        }
        desc.depthTest = true;
        desc.depthWrite = def.depthWrite;
        desc.blend = def.blend;
        desc.name = def.name;

        desc.samples = kSamples;
        desc.renderPass = scenePass;
        scenePipelines[static_cast<int>(def.material)] = createPipeline(*gpu, desc);
        desc.samples = VK_SAMPLE_COUNT_1_BIT;
        desc.renderPass = reflectionPass;
        reflectionPipelines[static_cast<int>(def.material)] = createPipeline(*gpu, desc);
    }

    PipelineDesc post;
    post.vert = kFullscreenVert;
    post.layout = postLayout;
    post.renderPass = bloomDownPass;
    post.frag = kBloomDownFrag;
    post.name = "bloom down";
    bloomDownPipeline = createPipeline(*gpu, post);
    post.frag = kBloomUpFrag;
    post.blend = Blend::Additive;
    post.renderPass = bloomUpPass;
    post.name = "bloom up";
    bloomUpPipeline = createPipeline(*gpu, post);
    post.frag = kCompositeFrag;
    post.blend = Blend::Opaque;
    post.renderPass = outputPass;
    post.name = "composite";
    compositePipeline = createPipeline(*gpu, post);

    // Interface : atlas de police, pipeline et un tampon de sommets par frame.
    fonts.bake();
    fontTexture = gpu->createTexture({static_cast<uint32_t>(fonts.width),
                                      static_cast<uint32_t>(fonts.height)},
                                     VK_FORMAT_R8_UNORM, fonts.pixels.data(),
                                     fonts.pixels.size(), "font atlas");
    {
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = staticPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &texturesLayout;
        VK_CHECK(vkAllocateDescriptorSets(dev, &ai, &fontSet));
        VkDescriptorImageInfo info{sampler, fontTexture.view,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet writes[2]{};
        for (uint32_t i = 0; i < 2; i++) {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = fontSet;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[i].pImageInfo = &info;
        }
        vkUpdateDescriptorSets(dev, 2, writes, 0, nullptr);
    }
    PipelineDesc hud;
    hud.vert = kHudVert;
    hud.frag = kHudFrag;
    hud.layout = postLayout;
    hud.vertexStride = sizeof(HudVertex);
    hud.attributes = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(HudVertex, pos)},
        {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(HudVertex, uv)},
        {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(HudVertex, color)},
        {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(HudVertex, color2)},
        {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(HudVertex, params)},
        {5, 0, VK_FORMAT_R32_SFLOAT, offsetof(HudVertex, mode)},
    };
    hud.renderPass = outputPass;
    hud.blend = Blend::Premultiplied;
    hud.name = "hud";
    hudPipeline = createPipeline(*gpu, hud);
    for (int i = 0; i < kFramesInFlight; i++)
        hudBuffer[i] = gpu->createBuffer(kMaxHudVertices * sizeof(HudVertex),
                                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true, "hud");

    // Les horodatages ne sont pas garantis sur tous les pilotes.
    uint32_t famCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu->physical, &famCount, nullptr);
    std::vector<VkQueueFamilyProperties> fams(famCount);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu->physical, &famCount, fams.data());
    if (fams[gpu->queueFamily].timestampValidBits > 0) {
        VkQueryPoolCreateInfo qci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        qci.queryType = VK_QUERY_TYPE_TIMESTAMP;
        qci.queryCount = kTimestamps * kFramesInFlight;
        VK_CHECK(vkCreateQueryPool(dev, &qci, nullptr, &queryPool));
    }

    for (int i = 0; i < sim::kShipCount; i++) {
        ships[i] = uploadMesh(*gpu, buildShip(i), "ship");
        trails[i] = uploadMesh(*gpu, buildTrail(i), "trail");
    }
    shield = uploadMesh(*gpu, buildShield(), "shield");
    missile = uploadMesh(*gpu, buildMissile(), "missile");
}

void Renderer::loadTrack(const sim::Track& track, int themeId, int serial) {
    VK_CHECK(vkDeviceWaitIdle(gpu->device));
    destroyMesh(*gpu, road);
    destroyMesh(*gpu, rails);
    destroyMesh(*gpu, gates);
    destroyMesh(*gpu, features);
    destroyMesh(*gpu, city);
    theme = themeFor(themeId);
    loadedTrack = serial;
    trackLead = track.lead;
    auto upload = [&](MeshData data, const char* name) {
        std::vector<uint32_t> chunks = sortByChunk(data, track);
        Mesh mesh = uploadMesh(*gpu, data, name);
        mesh.chunks = std::move(chunks);
        return mesh;
    };
    road = upload(buildRoad(track), "road");
    rails = upload(buildRails(track, theme), "rails");
    gates = upload(buildGates(track, theme), "gates");
    features = upload(buildFeatures(track), "features");
    city = upload(buildScenery(track, theme), "scenery");
    logInfo("Piste chargée : %u triangles, dont %u de décor", (road.indexCount + rails.indexCount + gates.indexCount +
            features.indexCount + city.indexCount) / 3, city.indexCount / 3);
}

void Renderer::shutdown() {
    VkDevice dev = gpu->device;
    destroyTargets();
    releaseOutputs();
    destroyMesh(*gpu, road);
    destroyMesh(*gpu, rails);
    destroyMesh(*gpu, gates);
    destroyMesh(*gpu, features);
    destroyMesh(*gpu, city);
    for (int i = 0; i < sim::kShipCount; i++) {
        destroyMesh(*gpu, ships[i]);
        destroyMesh(*gpu, trails[i]);
    }
    destroyMesh(*gpu, shield);
    destroyMesh(*gpu, missile);
    for (int i = 0; i < static_cast<int>(Material::Count); i++) {
        vkDestroyPipeline(dev, scenePipelines[i], nullptr);
        vkDestroyPipeline(dev, reflectionPipelines[i], nullptr);
    }
    vkDestroyPipeline(dev, bloomDownPipeline, nullptr);
    vkDestroyPipeline(dev, bloomUpPipeline, nullptr);
    vkDestroyPipeline(dev, compositePipeline, nullptr);
    vkDestroyPipeline(dev, hudPipeline, nullptr);
    for (VkRenderPass pass : {reflectionPass, scenePass, bloomDownPass, bloomUpPass, outputPass})
        vkDestroyRenderPass(dev, pass, nullptr);
    if (queryPool) vkDestroyQueryPool(dev, queryPool, nullptr);
    gpu->destroyImage(fontTexture);
    for (int i = 0; i < kFramesInFlight; i++) gpu->destroyBuffer(hudBuffer[i]);
    for (int i = 0; i < kFramesInFlight; i++) gpu->destroyBuffer(globalsBuffer[i]);
    vkDestroyDescriptorPool(dev, staticPool, nullptr);
    vkDestroyDescriptorPool(dev, targetPool, nullptr);
    vkDestroyPipelineLayout(dev, sceneLayout, nullptr);
    vkDestroyPipelineLayout(dev, postLayout, nullptr);
    vkDestroyDescriptorSetLayout(dev, globalsLayout, nullptr);
    vkDestroyDescriptorSetLayout(dev, texturesLayout, nullptr);
    vkDestroySampler(dev, sampler, nullptr);
    *this = Renderer{};
}

void Renderer::releaseOutputs() {
    for (const OutputFramebuffer& o : outputs)
        vkDestroyFramebuffer(gpu->device, o.framebuffer, nullptr);
    outputs.clear();
}

VkFramebuffer Renderer::outputFramebuffer(const FrameTarget& target) {
    for (const OutputFramebuffer& o : outputs)
        if (o.view == target.view && o.extent.width == target.extent.width &&
            o.extent.height == target.extent.height)
            return o.framebuffer;
    VkFramebuffer created = createFramebuffer(*gpu, outputPass, {&target.view, 1}, target.extent);
    outputs.push_back({target.view, target.extent, created});
    return created;
}

void Renderer::destroyTargets() {
    VkDevice dev = gpu->device;
    vkDestroyFramebuffer(dev, reflectionFramebuffer, nullptr);
    vkDestroyFramebuffer(dev, sceneFramebuffer, nullptr);
    for (VkFramebuffer& fb : bloomFramebuffer) {
        vkDestroyFramebuffer(dev, fb, nullptr);
        fb = VK_NULL_HANDLE;
    }
    reflectionFramebuffer = sceneFramebuffer = VK_NULL_HANDLE;
    gpu->destroyImage(reflectionColor);
    gpu->destroyImage(reflectionDepth);
    gpu->destroyImage(msaaColor);
    gpu->destroyImage(msaaDepth);
    gpu->destroyImage(sceneColor);
    for (Image& level : bloom) gpu->destroyImage(level);
}

void Renderer::createTargets(VkExtent2D extent) {
    destroyTargets();
    sceneExtent = extent;
    auto halved = [](VkExtent2D e, int shift) {
        return VkExtent2D{std::max(1u, e.width >> shift), std::max(1u, e.height >> shift)};
    };
    const VkImageUsageFlags sampled =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    ImageDesc desc;
    desc.extent = halved(extent, 1);
    desc.format = hdrFormat;
    desc.usage = sampled;
    desc.name = "reflection color";
    reflectionColor = gpu->createImage(desc);

    desc.format = depthFormat;
    desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    desc.transient = true;
    desc.name = "reflection depth";
    reflectionDepth = gpu->createImage(desc);

    desc.extent = extent;
    desc.samples = kSamples;
    desc.name = "msaa depth";
    msaaDepth = gpu->createImage(desc);

    desc.format = hdrFormat;
    desc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    desc.name = "msaa color";
    msaaColor = gpu->createImage(desc);

    desc.samples = VK_SAMPLE_COUNT_1_BIT;
    desc.transient = false;
    desc.usage = sampled;
    desc.name = "scene color";
    sceneColor = gpu->createImage(desc);

    for (int i = 0; i < kBloomLevels; i++) {
        desc.extent = halved(extent, i + 1);
        desc.name = "bloom";
        bloom[i] = gpu->createImage(desc);
    }

    const VkImageView reflectionViews[] = {reflectionColor.view, reflectionDepth.view};
    reflectionFramebuffer =
        createFramebuffer(*gpu, reflectionPass, reflectionViews, reflectionColor.extent);
    const VkImageView sceneViews[] = {msaaColor.view, msaaDepth.view, sceneColor.view};
    sceneFramebuffer = createFramebuffer(*gpu, scenePass, sceneViews, extent);
    for (int i = 0; i < kBloomLevels; i++)
        bloomFramebuffer[i] =
            createFramebuffer(*gpu, bloomDownPass, {&bloom[i].view, 1}, bloom[i].extent);

    // Les jeux de descripteurs pointent sur les images, donc ils suivent.
    VkDevice dev = gpu->device;
    VK_CHECK(vkResetDescriptorPool(dev, targetPool, 0));
    auto makeSet = [&](VkImageView a, VkImageView b) {
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = targetPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &texturesLayout;
        VkDescriptorSet set = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateDescriptorSets(dev, &ai, &set));

        VkDescriptorImageInfo infos[2] = {
            {sampler, a, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {sampler, b, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        };
        VkWriteDescriptorSet writes[2]{};
        for (uint32_t i = 0; i < 2; i++) {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = set;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[i].pImageInfo = &infos[i];
        }
        vkUpdateDescriptorSets(dev, 2, writes, 0, nullptr);
        return set;
    };
    reflectionSet = makeSet(reflectionColor.view, reflectionColor.view);
    sceneSet = makeSet(sceneColor.view, sceneColor.view);
    for (int i = 0; i < kBloomLevels; i++) bloomSet[i] = makeSet(bloom[i].view, bloom[i].view);
    compositeSet = makeSet(sceneColor.view, bloom[0].view);
}

void Renderer::drawWorld(VkCommandBuffer cmd, const FrameParams& params, bool reflection) {
    const VkPipeline* pipelines = reflection ? reflectionPipelines : scenePipelines;
    auto bind = [&](Material m) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[static_cast<int>(m)]);
    };

    uint32_t offset = reflection ? globalsStride : 0;
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sceneLayout, 0, 1,
                            &globalsSet[params.slot], 1, &offset);

    ScenePush identity;
    pushScene(cmd, sceneLayout, identity);

    // Tronçons de piste visibles : un peu derrière la caméra, loin devant.
    auto drawNear = [&](const Mesh& mesh, float behind, float ahead) {
        const int count = static_cast<int>(mesh.chunks.size()) - 1;
        if (count < 1) {
            vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
            return;
        }
        const float at = params.trackDist + trackLead;
        const int c0 = std::clamp(static_cast<int>((at - behind) / kChunkLength), 0, count);
        const int c1 = std::clamp(static_cast<int>((at + ahead) / kChunkLength) + 1, c0, count);
        const uint32_t first = mesh.chunks[c0], last = mesh.chunks[c1];
        if (last > first) vkCmdDrawIndexed(cmd, last - first, 1, first, 0, 0);
    };

    if (!reflection) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sceneLayout, 1, 1,
                                &reflectionSet, 0, nullptr);
        bind(Material::Road);
        bindMesh(cmd, road);
        drawNear(road, 800.0f, 6000.0f);
    }

    bind(Material::Neon);
    bindMesh(cmd, rails);
    drawNear(rails, 800.0f, 6000.0f);
    bindMesh(cmd, gates);
    drawNear(gates, 800.0f, 6000.0f);
    bindMesh(cmd, features);
    drawNear(features, 800.0f, 6000.0f);

    // Missiles : même matériau lumineux que les rails.
    auto missileModel = [&](const MissileDraw& m) {
        return reflection ? mirrorMatrix(m.planePoint, m.planeNormal) * m.model : m.model;
    };
    if (params.missileCount > 0) {
        bindMesh(cmd, missile);
        for (int i = 0; i < params.missileCount; i++) {
            ScenePush push;
            push.model = missileModel(params.missiles[i]);
            pushScene(cmd, sceneLayout, push);
            vkCmdDrawIndexed(cmd, missile.indexCount, 1, 0, 0, 0);
        }
        pushScene(cmd, sceneLayout, identity);
    }

    bind(Material::Building);
    bindMesh(cmd, city);
    ScenePush cityPush;
    if (reflection) cityPush.model = mirrorMatrix(params.planePoint, params.planeNormal);
    // Au-dessus de 1,5, colorB.w est l'amplitude de l'égaliseur : la part de
    // hauteur qu'un bâtiment gagne quand sa bande est au maximum, plus 1,5.
    cityPush.colorB.w = 1.5f + kEqualizerStretch;
    pushScene(cmd, sceneLayout, cityPush);
    drawNear(city, 2500.0f, 8000.0f);

    auto shipPush = [&](const ShipDraw& s) {
        ScenePush push;
        push.model = reflection ? mirrorMatrix(s.planePoint, s.planeNormal) * s.model : s.model;
        push.colorA = s.trim;
        push.colorB = s.engine;
        return push;
    };

    auto variantOf = [&](const ShipDraw& s) {
        return std::clamp(s.variant, 0, sim::kShipCount - 1);
    };
    bind(Material::Ship);
    for (int i = 0; i < params.shipCount; i++) {
        const Mesh& mesh = ships[variantOf(params.ships[i])];
        bindMesh(cmd, mesh);
        pushScene(cmd, sceneLayout, shipPush(params.ships[i]));
        vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
    }

    // Le ciel passe après les objets opaques : le test de profondeur ne
    // laisse que les pixels encore vides.
    bind(Material::Sky);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    bind(Material::Trail);
    for (int i = 0; i < params.shipCount; i++) {
        const Mesh& mesh = trails[variantOf(params.ships[i])];
        bindMesh(cmd, mesh);
        pushScene(cmd, sceneLayout, shipPush(params.ships[i]));
        vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
    }
    // Flamme des missiles : celle de la Flèche, réduite.
    const Mesh& trail = trails[0];
    bindMesh(cmd, trail);
    for (int i = 0; i < params.missileCount; i++) {
        ScenePush push;
        push.model = missileModel(params.missiles[i]) *
                     glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.12f, 0.55f)) *
                     glm::scale(glm::mat4(1.0f), glm::vec3(0.35f));
        push.colorB = glm::vec4(1.0f, 0.2f, 0.8f, 14.0f);
        pushScene(cmd, sceneLayout, push);
        vkCmdDrawIndexed(cmd, trail.indexCount, 1, 0, 0, 0);
    }
    // Bulles des boucliers.
    bindMesh(cmd, shield);
    for (int i = 0; i < params.shipCount; i++) {
        if (params.ships[i].shield <= 0.0f) continue;
        ScenePush push = shipPush(params.ships[i]);
        push.colorB = glm::vec4(0.1f, 0.8f, 1.0f, params.ships[i].shield);
        pushScene(cmd, sceneLayout, push);
        vkCmdDrawIndexed(cmd, shield.indexCount, 1, 0, 0, 0);
    }
}

void Renderer::drawBloom(VkCommandBuffer cmd) {
    auto pushPost = [&](float tx, float ty, float z) {
        PostPush push;
        push.params[0] = tx;
        push.params[1] = ty;
        push.params[2] = z;
        vkCmdPushConstants(cmd, postLayout, kPushStages, 0, sizeof(push), &push);
    };

    // Réductions successives.
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bloomDownPipeline);
    for (int i = 0; i < kBloomLevels; i++) {
        VkExtent2D srcExtent = i == 0 ? sceneExtent : bloom[i - 1].extent;
        VkDescriptorSet src = i == 0 ? sceneSet : bloomSet[i - 1];
        beginPass(cmd, bloomDownPass, bloomFramebuffer[i], bloom[i].extent);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, postLayout, 0, 1, &src, 0,
                                nullptr);
        pushPost(1.0f / static_cast<float>(srcExtent.width),
                 1.0f / static_cast<float>(srcExtent.height),
                 i == 0 ? settings.bloomThreshold : 0.0f);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
    }

    // Agrandissements, chacun ajouté au niveau au-dessus.
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bloomUpPipeline);
    for (int i = kBloomLevels - 2; i >= 0; i--) {
        beginPass(cmd, bloomUpPass, bloomFramebuffer[i], bloom[i].extent);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, postLayout, 0, 1,
                                &bloomSet[i + 1], 0, nullptr);
        pushPost(1.0f / static_cast<float>(bloom[i + 1].extent.width),
                 1.0f / static_cast<float>(bloom[i + 1].extent.height), 1.0f);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
    }
}

// Relit les horodatages de la dernière image rendue dans ce slot. L'appelant a
// déjà attendu la fin de cette image, donc les résultats sont disponibles.
void Renderer::collectStats(int slot) {
    if (!queryPool || !queryPending[slot]) return;
    uint64_t ticks[kTimestamps] = {};
    VkResult result = vkGetQueryPoolResults(gpu->device, queryPool, slot * kTimestamps,
                                            kTimestamps, sizeof(ticks), ticks, sizeof(uint64_t),
                                            VK_QUERY_RESULT_64_BIT);
    queryPending[slot] = false;
    if (result != VK_SUCCESS) return;
    double period = gpu->properties.limits.timestampPeriod * 1e-6; // ticks vers millisecondes
    for (int i = 0; i < GpuStats::kPasses; i++) {
        double ms = static_cast<double>(ticks[i + 1] - ticks[i]) * period;
        stats.lastMs[i] = ms;
        stats.sumMs[i] += ms;
    }
    stats.samples++;
}

void Renderer::draw(VkCommandBuffer cmd, const FrameTarget& target, const FrameParams& params) {
    collectStats(params.slot);
    uint32_t firstQuery = static_cast<uint32_t>(params.slot) * kTimestamps;
    auto stamp = [&](uint32_t index) {
        if (queryPool)
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool,
                                firstQuery + index);
    };
    if (queryPool) vkCmdResetQueryPool(cmd, queryPool, firstQuery, kTimestamps);
    stamp(0);

    VkExtent2D wanted{
        std::max(1u, static_cast<uint32_t>(std::lround(
                         static_cast<float>(target.logicalExtent.width) * settings.renderScale))),
        std::max(1u, static_cast<uint32_t>(std::lround(
                         static_cast<float>(target.logicalExtent.height) * settings.renderScale)))};
    if (wanted.width != sceneExtent.width || wanted.height != sceneExtent.height) {
        VK_CHECK(vkDeviceWaitIdle(gpu->device));
        createTargets(wanted);
    }

    // Données globales des deux vues.
    float aspect = static_cast<float>(sceneExtent.width) / static_cast<float>(sceneExtent.height);
    glm::vec3 forward = glm::normalize(params.camTarget - params.camPos);
    glm::vec3 right = glm::normalize(glm::cross(forward, params.camUp));
    glm::vec3 up = glm::cross(right, forward);
    float tanY = std::tan(params.fovY * 0.5f);
    float tanX = tanY * aspect;

    Globals scene;
    scene.viewProj = viewProjection(params, aspect);
    scene.camPos = glm::vec4(params.camPos, params.time);
    scene.camRight = glm::vec4(right * tanX, 1.0f);
    scene.camUp = glm::vec4(up * tanY, 0.0f);
    scene.camForward = glm::vec4(forward, 0.0f);
    scene.fog = glm::vec4(theme.fogColor, theme.fogDensity);
    scene.neonA = glm::vec4(theme.neonA, 0.0f);
    scene.neonB = glm::vec4(theme.neonB, 0.0f);
    scene.skyZenith = glm::vec4(theme.skyZenith, 0.0f);
    scene.skyMid = glm::vec4(theme.skyMid, 0.0f);
    scene.skyHaze = glm::vec4(theme.skyHaze, static_cast<float>(theme.sky));
    scene.bandsLow = glm::vec4(params.bands[0], params.bands[1], params.bands[2], params.bands[3]);
    scene.bandsHigh = glm::vec4(params.bands[4], params.bands[5], params.bands[6], params.bands[7]);
    scene.screen = glm::vec4(1.0f / static_cast<float>(sceneExtent.width),
                             1.0f / static_cast<float>(sceneExtent.height), params.speed, params.pulse);

    // Vue en miroir : même caméra, géométrie retournée sous la piste, et
    // directions du ciel réfléchies.
    Globals mirror = scene;
    const glm::vec3& n = params.planeNormal;
    mirror.camRight = glm::vec4(mirrorVector(right, n) * tanX, -1.0f);
    mirror.camUp = glm::vec4(mirrorVector(up, n) * tanY, 0.0f);
    mirror.camForward = glm::vec4(mirrorVector(forward, n), 0.0f);

    Buffer& ubo = globalsBuffer[params.slot];
    std::memcpy(ubo.mapped, &scene, sizeof(scene));
    std::memcpy(static_cast<char*>(ubo.mapped) + globalsStride, &mirror, sizeof(mirror));
    vmaFlushAllocation(gpu->allocator, ubo.alloc, 0, VK_WHOLE_SIZE);

    // Passe 1 : reflet, à demi-résolution. Chaque passe laisse son image
    // prête à être lue par la suivante.
    beginPass(cmd, reflectionPass, reflectionFramebuffer, reflectionColor.extent);
    if (settings.reflections) drawWorld(cmd, params, true);
    vkCmdEndRenderPass(cmd);

    stamp(1);

    // Passe 2 : scène en MSAA. Les images multi-échantillonnées et la
    // profondeur ne sont jamais conservées : seule la résolution est écrite.
    beginPass(cmd, scenePass, sceneFramebuffer, sceneExtent);
    drawWorld(cmd, params, false);
    vkCmdEndRenderPass(cmd);

    stamp(2);

    // Passe 3 : bloom.
    drawBloom(cmd);
    stamp(3);

    // Passe 4 : composition dans l'image de sortie.
    {
        beginPass(cmd, outputPass, outputFramebuffer(target), target.extent);
        PostPush push;
        rotationMatrix(target.rotation, push.rotation);
        push.params[0] = settings.bloom ? settings.bloomStrength : 0.0f;
        push.params[1] = settings.exposure;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, postLayout, 0, 1,
                                &compositeSet, 0, nullptr);
        vkCmdPushConstants(cmd, postLayout, kPushStages, 0, sizeof(push), &push);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        // Interface, par-dessus l'image finale et à la résolution native.
        if (params.hud && !params.hud->verts.empty()) {
            uint32_t count = std::min<uint32_t>(
                static_cast<uint32_t>(params.hud->verts.size()), kMaxHudVertices);
            Buffer& vb = hudBuffer[params.slot];
            std::memcpy(vb.mapped, params.hud->verts.data(), count * sizeof(HudVertex));
            vmaFlushAllocation(gpu->allocator, vb.alloc, 0, VK_WHOLE_SIZE);

            push.params[0] = params.hud->screen.x;
            push.params[1] = params.hud->screen.y;
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hudPipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, postLayout, 0, 1,
                                    &fontSet, 0, nullptr);
            vkCmdPushConstants(cmd, postLayout, kPushStages, 0, sizeof(push), &push);
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &vb.buffer, &offset);
            vkCmdDraw(cmd, count, 1, 0, 0);
        }
        vkCmdEndRenderPass(cmd);
    }
    stamp(4);
    queryPending[params.slot] = true;
}

} // namespace gfx

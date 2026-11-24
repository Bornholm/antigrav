#pragma once

#include "gfx/gpu.h"
#include "gfx/hud.h"
#include "gfx/mesh.h"
#include "sim/track.h"

#include <glm/glm.hpp>

namespace gfx {

constexpr int kFramesInFlight = 2;
constexpr int kBloomLevels = 6;
constexpr int kMaxShips = 8;

// Image dans laquelle la dernière passe écrit : une image de la swapchain ou
// la cible hors écran du mode capture.
struct FrameTarget {
    VkImageView view = VK_NULL_HANDLE;
    VkExtent2D extent{};        // taille physique de l'image
    VkExtent2D logicalExtent{}; // taille vue par le jeu, après rotation
    int rotation = 0;
};

struct ShipDraw {
    glm::mat4 model{1.0f};
    // Plan de la piste sous le vaisseau, pour son reflet.
    glm::vec3 planePoint{0.0f};
    glm::vec3 planeNormal{0.0f, 1.0f, 0.0f};
    glm::vec4 trim{1.0f};   // rgb couleur des liserés, a intensité
    glm::vec4 engine{1.0f}; // rgb couleur des réacteurs, a intensité
};

// Tout ce que le rendu doit savoir d'une image. Rempli par le jeu.
struct FrameParams {
    int slot = 0; // indice de frame en vol
    float time = 0.0f;
    float speed = 0.0f;
    glm::vec3 camPos{0.0f};
    glm::vec3 camTarget{0.0f, 0.0f, -1.0f};
    glm::vec3 camUp{0.0f, 1.0f, 0.0f};
    float fovY = 1.2f;
    // Plan de la piste sous la caméra, pour le reflet de la ville et du ciel.
    glm::vec3 planePoint{0.0f};
    glm::vec3 planeNormal{0.0f, 1.0f, 0.0f};
    int shipCount = 0;
    ShipDraw ships[kMaxShips];
    // Interface de l'image, facultative.
    const HudBatch* hud = nullptr;
};

// Matrice vue-projection de la caméra d'une image, pour placer dans
// l'interface des éléments attachés à des objets de la scène.
glm::mat4 viewProjection(const FrameParams& params, float aspect);

struct RenderSettings {
    float renderScale = 1.0f; // échelle de la 3D par rapport à la sortie
    float bloomStrength = 0.11f;
    float bloomThreshold = 0.9f;
    float exposure = 1.0f;
    bool reflections = true;
    bool bloom = true;
};

enum class Material { Road, Neon, Building, Ship, Sky, Trail, Count };

// Temps GPU de chaque passe, mesurés par des horodatages dans la commande.
// Les budgets du plan se vérifient avec ces chiffres, pas à l'estime.
struct GpuStats {
    static constexpr int kPasses = 4; // reflet, scène, bloom, composition
    double sumMs[kPasses] = {};
    double lastMs[kPasses] = {};
    uint64_t samples = 0;

    double averageMs(int pass) const { return samples ? sumMs[pass] / samples : 0.0; }
    double averageTotalMs() const {
        double total = 0.0;
        for (int i = 0; i < kPasses; i++) total += averageMs(i);
        return total;
    }
    double lastTotalMs() const {
        double total = 0.0;
        for (double ms : lastMs) total += ms;
        return total;
    }
};

struct Renderer {
    Gpu* gpu = nullptr;
    RenderSettings settings;

    VkFormat outputFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D sceneExtent{};

    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout globalsLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout texturesLayout = VK_NULL_HANDLE;
    VkPipelineLayout sceneLayout = VK_NULL_HANDLE;
    VkPipelineLayout postLayout = VK_NULL_HANDLE;
    VkDescriptorPool staticPool = VK_NULL_HANDLE;
    VkDescriptorPool targetPool = VK_NULL_HANDLE;

    // Deux variantes par matériau : scène en MSAA, reflet sans MSAA.
    VkPipeline scenePipelines[static_cast<int>(Material::Count)]{};
    VkPipeline reflectionPipelines[static_cast<int>(Material::Count)]{};
    VkPipeline bloomDownPipeline = VK_NULL_HANDLE;
    VkPipeline bloomUpPipeline = VK_NULL_HANDLE;
    VkPipeline compositePipeline = VK_NULL_HANDLE;

    Buffer globalsBuffer[kFramesInFlight];
    VkDescriptorSet globalsSet[kFramesInFlight]{};
    uint32_t globalsStride = 0;

    Image reflectionColor, reflectionDepth;
    Image msaaColor, msaaDepth;
    Image sceneColor;
    Image bloom[kBloomLevels];
    VkDescriptorSet reflectionSet = VK_NULL_HANDLE;
    VkDescriptorSet sceneSet = VK_NULL_HANDLE;
    VkDescriptorSet bloomSet[kBloomLevels]{};
    VkDescriptorSet compositeSet = VK_NULL_HANDLE;

    Mesh road, rails, gates, city, ship, trail;

    FontAtlas fonts;
    Image fontTexture;
    VkDescriptorSet fontSet = VK_NULL_HANDLE;
    VkPipeline hudPipeline = VK_NULL_HANDLE;
    Buffer hudBuffer[kFramesInFlight];
    static constexpr uint32_t kMaxHudVertices = 24000;

    // Horodatages : un groupe de requêtes par frame en vol.
    static constexpr uint32_t kTimestamps = GpuStats::kPasses + 1;
    VkQueryPool queryPool = VK_NULL_HANDLE;
    bool queryPending[kFramesInFlight] = {};
    GpuStats stats;

    void init(Gpu& gpu, VkFormat outputFormat, const sim::Track& track);
    void shutdown();

    // La cible doit être en disposition COLOR_ATTACHMENT_OPTIMAL.
    void draw(VkCommandBuffer cmd, const FrameTarget& target, const FrameParams& params);

  private:
    void createTargets(VkExtent2D extent);
    void destroyTargets();
    void drawWorld(VkCommandBuffer cmd, const FrameParams& params, bool reflection);
    void collectStats(int slot);
    void drawBloom(VkCommandBuffer cmd);
};

} // namespace gfx

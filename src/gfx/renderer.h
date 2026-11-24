#pragma once

#include "gfx/gpu.h"

namespace gfx {

// Image dans laquelle la dernière passe écrit : une image de la swapchain ou
// la cible hors écran du mode capture.
struct FrameTarget {
    VkImageView view = VK_NULL_HANDLE;
    VkExtent2D extent{};        // taille physique de l'image
    VkExtent2D logicalExtent{}; // taille vue par le jeu, après rotation
    int rotation = 0;
};

struct FrameParams {
    float time = 0.0f;
};

struct Renderer {
    Gpu* gpu = nullptr;
    VkPipelineLayout bootLayout = VK_NULL_HANDLE;
    VkPipeline bootPipeline = VK_NULL_HANDLE;

    void init(Gpu& gpu, VkFormat outputFormat);
    void shutdown();

    // La cible doit être en disposition COLOR_ATTACHMENT_OPTIMAL.
    void draw(VkCommandBuffer cmd, const FrameTarget& target, const FrameParams& params);
};

} // namespace gfx

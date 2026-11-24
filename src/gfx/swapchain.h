#pragma once

#include "gfx/gpu.h"

#include <vector>

namespace gfx {

// Chaîne d'images présentées à l'écran.
//
// Sur Android, l'écran physique est en portrait et le jeu en paysage. Le jeu
// applique lui-même la rotation au lieu de la laisser au compositeur, ce qui
// retire une passe de composition par image. `extent` est donc la taille de
// l'image physique, `logicalExtent` celle vue par le jeu, et `rotation` l'angle
// à appliquer dans la dernière passe.
struct Swapchain {
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
    VkExtent2D logicalExtent{};
    int rotation = 0; // degrés, sens horaire : 0, 90, 180 ou 270
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    // Un sémaphore par image, et non par frame : une image peut être
    // réacquise avant que la présentation précédente ait libéré le sien.
    std::vector<VkSemaphore> renderDone;

    // Renvoie false si la surface a une taille nulle, fenêtre réduite par exemple.
    bool create(Gpu& gpu, uint32_t width, uint32_t height, bool vsync);
    void destroy(Gpu& gpu);
};

} // namespace gfx

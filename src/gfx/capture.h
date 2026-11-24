#pragma once

#include "gfx/gpu.h"

namespace gfx {

// Relit une image RGBA 8 bits et l'écrit en PNG. Sert au mode sans fenêtre.
struct Capture {
    Buffer staging;
    VkExtent2D extent{};

    void init(Gpu& gpu, VkExtent2D size);
    void shutdown(Gpu& gpu);

    // L'image doit être en disposition TRANSFER_SRC_OPTIMAL.
    void recordCopy(VkCommandBuffer cmd, const Image& image);
    // À appeler une fois la commande de copie terminée sur le GPU.
    bool writePng(const char* path) const;
};

} // namespace gfx

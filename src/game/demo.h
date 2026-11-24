#pragma once

#include "game/hud.h"
#include "gfx/renderer.h"
#include "sim/track.h"

namespace game {

// Scène du prototype de rendu : quatre vaisseaux sur rails et une caméra de
// poursuite. Aucune physique, la simulation arrive en phase 2.
struct Demo {
    sim::Track track;
    float speed = 352.0f; // m/s, soit 1 268 km/h comme sur le mockup

    std::vector<glm::vec2> minimap;
    gfx::HudBatch hud;

    void init();
    // screen : taille de l'écran vue par le jeu, en pixels.
    void fill(float time, const gfx::FontAtlas& fonts, glm::vec2 screen, gfx::FrameParams& out);
};

} // namespace game

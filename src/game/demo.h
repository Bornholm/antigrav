#pragma once

#include "gfx/renderer.h"
#include "sim/track.h"

namespace game {

// Scène du prototype de rendu : quatre vaisseaux sur rails et une caméra de
// poursuite. Aucune physique, la simulation arrive en phase 2.
struct Demo {
    sim::Track track;
    float speed = 352.0f; // m/s, soit 1 268 km/h comme sur le mockup

    void init();
    void fill(float time, gfx::FrameParams& out) const;
};

} // namespace game

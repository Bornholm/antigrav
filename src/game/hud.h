#pragma once

#include "game/input.h"
#include "gfx/hud.h"

#include <glm/glm.hpp>

#include <vector>

namespace game {

struct NameTag {
    glm::vec3 worldPos{0.0f};
    int rank = 0;
    const char* name = "";
    glm::vec4 color{1.0f};
};

// Ce que l'interface affiche pendant une course.
struct HudState {
    float energy = 1.0f; // 0 à 1
    int position = 1;
    int racers = 1;
    int lap = 1;
    int laps = 3;
    float speedKmh = 0.0f;
    int tagCount = 0;
    NameTag tags[8];
    // Tracé du circuit vu de dessus, normalisé dans [-1, 1].
    const std::vector<glm::vec2>* minimap = nullptr;
    float progress = 0.0f; // position du joueur sur le tour, 0 à 1
    bool boosting = false;
    float countdown = 0.0f; // secondes avant le départ
    float raceTime = 0.0f;
    bool finished = false;
    bool pressed[static_cast<int>(Button::Count)] = {};
};

void buildHud(gfx::HudBatch& hud, const HudState& state, const glm::mat4& viewProj);

} // namespace game

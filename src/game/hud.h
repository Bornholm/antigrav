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
    int position = 1;
    int racers = 1;
    float remaining = 0.0f; // secondes de morceau restantes
    bool showGap = false;   // affiche l'avance sur la musique
    float gap = 0.0f;       // secondes d'avance, négatif en retard
    float speedKmh = 0.0f;
    int tagCount = 0;
    NameTag tags[8];
    // Piste à venir vue de dessus, normalisée dans [-1, 1], du point le plus
    // en arrière au plus lointain.
    const std::vector<glm::vec2>* minimap = nullptr;
    float progress = 0.0f; // position du joueur le long de ce tracé, 0 à 1
    // Avancement des vaisseaux dans le morceau, 0 à 1.
    int markCount = 0;
    float marks[8] = {};
    glm::vec4 markColors[8];
    int localMark = 0;
    bool boosting = false;  // en turbo
    float countdown = 0.0f; // secondes avant le départ
    float raceTime = 0.0f;
    bool finished = false;
    // Ligne de mesures affichée en bas de l'écran, vide pour la masquer.
    const char* statsLine = "";
    glm::vec3 shipPos{0.0f}; // vaisseau du joueur, pour y accrocher les alertes
    int item = 0;            // bonus porté, voir sim::Item ; 0 pour aucun
    bool incoming = false;   // un tir arrive sur le joueur
    bool shielded = false;
    bool gridStart = false; // départ arrêté : affiche le signal du départ
    bool pressed[static_cast<int>(Button::Count)] = {};
};

void buildHud(gfx::HudBatch& hud, const HudState& state, const glm::mat4& viewProj);

} // namespace game

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
    float remaining = 0.0f; // secondes de morceau restantes
    bool showGap = false;   // horloge commune : affiche l'avance sur la musique
    float gap = 0.0f;       // secondes d'avance, négatif en retard
    float accord = 0.0f;    // 1 juste après un boost lancé en rythme, puis retombe
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
    bool boosting = false;
    float countdown = 0.0f; // secondes avant le départ
    float raceTime = 0.0f;
    bool finished = false;
    // Ligne de mesures affichée en bas de l'écran, vide pour la masquer.
    const char* statsLine = "";
    glm::vec3 shipPos{0.0f}; // vaisseau du joueur, pour y accrocher le retour de rythme
    float beatPhase = -1.0f; // part de l'intervalle avant le prochain temps, de 1 à 0 ; négatif pour masquer
    float cooldown = 0.0f;   // délai restant avant le prochain accord possible, de 1 à 0
    int miss = 0;            // appui raté : 1 trop tôt, 2 trop tard
    float missFade = 0.0f;
    int combo = 0;        // accords d'affilée, sur comboSize
    int comboSize = 4;
    bool armored = false; // la jauge suffit à encaisser une barrière
    bool incoming = false; // une décharge arrive sur le joueur
    bool gridStart = false; // départ arrêté : affiche le signal du départ
    bool pressed[static_cast<int>(Button::Count)] = {};
};

void buildHud(gfx::HudBatch& hud, const HudState& state, const glm::mat4& viewProj);

} // namespace game

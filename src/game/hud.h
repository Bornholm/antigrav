#pragma once

#include "game/score.h"
#include "game/input.h"
#include "gfx/hud.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace game {

struct NameTag {
    glm::vec3 worldPos{0.0f};
    int rank = 0;
    const char* name = "";
    glm::vec4 color{1.0f};
};

// Éléments de l'interface de course, à afficher ou non. Le tutoriel ne
// montre que ceux dont parle chaque page.
enum HudPart : uint32_t {
    kHudStandings = 1u << 0, // place et fin du morceau, en haut à gauche
    kHudTimer = 1u << 1,     // chrono, écart avec la musique et avec le fantôme
    kHudSpeed = 1u << 2,
    kHudMinimap = 1u << 3,
    kHudSongBar = 1u << 4,
    kHudButtons = 1u << 5,   // boutons tactiles, ou bonus porté au clavier
    kHudTags = 1u << 6,      // étiquettes des autres vaisseaux
    kHudAlerts = 1u << 7,
    kHudCountdown = 1u << 8, // compte à rebours et signal du départ
    kHudAll = 0xFFFFFFFFu,
};

// Ce que l'interface affiche pendant une course.
struct HudState {
    uint32_t parts = kHudAll;
    float countdownY = 400.0f; // hauteur du compte à rebours, dans le repère de 1600 × 900
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
    Score score;           // score de la course, affiché à l'arrivée
    int bestScore = 0;     // record précédent sur ce morceau, 0 s'il n'y en a pas
    bool newRecord = false;
    float finishTime = 0.0f; // temps à l'arrivée
    float bestTime = 0.0f;   // meilleur temps précédent sur ce morceau, 0 s'il n'y en a pas
    bool newBestTime = false;
    bool showGhostGap = false; // affiche l'écart avec le fantôme
    float ghostGap = 0.0f;     // secondes de retard sur le fantôme, négatif en avance
    // Ligne de mesures affichée en bas de l'écran, vide pour la masquer.
    const char* statsLine = "";
    glm::vec3 shipPos{0.0f}; // vaisseau du joueur, pour y accrocher les alertes
    glm::vec4 flash{0.0f};   // voile de couleur sur tout l'écran : rgb teinte, a force
    int item = 0;            // bonus porté, voir sim::Item ; 0 pour aucun
    bool incoming = false;   // un tir arrive sur le joueur
    bool shielded = false;
    bool gridStart = false; // départ arrêté : affiche le signal du départ
    int startBoost = 0;     // départ turbo du joueur : 0 pas tenté, 1 réussi, 2 raté
    bool pressed[static_cast<int>(Button::Count)] = {};
};

void buildHud(gfx::HudBatch& hud, const HudState& state, const glm::mat4& viewProj);

// Nom et couleur d'un bonus, voir sim::Item. Le nom est à traduire.
const char* itemName(int item);
glm::vec4 itemColor(int item);

// Couleur d'une note, de S à D.
glm::vec4 gradeColor(char grade);

} // namespace game

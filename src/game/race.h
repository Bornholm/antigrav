#pragma once

#include "game/hud.h"
#include "game/input.h"
#include "game/synth.h"
#include "gfx/renderer.h"
#include "sim/session.h"

namespace game {

using sim::kHuman;
using sim::RaceConfig;
using sim::RacerSetup;

// Une course à l'écran : la session de simulation, plus la caméra de
// poursuite, le son et l'interface.
struct Race : sim::Session {
    float accumulator = 0.0f;
    // Client réseau : chaque pas passe par le code réseau, qui prédit et corrige.
    void (*clientTick)(void* user, Race& race, const sim::Input& local) = nullptr;
    void* clientUser = nullptr;

    std::vector<glm::vec2> minimap;
    gfx::HudBatch hud;
    Controls controls;
    // Synthétiseur à tenir informé de l'état de la course. Peut rester nul.
    Synth* synth = nullptr;
    // Texte de mesures à afficher en bas de l'écran, fourni par la boucle principale.
    const char* statsLine = "";
    float bands[8] = {}; // niveaux de la musique, lissés pour l'affichage
    int theme = 0;      // décor de la piste
    int songSerial = 0; // change quand la piste change : le rendu la reconstruit alors
    bool pinToSync = false; // tient le vaisseau local sur le point de synchronisation
    bool showHud = true;

    // Caméra lissée.
    float camLateral = 0.0f;
    float camFov = 1.12f;
    float camShake = 0.0f;
    // Écarts de correction réseau, résorbés en douceur à l'affichage.
    float smoothDist[sim::kMaxVehicles] = {};
    float smoothLateral[sim::kMaxVehicles] = {};

    void init(const RaceConfig& config, int localIndex);
    // Avance la simulation du temps écoulé depuis la dernière image.
    void update(float dt, const Controls& input);
    // Caméra, son et lissages, à appeler une fois par image.
    void updatePresentation(float dt);
    // screen : taille de l'écran vue par le jeu, en pixels.
    void fill(const gfx::FontAtlas& fonts, glm::vec2 screen, gfx::FrameParams& out);

    bool finished() const { return world.vehicles[localIndex].finished; }
    // Caméra en orbite autour du vaisseau local, pour les menus. Angle en radians.
    bool orbit = false;
    float orbitAngle = 0.0f;
    // Décale le vaisseau dans l'image : x vers la droite, y vers le bas, en mètres.
    glm::vec2 orbitShift{0.0f};
};

// Course d'exemple : un joueur et trois IA de personnalités différentes.
RaceConfig defaultConfig(int track, bool gridStart);

} // namespace game

#pragma once

#include "game/hud.h"
#include "game/input.h"
#include "game/synth.h"
#include "gfx/renderer.h"
#include "sim/track.h"
#include "sim/world.h"

namespace game {

constexpr int kHuman = -1;

struct RacerSetup {
    char name[20] = "";
    int ship = 0;
    int profile = kHuman; // personnalité d'IA, ou kHuman pour un joueur
};

// Tout ce qui définit une course. C'est aussi ce que l'hôte envoie aux clients.
struct RaceConfig {
    int track = 0;
    int laps = 3;
    int racerCount = 0;
    RacerSetup racers[sim::kMaxVehicles];
    bool gridStart = true; // départ arrêté avec compte à rebours
};

// Une course : simulation à pas fixe, caméra de poursuite et interface.
struct Race {
    RaceConfig config;
    sim::Track track;
    sim::World world;
    sim::World previous; // état du pas précédent, pour interpoler le rendu
    float accumulator = 0.0f;
    // Vaisseau suivi par la caméra et piloté par les commandes locales.
    int localIndex = 0;
    // Le vaisseau local est piloté par l'ordinateur : démonstration et captures.
    bool autopilot = false;
    // Client réseau : la simulation est avancée par le code réseau, pas ici.
    bool externalSim = false;
    // Commandes des joueurs distants, remplies par l'hôte.
    sim::Input remoteInputs[sim::kMaxVehicles];
    // Dernières commandes appliquées à chaque vaisseau.
    sim::Input lastInputs[sim::kMaxVehicles];

    std::vector<glm::vec2> minimap;
    gfx::HudBatch hud;
    Controls controls;
    // Synthétiseur à tenir informé de l'état de la course. Peut rester nul.
    Synth* synth = nullptr;
    // Texte de mesures à afficher en bas de l'écran, fourni par la boucle principale.
    const char* statsLine = "";
    bool showHud = true;

    // Caméra lissée.
    float camLateral = 0.0f;
    float camFov = 1.12f;
    float camShake = 0.0f;
    // Écarts de correction réseau, résorbés en douceur à l'affichage.
    float smoothDist[sim::kMaxVehicles] = {};
    float smoothLateral[sim::kMaxVehicles] = {};

    void init(const RaceConfig& config, int localIndex);
    // Commandes de chaque vaisseau pour le prochain pas.
    void gatherInputs(const Controls& input, sim::Input* out);
    // Avance la simulation du temps écoulé depuis la dernière image.
    void update(float dt, const Controls& input);
    // Caméra, son et lissages, à appeler une fois par image.
    void updatePresentation(float dt);
    // screen : taille de l'écran vue par le jeu, en pixels.
    void fill(const gfx::FontAtlas& fonts, glm::vec2 screen, gfx::FrameParams& out);

    bool finished() const { return world.vehicles[localIndex].finished; }
};

// Course d'exemple : un joueur et trois IA de personnalités différentes.
RaceConfig defaultConfig(int track, bool gridStart);

} // namespace game

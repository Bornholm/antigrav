#pragma once

#include "game/hud.h"
#include "game/input.h"
#include "gfx/renderer.h"
#include "sim/track.h"
#include "sim/world.h"

namespace game {

// Une course : simulation à pas fixe, caméra de poursuite et interface.
struct Race {
    sim::Track track;
    sim::World world;
    sim::World previous; // état du pas précédent, pour interpoler le rendu
    float accumulator = 0.0f;
    int laps = 3;
    // Le joueur est piloté par l'ordinateur : mode démonstration et captures.
    bool autopilot = false;

    std::vector<glm::vec2> minimap;
    gfx::HudBatch hud;
    Controls controls;

    // Caméra lissée.
    float camLateral = 0.0f;
    float camFov = 1.12f;
    float camShake = 0.0f;

    // gridStart : départ arrêté avec compte à rebours. Sinon les vaisseaux
    // sont déjà lancés, ce qui sert aux démonstrations et aux captures.
    void init(bool gridStart);
    // Texte de mesures à afficher en bas de l'écran, fourni par la boucle principale.
    const char* statsLine = "";
    // Avance la simulation du temps écoulé depuis la dernière image.
    void update(float dt, const Controls& input);
    // screen : taille de l'écran vue par le jeu, en pixels.
    void fill(const gfx::FontAtlas& fonts, glm::vec2 screen, gfx::FrameParams& out);
};

} // namespace game

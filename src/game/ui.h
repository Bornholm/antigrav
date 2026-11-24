#pragma once

#include "game/input.h"
#include "gfx/hud.h"

namespace game {

// Petits composants de menu en mode immédiat : chaque image redessine tout et
// renvoie ce qui vient d'être activé. Utilisables au doigt, à la souris, au
// clavier et à la manette.
struct Ui {
    gfx::HudBatch* hud = nullptr;
    UiInput in;
    int focus = 0;     // composant sélectionné au clavier ou à la manette
    int count = 0;     // composants déclarés pendant l'image en cours
    int lastCount = 0; // composants de l'image précédente
    float time = 0.0f;

    void begin(gfx::HudBatch& batch, const UiInput& input, float now);
    void end();
    // Remet la sélection en haut, à appeler en changeant d'écran.
    void reset() { focus = 0; }

    // Coordonnées dans le repère de référence de l'interface, ancrées au centre.
    bool button(glm::vec2 center, glm::vec2 half, const char* label, bool enabled = true,
                glm::vec4 accent = glm::vec4(0.25f, 0.92f, 1.0f, 1.0f));
    // Ligne « libellé  < valeur > ». Renvoie -1, 0 ou +1.
    int selector(glm::vec2 center, float halfWidth, const char* label, const char* value);

    void title(glm::vec2 pos, const char* text, float size);
    void text(glm::vec2 pos, const char* text, float size, glm::vec4 color,
              gfx::Align align = gfx::Align::Center, gfx::FontId font = gfx::FontId::Medium);
    void panel(glm::vec2 center, glm::vec2 half);

  private:
    bool tapped(glm::vec2 center, glm::vec2 half) const;
};

} // namespace game

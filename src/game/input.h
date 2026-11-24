#pragma once

#include <glm/glm.hpp>

union SDL_Event;

namespace game {

enum class Button { Left, Right, Boost, Count };

// Centre et rayon d'un bouton tactile, en pixels. Sert à la fois au dessin de
// l'interface et aux tests de contact, pour que les deux ne divergent jamais.
struct ButtonShape {
    glm::vec2 center;
    float radius;
};
ButtonShape buttonShape(Button button, glm::vec2 screen);

// Commandes du joueur pour une image.
struct Controls {
    float steer = 0.0f;
    bool pressed[static_cast<int>(Button::Count)] = {};
};

// Ce que les menus retiennent des entrées d'une image : des appuis ponctuels,
// pas des touches maintenues.
struct UiInput {
    glm::vec2 taps[4];
    int tapCount = 0;
    bool up = false, down = false, left = false, right = false;
    bool accept = false, back = false;
    char typed[8] = ""; // chiffres et points saisis au clavier
    int typedCount = 0;
    bool backspace = false;
};

// Réunit clavier, souris et écran tactile en un seul état de commandes.
struct InputReader {
    static constexpr int kMaxPointers = 10;
    struct Pointer {
        bool active = false;
        long long id = 0;
        glm::vec2 pos{0.0f};
    };
    Pointer pointers[kMaxPointers];
    bool keyLeft = false, keyRight = false, keyBoost = false;
    // Manette : stick gauche ou croix pour tourner, A ou gâchette droite pour le boost.
    float padSteer = 0.0f;
    bool padLeft = false, padRight = false, padBoost = false, padTrigger = false;

    UiInput ui;

    void handleEvent(const SDL_Event& event, glm::vec2 screen);
    // Rend les appuis accumulés depuis le dernier appel et les efface.
    UiInput consumeUi();
    Controls read(glm::vec2 screen) const;
};

} // namespace game

#include "game/input.h"

#include "gfx/hud.h"

#include <SDL3/SDL.h>

namespace game {

ButtonShape buttonShape(Button button, glm::vec2 screen) {
    // Positions dans le repère de référence de l'interface, 1600 par 900.
    struct Def {
        gfx::Anchor anchor;
        glm::vec2 center;
        float radius;
    };
    static const Def defs[] = {
        {gfx::Anchor::Left, {113.0f, 757.0f}, 70.0f},
        {gfx::Anchor::Left, {277.0f, 757.0f}, 70.0f},
        {gfx::Anchor::Right, {1427.0f, 742.0f}, 96.0f},
        {gfx::Anchor::Right, {1316.0f, 588.0f}, 60.0f},
        {gfx::Anchor::Right, {1476.0f, 563.0f}, 58.0f},
    };
    const Def& def = defs[static_cast<int>(button)];
    float scale = screen.y / gfx::HudBatch::kRefHeight;
    float x = def.anchor == gfx::Anchor::Right
                  ? screen.x - (gfx::HudBatch::kRefWidth - def.center.x) * scale
                  : def.center.x * scale;
    return {{x, def.center.y * scale}, def.radius * scale};
}

void InputReader::handleEvent(const SDL_Event& event, glm::vec2 screen) {
    auto setPointer = [&](long long id, glm::vec2 pos, bool down) {
        Pointer* slot = nullptr;
        for (Pointer& p : pointers)
            if (p.active && p.id == id) slot = &p;
        if (!slot && down)
            for (Pointer& p : pointers)
                if (!p.active) {
                    slot = &p;
                    break;
                }
        if (!slot) return;
        slot->active = down;
        slot->id = id;
        slot->pos = pos;
    };

    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        bool down = event.type == SDL_EVENT_KEY_DOWN;
        switch (event.key.scancode) {
        case SDL_SCANCODE_LEFT:
        case SDL_SCANCODE_A:
            keyLeft = down;
            break;
        case SDL_SCANCODE_RIGHT:
        case SDL_SCANCODE_D:
            keyRight = down;
            break;
        case SDL_SCANCODE_SPACE:
        case SDL_SCANCODE_UP:
        case SDL_SCANCODE_W:
            keyBoost = down;
            break;
        case SDL_SCANCODE_E:
            keyMissile = down;
            break;
        case SDL_SCANCODE_Q:
            keyShield = down;
            break;
        default:
            break;
        }
        break;
    }
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        // Coordonnées normalisées entre 0 et 1.
        setPointer(static_cast<long long>(event.tfinger.fingerID) + 1,
                   {event.tfinger.x * screen.x, event.tfinger.y * screen.y},
                   event.type == SDL_EVENT_FINGER_DOWN || event.type == SDL_EVENT_FINGER_MOTION);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.which == SDL_TOUCH_MOUSEID) break; // déjà reçu comme doigt
        setPointer(0, {event.button.x, event.button.y},
                   event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (event.motion.which == SDL_TOUCH_MOUSEID) break;
        for (Pointer& p : pointers)
            if (p.active && p.id == 0) p.pos = {event.motion.x, event.motion.y};
        break;
    default:
        break;
    }
}

Controls InputReader::read(glm::vec2 screen) const {
    Controls c;
    for (int b = 0; b < static_cast<int>(Button::Count); b++) {
        ButtonShape shape = buttonShape(static_cast<Button>(b), screen);
        for (const Pointer& p : pointers) {
            if (!p.active) continue;
            // Marge de tolérance : un pouce vise moins bien qu'une souris.
            if (glm::distance(p.pos, shape.center) <= shape.radius * 1.15f) c.pressed[b] = true;
        }
    }
    auto on = [&](Button b) -> bool& { return c.pressed[static_cast<int>(b)]; };
    on(Button::Left) |= keyLeft;
    on(Button::Right) |= keyRight;
    on(Button::Boost) |= keyBoost;
    on(Button::Missile) |= keyMissile;
    on(Button::Shield) |= keyShield;
    c.steer = (on(Button::Right) ? 1.0f : 0.0f) - (on(Button::Left) ? 1.0f : 0.0f);
    return c;
}

} // namespace game

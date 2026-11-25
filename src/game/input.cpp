#include "game/input.h"

#include <cstring>

#include "gfx/hud.h"

#include <SDL3/SDL.h>

#include <cmath>

namespace game {

namespace {
bool swappedButtons = false;
#ifdef __ANDROID__
bool touchSeen = true;
#else
bool touchSeen = false;
#endif
} // namespace

void setButtonsSwapped(bool swapped) { swappedButtons = swapped; }
bool buttonsSwapped() { return swappedButtons; }
bool touchInterface() { return touchSeen; }
void forceTouchInterface() { touchSeen = true; }

ButtonShape buttonShape(Button button, glm::vec2 screen) {
    // Positions dans le repère de référence de l'interface, 1600 par 900.
    struct Def {
        gfx::Anchor anchor;
        glm::vec2 center;
        float radius;
    };
    static const Def defs[] = {
        {gfx::Anchor::Left, {128.0f, 756.0f}, 104.0f},
        {gfx::Anchor::Left, {356.0f, 756.0f}, 104.0f},
        {gfx::Anchor::Right, {1418.0f, 734.0f}, 118.0f},
    };
    Def def = defs[static_cast<int>(button)];
    if (swappedButtons) {
        // Miroir : chaque bouton passe de l'autre côté de l'écran. Gauche et
        // droite gardent leur ordre entre eux.
        def.anchor = def.anchor == gfx::Anchor::Left ? gfx::Anchor::Right : gfx::Anchor::Left;
        def.center.x = gfx::HudBatch::kRefWidth - def.center.x;
        if (button == Button::Left) def.center.x -= 228.0f;
        if (button == Button::Right) def.center.x += 228.0f;
    }
    return {gfx::HudBatch::place(def.center, def.anchor, gfx::VAnchor::Bottom, screen),
            def.radius * gfx::HudBatch::fitScale(screen)};
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

    auto tap = [&](glm::vec2 pos) {
        if (ui.tapCount < 4) ui.taps[ui.tapCount++] = pos;
    };

    // Appuis ponctuels pour les menus.
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        SDL_Scancode sc = event.key.scancode;
        if (sc == SDL_SCANCODE_UP) ui.up = true;
        else if (sc == SDL_SCANCODE_DOWN) ui.down = true;
        else if (sc == SDL_SCANCODE_LEFT) ui.left = true;
        else if (sc == SDL_SCANCODE_RIGHT) ui.right = true;
        else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) ui.accept = true;
        else if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_AC_BACK) ui.back = true;
        else if (sc == SDL_SCANCODE_BACKSPACE) ui.backspace = true;
        char c = 0;
        if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) c = static_cast<char>('1' + (sc - SDL_SCANCODE_1));
        else if (sc == SDL_SCANCODE_0) c = '0';
        else if (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_9) c = static_cast<char>('1' + (sc - SDL_SCANCODE_KP_1));
        else if (sc == SDL_SCANCODE_KP_0) c = '0';
        else if (sc == SDL_SCANCODE_PERIOD || sc == SDL_SCANCODE_KP_PERIOD) c = '.';
        if (c && ui.typedCount < 7) {
            ui.typed[ui.typedCount++] = c;
            ui.typed[ui.typedCount] = '\0';
        }
    }
    if (event.type == SDL_EVENT_TEXT_INPUT && event.text.text) {
        const size_t have = std::strlen(ui.text), add = std::strlen(event.text.text);
        if (have + add < sizeof(ui.text)) std::memcpy(ui.text + have, event.text.text, add + 1);
    }
    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        switch (event.gbutton.button) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP: ui.up = true; break;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN: ui.down = true; break;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: ui.left = true; break;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: ui.right = true; break;
        case SDL_GAMEPAD_BUTTON_SOUTH: ui.accept = true; break;
        case SDL_GAMEPAD_BUTTON_EAST: ui.back = true; break;
        default: break;
        }
    }
    if (event.type == SDL_EVENT_FINGER_DOWN) touchSeen = true;
    if (event.type == SDL_EVENT_FINGER_DOWN)
        tap({event.tfinger.x * screen.x, event.tfinger.y * screen.y});
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.which != SDL_TOUCH_MOUSEID)
        tap({event.button.x, event.button.y});

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
            keyUse = down;
            break;
        default:
            break;
        }
        break;
    }
    case SDL_EVENT_GAMEPAD_ADDED:
        // Ouvrir la manette suffit pour recevoir ses événements.
        SDL_OpenGamepad(event.gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
        float value = static_cast<float>(event.gaxis.value) / 32767.0f;
        if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) {
            // Zone morte : un stick au repos ne renvoie jamais exactement zéro.
            const float dead = 0.18f;
            float magnitude = (std::abs(value) - dead) / (1.0f - dead);
            padSteer = magnitude > 0.0f ? (value > 0.0f ? magnitude : -magnitude) : 0.0f;
        } else if (event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
            padTrigger = value > 0.3f;
        }
        break;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        bool down = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        switch (event.gbutton.button) {
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            padLeft = down;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            padRight = down;
            break;
        case SDL_GAMEPAD_BUTTON_SOUTH:
            padUse = down;
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

UiInput InputReader::consumeUi() {
    UiInput out = ui;
    ui = UiInput{};
    return out;
}

Controls InputReader::read(glm::vec2 screen) const {
    Controls c;
    // Direction : toute la zone autour des deux boutons compte, pas seulement
    // les disques. Un pouce qui glisse hors du dessin continue de tourner, et
    // passer d'un bouton à l'autre se fait sans lever le doigt.
    {
        ButtonShape left = buttonShape(Button::Left, screen);
        ButtonShape right = buttonShape(Button::Right, screen);
        float split = (left.center.x + right.center.x) * 0.5f;
        float begin = left.center.x - left.radius * 1.35f;
        float end = right.center.x + right.radius * 1.35f;
        float top = left.center.y - left.radius * 1.9f;
        for (const Pointer& p : pointers) {
            // Sans écran tactile, la souris ne pilote pas : elle sert aux menus.
            if (!touchSeen || !p.active || p.pos.y < top || p.pos.x > end || p.pos.x < begin) continue;
            c.pressed[static_cast<int>(p.pos.x < split ? Button::Left : Button::Right)] = true;
        }
    }
    for (int b = static_cast<int>(Button::Use); b < static_cast<int>(Button::Count); b++) {
        ButtonShape shape = buttonShape(static_cast<Button>(b), screen);
        for (const Pointer& p : pointers) {
            if (!touchSeen || !p.active) continue;
            // Marge de tolérance : un pouce vise moins bien qu'une souris.
            if (glm::distance(p.pos, shape.center) <= shape.radius * 1.15f) c.pressed[b] = true;
        }
    }
    auto on = [&](Button b) -> bool& { return c.pressed[static_cast<int>(b)]; };
    on(Button::Left) |= keyLeft || padLeft;
    on(Button::Right) |= keyRight || padRight;
    on(Button::Use) |= keyUse || padUse || padTrigger;
    c.steer = (on(Button::Right) ? 1.0f : 0.0f) - (on(Button::Left) ? 1.0f : 0.0f);
    // Le stick donne un braquage progressif, que les boutons n'ont pas.
    if (c.steer == 0.0f) c.steer = padSteer;
    return c;
}

} // namespace game

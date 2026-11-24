#include "game/ui.h"

#include "game/i18n.h"

#include <cmath>

namespace game {

namespace {

using gfx::Align;
using gfx::FontId;
using glm::vec2;
using glm::vec4;

const vec4 kWhite{1.0f, 1.0f, 1.0f, 1.0f};
const vec4 kCyan{0.25f, 0.92f, 1.0f, 1.0f};

vec4 withAlpha(vec4 c, float a) { return {c.r, c.g, c.b, c.a * a}; }

} // namespace

void Ui::begin(gfx::HudBatch& batch, const UiInput& input, float now) {
    hud = &batch;
    hud->anchor = gfx::Anchor::Center;
    hud->vAnchor = gfx::VAnchor::Middle;
    in = input;
    time = now;
    count = 0;
    if (lastCount > 0) {
        if (in.down) focus = (focus + 1) % lastCount;
        if (in.up) focus = (focus + lastCount - 1) % lastCount;
        if (focus >= lastCount) focus = lastCount - 1;
    }
}

void Ui::end() { lastCount = count; }

bool Ui::tapped(vec2 center, vec2 half) const {
    vec2 c = hud->px(center);
    vec2 h = half * hud->scale;
    for (int i = 0; i < in.tapCount; i++) {
        vec2 d = glm::abs(in.taps[i] - c);
        if (d.x <= h.x && d.y <= h.y) return true;
    }
    return false;
}

bool Ui::button(vec2 center, vec2 half, const char* label, bool enabled, vec4 accent) {
    label = tr(label);
    int index = count++;
    bool focused = index == focus;
    bool hit = enabled && tapped(center, half);
    if (hit) focus = index;
    bool activated = hit || (enabled && focused && in.accept);

    float pulse = focused ? 0.75f + 0.25f * std::sin(time * 6.0f) : 0.0f;
    float on = enabled ? 1.0f : 0.35f;
    vec4 fill = focused ? vec4(accent.r * 0.30f, accent.g * 0.30f, accent.b * 0.30f, 0.72f)
                        : vec4(0.04f, 0.05f, 0.12f, 0.66f);
    hud->roundRect(center, half, 10.0f, fill, withAlpha(accent, on * (focused ? 1.0f : 0.6f)),
                   focused ? 3.0f + pulse : 1.8f);
    float size = half.y * 0.78f;
    hud->text(FontId::Bold, label, center + vec2(0.0f, size * 0.36f), size, withAlpha(kWhite, on),
              Align::Center);
    return activated;
}

int Ui::selector(vec2 center, float halfWidth, const char* label, const char* value) {
    label = tr(label);
    value = tr(value);
    int index = count++;
    bool focused = index == focus;
    const float halfH = 30.0f;
    const vec2 half{halfWidth, halfH};
    const float arrow = 34.0f;
    vec2 leftC = center + vec2(halfWidth - 330.0f, 0.0f);
    vec2 rightC = center + vec2(halfWidth - arrow - 6.0f, 0.0f);

    int delta = 0;
    if (tapped(leftC, {arrow, halfH})) delta = -1;
    else if (tapped(rightC, {arrow, halfH})) delta = 1;
    else if (tapped(center, half)) delta = 1;
    if (delta != 0) focus = index;
    if (focused && in.left) delta = -1;
    if (focused && (in.right || in.accept)) delta = 1;

    vec4 border = focused ? kCyan : vec4(0.55f, 0.65f, 0.9f, 0.5f);
    hud->roundRect(center, half, 8.0f,
                   focused ? vec4(0.05f, 0.14f, 0.22f, 0.72f) : vec4(0.04f, 0.05f, 0.12f, 0.6f),
                   border, focused ? 2.6f : 1.4f);
    hud->text(FontId::Medium, label, center + vec2(-halfWidth + 22.0f, 9.0f), 23.0f,
              withAlpha(kWhite, 0.8f), Align::Left);
    // Chevrons.
    for (float dir : {-1.0f, 1.0f}) {
        vec2 c = dir < 0.0f ? leftC : rightC;
        hud->line(c + vec2(-6.0f * dir, -12.0f), c + vec2(7.0f * dir, 0.0f), 5.0f, kCyan);
        hud->line(c + vec2(7.0f * dir, 0.0f), c + vec2(-6.0f * dir, 12.0f), 5.0f, kCyan);
    }
    vec2 valueC = (leftC + rightC) * 0.5f;
    hud->text(FontId::Bold, value, valueC + vec2(0.0f, 9.0f), 24.0f, kWhite, Align::Center);
    return delta;
}

void Ui::title(vec2 pos, const char* str, float size) {
    str = tr(str);
    hud->text(FontId::Bold, str, pos, size, kWhite, Align::Center, 0.2f,
              vec4(1.0f, 0.2f, 0.85f, 0.85f));
}

void Ui::text(vec2 pos, const char* str, float size, vec4 color, Align align, FontId font) {
    str = tr(str);
    hud->text(font, str, pos, size, color, align);
}

void Ui::panel(vec2 center, vec2 half) {
    hud->roundRect(center, half, 14.0f, vec4(0.02f, 0.03f, 0.08f, 0.62f),
                   vec4(0.5f, 0.7f, 1.0f, 0.35f), 1.5f);
}

} // namespace game

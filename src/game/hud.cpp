#include "game/hud.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace game {

namespace {

using gfx::Align;
using gfx::Anchor;
using gfx::FontId;
using glm::vec2;
using glm::vec4;

const vec4 kWhite{1.0f, 1.0f, 1.0f, 1.0f};
const vec4 kCyan{0.25f, 0.92f, 1.0f, 1.0f};
const vec4 kMagenta{1.0f, 0.28f, 0.86f, 1.0f};
const vec4 kYellow{1.0f, 0.80f, 0.25f, 1.0f};
const vec4 kPanel{0.03f, 0.04f, 0.10f, 0.55f};

vec4 alpha(vec4 c, float a) { return {c.r, c.g, c.b, c.a * a}; }

void polyline(gfx::HudBatch& hud, std::initializer_list<vec2> points, float thickness,
              vec4 color, float glow = 0.0f) {
    const vec2* p = points.begin();
    for (size_t i = 0; i + 1 < points.size(); i++) hud.line(p[i], p[i + 1], thickness, color, glow);
}

// Nombre avec espace entre les milliers : 1268 devient « 1 268 ».
void formatThousands(char* out, size_t size, int value) {
    if (value >= 1000) std::snprintf(out, size, "%d %03d", value / 1000, value % 1000);
    else std::snprintf(out, size, "%d", value);
}

void drawStandings(gfx::HudBatch& hud, const HudState& s) {
    hud.anchor = Anchor::Left;
    char buf[32];

    hud.text(FontId::Medium, "POS", {34.0f, 104.0f}, 27.0f, alpha(kWhite, 0.92f));
    std::snprintf(buf, sizeof(buf), "%d", s.position);
    float w = hud.text(FontId::Bold, buf, {146.0f, 106.0f}, 37.0f, kMagenta, Align::Left, 0.2f,
                       alpha(kMagenta, 0.5f));
    std::snprintf(buf, sizeof(buf), " / %d", s.racers);
    hud.text(FontId::Bold, buf, {146.0f + w, 106.0f}, 37.0f, kWhite);
    hud.line({30.0f, 121.0f}, {248.0f, 121.0f}, 2.0f, alpha(kCyan, 0.85f), 0.5f);

    hud.text(FontId::Medium, "TOUR", {34.0f, 158.0f}, 27.0f, alpha(kWhite, 0.92f));
    std::snprintf(buf, sizeof(buf), "%d / %d", s.lap, s.laps);
    hud.text(FontId::Bold, buf, {146.0f, 160.0f}, 37.0f, kWhite);
    hud.line({30.0f, 174.0f}, {236.0f, 174.0f}, 2.0f, alpha(kCyan, 0.85f), 0.5f);
}

void drawEnergy(gfx::HudBatch& hud, const HudState& s) {
    hud.anchor = Anchor::Center;
    const float top = 40.0f, bottom = 86.0f, mid = 63.0f;
    const vec2 a{498.0f, top}, b{1146.0f, top}, c{1132.0f, bottom}, d{498.0f, bottom},
        tip{480.0f, mid};
    hud.quad(a, b, c, d, kPanel);
    hud.tri(a, d, tip, kPanel);
    polyline(hud, {a, b, c, d, tip, a}, 2.0f, kCyan, 0.35f);

    // Éclair.
    hud.tri({529.0f, 45.0f}, {511.0f, 66.0f}, {523.0f, 66.0f}, kCyan);
    hud.tri({516.0f, 82.0f}, {534.0f, 60.0f}, {521.0f, 60.0f}, kCyan);

    hud.text(FontId::Medium, "ÉNERGIE", {550.0f, 71.0f}, 22.0f, kWhite);

    // Jauge en segments inclinés.
    const int segments = 15;
    const float x0 = 680.0f, x1 = 1036.0f;
    const float pitch = (x1 - x0) / segments;
    const float width = pitch - 4.5f, skew = 7.0f;
    float filled = std::clamp(s.energy, 0.0f, 1.0f) * segments;
    for (int i = 0; i < segments; i++) {
        float x = x0 + pitch * static_cast<float>(i);
        float level = std::clamp(filled - static_cast<float>(i), 0.0f, 1.0f);
        vec4 color = glm::mix(vec4(0.12f, 0.35f, 0.55f, 0.30f), vec4(0.15f, 0.92f, 1.0f, 1.0f),
                              level);
        hud.quad({x + skew, 52.0f}, {x + skew + width, 52.0f}, {x + width, 74.0f}, {x, 74.0f},
                 color);
    }

    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d %%", static_cast<int>(std::lround(s.energy * 100.0f)));
    hud.text(FontId::Medium, buf, {1058.0f, 71.0f}, 22.0f, kWhite);
}

void drawSpeed(gfx::HudBatch& hud, const HudState& s) {
    hud.anchor = Anchor::Center;
    const vec2 a{560.0f, 782.0f}, b{1086.0f, 782.0f}, c{1040.0f, 852.0f}, d{514.0f, 852.0f};
    hud.quad(a, b, c, d, kPanel);
    polyline(hud, {a, b, c, d, a}, 2.0f, kCyan, 0.35f);

    hud.quad({545.0f, 826.0f}, {612.0f, 826.0f}, {600.0f, 842.0f}, {533.0f, 842.0f}, kCyan);
    for (int i = 0; i < 4; i++) {
        float x = 985.0f + 17.0f * static_cast<float>(i);
        hud.quad({x + 10.0f, 825.0f}, {x + 20.0f, 825.0f}, {x + 10.0f, 842.0f}, {x, 842.0f},
                 kCyan);
    }

    char buf[32];
    formatThousands(buf, sizeof(buf), static_cast<int>(std::lround(s.speedKmh)));
    hud.text(FontId::Bold, buf, {848.0f, 836.0f}, 54.0f, kWhite, Align::Right, 0.2f,
             alpha(kCyan, 0.35f));
    hud.text(FontId::Medium, "km/h", {862.0f, 836.0f}, 24.0f, kWhite);
}

void drawMinimap(gfx::HudBatch& hud, const HudState& s) {
    hud.anchor = Anchor::Right;
    const vec2 center{1476.0f, 140.0f};
    hud.roundRect(center, {92.0f, 76.0f}, 14.0f, kPanel, vec4(0.85f, 0.88f, 1.0f, 0.55f), 1.5f);
    if (!s.minimap || s.minimap->size() < 2) return;

    const std::vector<vec2>& pts = *s.minimap;
    const vec2 half{70.0f, 54.0f};
    const size_t n = pts.size();
    for (size_t i = 0; i < n; i++) {
        float t = static_cast<float>(i) / static_cast<float>(n);
        vec4 color = glm::mix(kCyan, kMagenta, 0.5f + 0.5f * std::sin(t * 12.566f + 1.0f));
        color = glm::mix(color, kCyan, 0.55f);
        hud.line(center + pts[i] * half, center + pts[(i + 1) % n] * half, 4.0f, color, 0.12f);
    }

    // Marqueur du joueur.
    float f = std::clamp(s.progress, 0.0f, 0.9999f) * static_cast<float>(n);
    size_t i0 = static_cast<size_t>(f);
    vec2 p0 = pts[i0], p1 = pts[(i0 + 1) % n];
    vec2 pos = center + glm::mix(p0, p1, f - std::floor(f)) * half;
    vec2 dir = glm::normalize((p1 - p0) * half);
    vec2 side{-dir.y, dir.x};
    hud.tri(pos + dir * 9.0f, pos - dir * 6.0f + side * 6.0f, pos - dir * 6.0f - side * 6.0f,
            kYellow);
}

void drawButtons(gfx::HudBatch& hud) {
    const vec4 glass{0.10f, 0.12f, 0.24f, 0.34f};

    hud.anchor = Anchor::Left;
    for (float dirSign : {-1.0f, 1.0f}) {
        vec2 c{dirSign < 0.0f ? 113.0f : 277.0f, 757.0f};
        hud.circle(c, 70.0f, glass, alpha(kWhite, 0.85f), 2.5f);
        polyline(hud,
                 {c + vec2(-13.0f * dirSign, -28.0f), c + vec2(15.0f * dirSign, 0.0f),
                  c + vec2(-13.0f * dirSign, 28.0f)},
                 9.0f, kWhite);
    }

    hud.anchor = Anchor::Right;
    // Boost.
    {
        vec2 c{1427.0f, 742.0f};
        hud.circle(c, 96.0f, vec4(0.10f, 0.07f, 0.03f, 0.5f), kYellow, 5.0f);
        polyline(hud, {c + vec2(-32.0f, -22.0f), c + vec2(0.0f, -50.0f), c + vec2(32.0f, -22.0f)},
                 10.0f, kYellow, 0.4f);
        polyline(hud, {c + vec2(-32.0f, 2.0f), c + vec2(0.0f, -26.0f), c + vec2(32.0f, 2.0f)},
                 10.0f, kYellow, 0.4f);
        hud.text(FontId::Bold, "BOOST", c + vec2(0.0f, 46.0f), 28.0f, kWhite, Align::Center);
    }
    // Missile.
    {
        vec2 c{1316.0f, 588.0f};
        const vec4 pink{1.0f, 0.45f, 0.85f, 1.0f};
        hud.circle(c, 60.0f, vec4(0.10f, 0.03f, 0.09f, 0.5f), pink, 3.0f);
        hud.line(c + vec2(-16.0f, 14.0f), c + vec2(14.0f, -14.0f), 16.0f, pink);
        hud.line(c + vec2(14.0f, -14.0f), c + vec2(24.0f, -24.0f), 8.0f, kWhite);
        hud.line(c + vec2(-22.0f, 6.0f), c + vec2(-8.0f, 22.0f), 6.0f, kWhite);
        hud.line(c + vec2(-24.0f, 22.0f), c + vec2(-33.0f, 31.0f), 6.0f, alpha(pink, 0.8f));
    }
    // Bouclier.
    {
        vec2 c{1476.0f, 563.0f};
        hud.circle(c, 58.0f, vec4(0.03f, 0.08f, 0.12f, 0.5f), kCyan, 3.0f);
        const vec2 topP = c + vec2(0.0f, -30.0f), tr = c + vec2(26.0f, -20.0f),
                   br = c + vec2(26.0f, 3.0f), bot = c + vec2(0.0f, 31.0f),
                   bl = c + vec2(-26.0f, 3.0f), tl = c + vec2(-26.0f, -20.0f);
        hud.quad(topP + vec2(0.0f, 9.0f), tr + vec2(-8.0f, 6.0f), br + vec2(-8.0f, -3.0f),
                 bot + vec2(0.0f, -12.0f), alpha(kCyan, 0.55f));
        polyline(hud, {topP, tr, br, bot, bl, tl, topP}, 4.5f, kCyan, 0.3f);
    }
}

void drawNameTags(gfx::HudBatch& hud, const HudState& s, const glm::mat4& viewProj) {
    // Positions calculées en pixels, puis ramenées au repère de référence.
    hud.anchor = Anchor::Left;
    for (int i = 0; i < s.tagCount; i++) {
        const NameTag& tag = s.tags[i];
        glm::vec4 clip = viewProj * glm::vec4(tag.worldPos, 1.0f);
        if (clip.w <= 0.5f) continue;
        vec2 ndc = vec2(clip.x, clip.y) / clip.w;
        if (std::abs(ndc.x) > 1.05f || std::abs(ndc.y) > 1.05f) continue;
        vec2 p = (ndc * 0.5f + 0.5f) * hud.screen / hud.scale;

        hud.tri(p + vec2(-8.0f, -13.0f), p + vec2(8.0f, -13.0f), p + vec2(0.0f, -2.0f), tag.color);

        const float h = 27.0f, skew = 6.0f, rankW = 30.0f;
        float nameW = hud.textWidth(FontId::Medium, tag.name, 16.0f);
        float w = rankW + nameW + 22.0f;
        float x0 = p.x - w * 0.5f, x1 = p.x + w * 0.5f;
        float y1 = p.y - 19.0f, y0 = y1 - h;
        hud.quad({x0 + skew, y0}, {x1 + skew, y0}, {x1, y1}, {x0, y1},
                 vec4(0.07f, 0.05f, 0.14f, 0.82f));
        hud.quad({x0 + skew, y0}, {x0 + skew + rankW, y0}, {x0 + rankW, y1}, {x0, y1},
                 vec4(0.30f, 0.10f, 0.36f, 0.95f));
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%d", tag.rank);
        hud.text(FontId::Bold, buf, {x0 + rankW * 0.5f + 3.0f, y1 - 7.0f}, 18.0f, kWhite,
                 Align::Center);
        hud.text(FontId::Medium, tag.name, {x0 + rankW + 11.0f, y1 - 8.0f}, 16.0f, kWhite);
    }
}

} // namespace

void buildHud(gfx::HudBatch& hud, const HudState& state, const glm::mat4& viewProj) {
    drawNameTags(hud, state, viewProj);
    drawStandings(hud, state);
    drawEnergy(hud, state);
    drawSpeed(hud, state);
    drawMinimap(hud, state);
    drawButtons(hud);
}

} // namespace game

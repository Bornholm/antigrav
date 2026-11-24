#include "game/hud.h"

#include "game/i18n.h"

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
    hud.vAnchor = gfx::VAnchor::Top;
    char buf[32];

    hud.text(FontId::Medium, tr("POS"), {34.0f, 104.0f}, 27.0f, alpha(kWhite, 0.92f));
    std::snprintf(buf, sizeof(buf), "%d", s.position);
    float w = hud.text(FontId::Bold, buf, {146.0f, 106.0f}, 37.0f, kMagenta, Align::Left, 0.2f,
                       alpha(kMagenta, 0.5f));
    std::snprintf(buf, sizeof(buf), " / %d", s.racers);
    hud.text(FontId::Bold, buf, {146.0f + w, 106.0f}, 37.0f, kWhite);
    hud.line({30.0f, 121.0f}, {248.0f, 121.0f}, 2.0f, alpha(kCyan, 0.85f), 0.5f);

    hud.text(FontId::Medium, tr("FIN"), {34.0f, 158.0f}, 27.0f, alpha(kWhite, 0.92f));
    const int left = static_cast<int>(std::ceil(s.remaining));
    std::snprintf(buf, sizeof(buf), "%d:%02d", left / 60, left % 60);
    hud.text(FontId::Bold, buf, {146.0f, 160.0f}, 37.0f, kWhite);
    hud.line({30.0f, 174.0f}, {236.0f, 174.0f}, 2.0f, alpha(kCyan, 0.85f), 0.5f);
}

void drawEnergy(gfx::HudBatch& hud, const HudState& s) {
    hud.anchor = Anchor::Center;
    hud.vAnchor = gfx::VAnchor::Top;
    const float top = 40.0f, bottom = 86.0f, mid = 63.0f;
    const vec2 a{498.0f, top}, b{1146.0f, top}, c{1132.0f, bottom}, d{498.0f, bottom},
        tip{480.0f, mid};
    hud.quad(a, b, c, d, kPanel);
    hud.tri(a, d, tip, kPanel);
    polyline(hud, {a, b, c, d, tip, a}, 2.0f, kCyan, 0.35f);

    // Éclair.
    hud.tri({529.0f, 45.0f}, {511.0f, 66.0f}, {523.0f, 66.0f}, kCyan);
    hud.tri({516.0f, 82.0f}, {534.0f, 60.0f}, {521.0f, 60.0f}, kCyan);

    hud.text(FontId::Medium, tr("ÉNERGIE"), {550.0f, 71.0f}, 22.0f, kWhite);

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
    hud.vAnchor = gfx::VAnchor::Bottom;
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
    hud.vAnchor = gfx::VAnchor::Top;
    const vec2 center{1476.0f, 140.0f};
    hud.roundRect(center, {92.0f, 76.0f}, 14.0f, kPanel, vec4(0.85f, 0.88f, 1.0f, 0.55f), 1.5f);
    if (!s.minimap || s.minimap->size() < 2) return;

    const std::vector<vec2>& pts = *s.minimap;
    const vec2 half{70.0f, 54.0f};
    const size_t n = pts.size();
    auto inside = [&](vec2 p) { return std::abs(p.x) <= 1.0f && std::abs(p.y) <= 1.0f; };
    for (size_t i = 0; i + 1 < n; i++) {
        if (!inside(pts[i]) || !inside(pts[i + 1])) continue;
        float t = static_cast<float>(i) / static_cast<float>(n);
        vec4 color = glm::mix(kMagenta, kCyan, 0.35f + 0.65f * t);
        hud.line(center + pts[i] * half, center + pts[i + 1] * half, 4.0f, color, 0.12f);
    }

    // Marqueur du joueur.
    float f = std::clamp(s.progress, 0.0f, 0.9999f) * static_cast<float>(n - 1);
    size_t i0 = std::min(static_cast<size_t>(f), n - 2);
    vec2 p0 = pts[i0], p1 = pts[i0 + 1];
    vec2 pos = center + glm::mix(p0, p1, f - std::floor(f)) * half;
    vec2 dir = glm::normalize((p1 - p0) * half);
    vec2 side{-dir.y, dir.x};
    hud.tri(pos + dir * 9.0f, pos - dir * 6.0f + side * 6.0f, pos - dir * 6.0f - side * 6.0f,
            kYellow);
}

// Barre du morceau : où en est chaque vaisseau entre la première note et la
// dernière.
void drawSongBar(gfx::HudBatch& hud, const HudState& s) {
    hud.anchor = Anchor::Center;
    hud.vAnchor = gfx::VAnchor::Top;
    const float x0 = 560.0f, x1 = 1040.0f, y = 96.0f;
    hud.line({x0, y}, {x1, y}, 3.0f, alpha(kWhite, 0.28f));
    for (int i = 0; i < s.markCount; i++) {
        if (i == s.localMark) continue;
        const float x = x0 + (x1 - x0) * s.marks[i];
        hud.line({x, y - 6.0f}, {x, y + 6.0f}, 3.0f, alpha(s.markColors[i], 0.9f));
    }
    if (s.localMark < s.markCount) {
        const float x = x0 + (x1 - x0) * s.marks[s.localMark];
        hud.line({x0, y}, {x, y}, 3.0f, kCyan, 0.4f);
        hud.tri({x, y - 4.0f}, {x - 7.0f, y - 15.0f}, {x + 7.0f, y - 15.0f}, kYellow);
    }
}

void drawButtons(gfx::HudBatch& hud, const HudState& s) {
    // Les formes viennent de buttonShape, en pixels : les mêmes que celles
    // testées au toucher. On les ramène au repère de référence.
    hud.anchor = Anchor::Left;
    hud.vAnchor = gfx::VAnchor::Top;
    auto shapeOf = [&](Button b, vec2& c, float& r) {
        ButtonShape shape = buttonShape(b, hud.screen);
        c = shape.center / hud.scale;
        r = shape.radius / hud.scale;
        return s.pressed[static_cast<int>(b)];
    };
    vec2 c;
    float r = 0.0f;

    for (Button b : {Button::Left, Button::Right}) {
        bool down = shapeOf(b, c, r);
        float dirSign = b == Button::Left ? -1.0f : 1.0f;
        vec4 glass = down ? vec4(0.30f, 0.60f, 0.85f, 0.45f) : vec4(0.10f, 0.12f, 0.24f, 0.34f);
        hud.circle(c, r, glass, alpha(down ? kCyan : kWhite, 0.9f), down ? 4.0f : 2.5f);
        float k = r / 70.0f; // la flèche suit la taille du bouton
        polyline(hud,
                 {c + vec2(-13.0f * dirSign, -28.0f) * k, c + vec2(15.0f * dirSign, 0.0f) * k,
                  c + vec2(-13.0f * dirSign, 28.0f) * k},
                 9.0f * k, kWhite);
    }
    {
        bool down = shapeOf(Button::Boost, c, r) || s.boosting;
        // Pendant le délai entre deux accords, le bouton s'éteint et se
        // rallume peu à peu : un appui n'y donnerait pas d'accord.
        const bool ready = s.cooldown <= 0.0f;
        const vec4 rim = ready ? kYellow : alpha(kYellow, 0.35f);
        vec4 fill = down ? vec4(0.55f, 0.38f, 0.05f, 0.6f) : vec4(0.10f, 0.07f, 0.03f, 0.5f);
        hud.circle(c, r, fill, rim, down ? 7.0f : 5.0f);
        if (!ready) hud.circle(c, r * (1.0f - s.cooldown), alpha(kYellow, 0.10f), alpha(kYellow, 0.5f), 2.0f);
        polyline(hud, {c + vec2(-32.0f, -22.0f), c + vec2(0.0f, -50.0f), c + vec2(32.0f, -22.0f)},
                 10.0f, kYellow, 0.4f);
        polyline(hud, {c + vec2(-32.0f, 2.0f), c + vec2(0.0f, -26.0f), c + vec2(32.0f, 2.0f)},
                 10.0f, kYellow, 0.4f);
        hud.text(FontId::Bold, tr("BOOST"), c + vec2(0.0f, 46.0f), 28.0f, kWhite, Align::Center);
    }
}

// Retour de rythme, accroché au vaisseau du joueur : c'est là qu'il regarde.
// Un anneau se referme sur le vaisseau à chaque temps, les pastilles de la
// série sont dessous, et le verdict de l'appui s'affiche dessus.
void drawRhythm(gfx::HudBatch& hud, const HudState& s, const glm::mat4& viewProj) {
    if (s.beatPhase < 0.0f) return;
    const glm::vec4 clip = viewProj * glm::vec4(s.shipPos, 1.0f);
    if (clip.w <= 0.5f) return;
    hud.anchor = Anchor::Left;
    hud.vAnchor = gfx::VAnchor::Top;
    const vec2 c = (vec2(clip.x, clip.y) / clip.w * 0.5f + 0.5f) * hud.screen / hud.scale;
    const bool ready = s.cooldown <= 0.0f;
    const float core = 74.0f;

    // Cible fixe, puis anneau qui s'y referme. Atténués pendant le délai
    // entre deux accords : un appui n'y compterait pas.
    const float near = 1.0f - s.beatPhase;
    hud.circle(c, core, vec4(0.0f), alpha(kYellow, ready ? 0.55f : 0.18f), 2.5f);
    hud.circle(c, core * (1.0f + 1.1f * s.beatPhase), vec4(0.0f),
               alpha(kWhite, ready ? 0.25f + 0.65f * near * near : 0.10f + 0.12f * near), ready ? 4.0f : 2.0f);
    if (!ready) hud.circle(c, core * (1.0f - s.cooldown), vec4(0.0f), alpha(kYellow, 0.35f), 2.0f);

    // Série d'accords : la décharge part quand les pastilles sont toutes allumées.
    const float step = 26.0f, x0 = c.x - step * 0.5f * static_cast<float>(s.comboSize - 1);
    for (int i = 0; i < s.comboSize; i++) {
        const vec2 p{x0 + step * static_cast<float>(i), c.y + core + 26.0f};
        const bool lit = i < s.combo;
        hud.circle(p, 7.5f, lit ? kMagenta : vec4(0.10f, 0.03f, 0.09f, 0.45f), alpha(kMagenta, lit ? 1.0f : 0.45f), 2.0f);
    }

    const vec2 label = c + vec2(0.0f, -core - 44.0f);
    if (s.accord > 0.0f) {
        hud.text(FontId::Bold, tr("EN RYTHME"), label, 30.0f + 8.0f * s.accord, alpha(kYellow, 0.4f + 0.6f * s.accord),
                 Align::Center, 0.2f, alpha(kYellow, 0.4f * s.accord));
    } else if (s.miss != 0) {
        hud.text(FontId::Bold, tr(s.miss == 1 ? "TROP TÔT" : "TROP TARD"), label, 28.0f,
                 alpha(vec4(1.0f, 0.45f, 0.35f, 1.0f), s.missFade), Align::Center);
    }
    if (s.incoming)
        hud.text(FontId::Bold, tr("DÉCHARGE ! PAREZ EN RYTHME"), label + vec2(0.0f, -40.0f), 28.0f, kMagenta, Align::Center,
                 0.2f, alpha(kMagenta, 0.5f));
}

void drawNameTags(gfx::HudBatch& hud, const HudState& s, const glm::mat4& viewProj) {
    // Positions calculées en pixels, puis ramenées au repère de référence.
    hud.anchor = Anchor::Left;
    hud.vAnchor = gfx::VAnchor::Top;
    const float h = 27.0f, skew = 6.0f, rankW = 30.0f;

    struct Placed {
        float x0, x1, y1;
    };
    Placed placed[8];
    int placedCount = 0;

    // Du plus proche au plus lointain : en cas de chevauchement, c'est
    // l'étiquette du plus lointain qui monte.
    int order[8];
    float depth[8];
    int count = 0;
    for (int i = 0; i < s.tagCount; i++) {
        glm::vec4 clip = viewProj * glm::vec4(s.tags[i].worldPos, 1.0f);
        if (clip.w <= 0.5f) continue;
        vec2 ndc = vec2(clip.x, clip.y) / clip.w;
        if (std::abs(ndc.x) > 1.05f || std::abs(ndc.y) > 1.05f) continue;
        int k = count++;
        while (k > 0 && depth[k - 1] > clip.w) {
            order[k] = order[k - 1];
            depth[k] = depth[k - 1];
            k--;
        }
        order[k] = i;
        depth[k] = clip.w;
    }

    for (int n = 0; n < count; n++) {
        const NameTag& tag = s.tags[order[n]];
        glm::vec4 clip = viewProj * glm::vec4(tag.worldPos, 1.0f);
        vec2 p = (vec2(clip.x, clip.y) / clip.w * 0.5f + 0.5f) * hud.screen / hud.scale;

        float nameW = hud.textWidth(FontId::Medium, tag.name, 16.0f);
        float w = rankW + nameW + 22.0f;
        float x0 = p.x - w * 0.5f, x1 = p.x + w * 0.5f;
        float y1 = p.y - 19.0f;
        for (int pass = 0; pass < 4; pass++)
            for (int k = 0; k < placedCount; k++)
                if (x0 < placed[k].x1 + 8.0f && x1 > placed[k].x0 - 8.0f &&
                    std::abs(y1 - placed[k].y1) < h + 4.0f)
                    y1 = placed[k].y1 - h - 5.0f;
        float y0 = y1 - h;
        placed[placedCount++] = {x0, x1, y1};

        hud.tri(p + vec2(-8.0f, -13.0f), p + vec2(8.0f, -13.0f), p + vec2(0.0f, -2.0f), tag.color);
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

void drawRaceFlow(gfx::HudBatch& hud, const HudState& s) {
    char buf[48];

    // Chrono sous le compteur de tours.
    hud.anchor = Anchor::Left;
    hud.vAnchor = gfx::VAnchor::Top;
    int centis = static_cast<int>(s.raceTime * 100.0f);
    std::snprintf(buf, sizeof(buf), "%d:%02d.%02d", centis / 6000, (centis / 100) % 60,
                  centis % 100);
    hud.text(FontId::Medium, tr("TEMPS"), {34.0f, 208.0f}, 20.0f, alpha(kWhite, 0.75f));
    hud.text(FontId::Bold, buf, {146.0f, 209.0f}, 24.0f, kWhite);
    if (s.showGap) {
        // Avance sur la musique : vert devant, rose derrière.
        std::snprintf(buf, sizeof(buf), "%+.1f s", s.gap);
        hud.text(FontId::Medium, tr("ÉCART"), {34.0f, 244.0f}, 20.0f, alpha(kWhite, 0.75f));
        hud.text(FontId::Bold, buf, {146.0f, 245.0f}, 24.0f, s.gap >= 0.0f ? vec4(0.45f, 1.0f, 0.5f, 1.0f) : kMagenta);
    }
    // Armure : la jauge peut encaisser une barrière.
    hud.text(FontId::Medium, tr("ARMURE"), {34.0f, 280.0f}, 20.0f, alpha(kWhite, 0.75f));
    hud.text(FontId::Bold, tr(s.armored ? "PRÊTE" : "VIDE"), {146.0f, 281.0f}, 24.0f, s.armored ? kCyan : alpha(kWhite, 0.4f));

    hud.anchor = Anchor::Center;
    hud.vAnchor = gfx::VAnchor::Middle;
    if (s.countdown > 0.0f) {
        std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::ceil(s.countdown)));
        float pulse = 1.0f + 0.25f * (s.countdown - std::floor(s.countdown));
        hud.text(FontId::Bold, buf, {800.0f, 400.0f}, 190.0f * pulse, kWhite, Align::Center,
                 0.2f, alpha(kCyan, 0.8f));
    } else if (s.gridStart && s.raceTime < 1.2f && !s.finished) {
        hud.text(FontId::Bold, tr("GO !"), {800.0f, 400.0f}, 170.0f,
                 alpha(kYellow, 1.0f - s.raceTime / 1.2f), Align::Center, 0.2f);
    }

    if (s.statsLine[0]) {
        hud.anchor = Anchor::Center;
        hud.vAnchor = gfx::VAnchor::Bottom;
        hud.text(FontId::Medium, s.statsLine, {800.0f, 884.0f}, 15.0f, alpha(kWhite, 0.85f),
                 Align::Center, 0.0f);
    }

    hud.anchor = Anchor::Center;
    hud.vAnchor = gfx::VAnchor::Middle;
    if (s.finished) {
        hud.quad({0.0f, 318.0f}, {1600.0f, 318.0f}, {1600.0f, 482.0f}, {0.0f, 482.0f},
                 vec4(0.02f, 0.02f, 0.08f, 0.72f));
        hud.line({200.0f, 318.0f}, {1400.0f, 318.0f}, 2.0f, kCyan, 0.5f);
        hud.line({200.0f, 482.0f}, {1400.0f, 482.0f}, 2.0f, kMagenta, 0.5f);
        hud.text(FontId::Bold, tr("COURSE TERMINÉE"), {800.0f, 392.0f}, 58.0f, kWhite, Align::Center,
                 0.2f, alpha(kCyan, 0.6f));
        std::snprintf(buf, sizeof(buf), tr("POSITION %d / %d"), s.position, s.racers);
        hud.text(FontId::Medium, buf, {800.0f, 448.0f}, 34.0f, kYellow, Align::Center);
    }
}

void buildHud(gfx::HudBatch& hud, const HudState& state, const glm::mat4& viewProj) {
    drawNameTags(hud, state, viewProj);
    drawStandings(hud, state);
    drawEnergy(hud, state);
    drawSpeed(hud, state);
    drawMinimap(hud, state);
    drawSongBar(hud, state);
    drawButtons(hud, state);
    drawRhythm(hud, state, viewProj);
    drawRaceFlow(hud, state);
}

} // namespace game

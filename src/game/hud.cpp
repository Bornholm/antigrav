#include "game/hud.h"

#include "game/i18n.h"
#include "sim/world.h"

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
    const float x0 = 440.0f, x1 = 1120.0f, y = 62.0f;
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

// Couleur et nom de chaque bonus, dans l'ordre de sim::Item.
const char* const kItemNames[] = {"", "TIR", "MINE", "ONDE", "TURBO", "BOUCLIER", "COMÈTE"};
const vec4 kItemColors[] = {
    kWhite, vec4(1.0f, 0.35f, 0.75f, 1.0f), vec4(1.0f, 0.30f, 0.20f, 1.0f),
    vec4(0.55f, 0.45f, 1.0f, 1.0f), kYellow, kCyan, vec4(1.0f, 0.55f, 0.15f, 1.0f),
};

// Aide au départ turbo, dessinée sur le bouton du bonus (ou sur le cadre du
// bonus au clavier). Un anneau se resserre sur le bouton pendant le compte à
// rebours et le touche à l'ouverture de la fenêtre : le bouton s'allume, c'est
// le moment d'appuyer. Rend vrai quand l'aide est affichée.
bool drawStartCue(gfx::HudBatch& hud, const HudState& s, vec2 c, vec2 half, float corner) {
    if (!s.gridStart || s.countdown <= 0.0f) return false;
    const float lead = 1.2f; // l'anneau apparaît 1,2 s avant la fenêtre
    const float untilWindow = s.countdown - sim::kStartWindow;
    const vec4 dark{0.06f, 0.05f, 0.09f, 0.5f};
    auto label = [&](const char* text, vec4 glow) {
        hud.text(FontId::Bold, tr(text), c + vec2(0.0f, 12.0f), 30.0f, kWhite, Align::Center, 0.2f, glow);
    };
    if (s.startBoost == 2) {
        hud.roundRect(c, half, corner, alpha(kMagenta, 0.25f), kMagenta, 5.0f);
        label("RATÉ", alpha(kMagenta, 0.7f));
        return true;
    }
    if (s.startBoost == 1 || untilWindow <= 0.0f) {
        // Fenêtre ouverte, ou appui déjà réussi : le bouton brille.
        const float beat = 0.5f + 0.5f * std::sin(s.countdown * 40.0f);
        hud.roundRect(c, half, corner, alpha(kCyan, 0.40f + 0.15f * beat), kCyan, 8.0f);
        const float halo = 0.14f * std::min(half.x, half.y);
        hud.roundRect(c, half + halo, corner + halo, vec4(0.0f), alpha(kCyan, 0.5f + 0.3f * beat), 3.0f);
        label("TURBO", alpha(kCyan, 0.9f));
        return true;
    }
    if (untilWindow > lead) return false;
    // L'anneau approche : il part loin du bord du bouton, d'une marge qui
    // dépend de sa plus petite dimension, et s'en rapproche.
    const float t = untilWindow / lead;
    const float grow = 0.9f * t * std::min(half.x, half.y);
    hud.roundRect(c, half, corner, dark, alpha(kCyan, 0.5f), 2.5f);
    hud.roundRect(c, half + grow, corner + grow, vec4(0.0f), alpha(kCyan, 0.35f + 0.65f * (1.0f - t)), 4.0f);
    return true;
}

// Au clavier ou à la manette, pas de boutons à l'écran : seulement le bonus
// porté, en bas à droite.
void drawItemBadge(gfx::HudBatch& hud, const HudState& s) {
    hud.anchor = Anchor::Right;
    hud.vAnchor = gfx::VAnchor::Bottom;
    const vec2 c{1440.0f, 800.0f};
    if (drawStartCue(hud, s, c, {120.0f, 40.0f}, 14.0f)) return;
    const int item = std::clamp(s.item, 0, 6);
    if (!item) return;
    hud.roundRect(c, {120.0f, 40.0f}, 14.0f, vec4(0.05f, 0.04f, 0.09f, 0.6f), kItemColors[item], 3.0f);
    hud.text(FontId::Bold, tr(kItemNames[item]), c + vec2(0.0f, 12.0f), 32.0f, kWhite, Align::Center, 0.2f,
             alpha(kItemColors[item], 0.6f));
}

void drawButtons(gfx::HudBatch& hud, const HudState& s) {
    if (!touchInterface()) {
        drawItemBadge(hud, s);
        return;
    }
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
        // Bouton du bonus : il porte le nom de ce qu'on tient, et s'éteint
        // quand on n'a rien.
        const bool down = shapeOf(Button::Use, c, r);
        const int item = std::clamp(s.item, 0, 6);
        const vec4 tint = item ? kItemColors[item] : alpha(kWhite, 0.35f);
        if (drawStartCue(hud, s, c, {r, r}, r)) return;
        hud.circle(c, r, down && item ? alpha(tint, 0.35f) : vec4(0.06f, 0.05f, 0.09f, 0.5f), tint, item ? (down ? 7.0f : 5.0f) : 2.5f);
        if (item) {
            hud.circle(c, r * 0.78f, vec4(0.0f), alpha(tint, 0.45f), 2.0f);
            hud.text(FontId::Bold, tr(kItemNames[item]), c + vec2(0.0f, 12.0f), item >= 5 ? 26.0f : 34.0f, kWhite, Align::Center, 0.2f,
                     alpha(tint, 0.6f));
        }
    }
}

// Alerte accrochée au vaisseau du joueur : c'est là qu'il regarde.
void drawAlerts(gfx::HudBatch& hud, const HudState& s, const glm::mat4& viewProj) {
    if (!s.incoming) return;
    const glm::vec4 clip = viewProj * glm::vec4(s.shipPos, 1.0f);
    if (clip.w <= 0.5f) return;
    hud.anchor = Anchor::Left;
    hud.vAnchor = gfx::VAnchor::Top;
    const vec2 c = (vec2(clip.x, clip.y) / clip.w * 0.5f + 0.5f) * hud.screen / hud.scale;
    hud.text(FontId::Bold, tr(s.shielded ? "TIR EN APPROCHE" : "TIR EN APPROCHE !"), c + vec2(0.0f, -120.0f), 28.0f,
             s.shielded ? kCyan : kMagenta, Align::Center, 0.2f, alpha(s.shielded ? kCyan : kMagenta, 0.5f));
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
        const float boxW = tag.rank > 0 ? rankW : 0.0f;
        float w = boxW + nameW + 22.0f;
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
        // Sans rang, comme le fantôme : pas de case de place.
        if (tag.rank > 0) {
            hud.quad({x0 + skew, y0}, {x0 + skew + rankW, y0}, {x0 + rankW, y1}, {x0, y1},
                     vec4(0.30f, 0.10f, 0.36f, 0.95f));
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d", tag.rank);
            hud.text(FontId::Bold, buf, {x0 + rankW * 0.5f + 3.0f, y1 - 7.0f}, 18.0f, kWhite,
                     Align::Center);
        }
        hud.text(FontId::Medium, tag.name, {x0 + boxW + 11.0f, y1 - 8.0f}, 16.0f, alpha(kWhite, tag.color.a));
    }
}

// Conseil de l'initiation, en haut au centre : sous la barre du morceau,
// au-dessus du compte à rebours et du vaisseau.
void drawTip(gfx::HudBatch& hud, const HudState& s) {
    if (s.tipLevel <= 0.01f || s.finished) return;
    hud.anchor = Anchor::Center;
    hud.vAnchor = gfx::VAnchor::Top;
    const float a = s.tipLevel;
    const bool detail = s.tipDetail[0] != '\0';
    const float width = std::max({hud.textWidth(FontId::Bold, s.tipTitle, 26.0f), hud.textWidth(FontId::Medium, s.tipBody, 25.0f),
                                  detail ? hud.textWidth(FontId::Medium, s.tipDetail, 20.0f) : 0.0f});
    const vec2 c{800.0f, detail ? 166.0f : 152.0f};
    const vec2 half{std::min(540.0f, width * 0.5f + 36.0f), detail ? 64.0f : 50.0f};
    hud.roundRect(c, half, 14.0f, vec4(0.03f, 0.04f, 0.10f, 0.72f * a), alpha(kCyan, 0.8f * a), 2.0f);
    float y = c.y - half.y + 36.0f;
    hud.text(FontId::Bold, s.tipTitle, {800.0f, y}, 26.0f, alpha(kCyan, a), Align::Center, 0.2f, alpha(kCyan, 0.4f * a));
    y += 34.0f;
    hud.text(FontId::Medium, s.tipBody, {800.0f, y}, 25.0f, alpha(kWhite, a), Align::Center);
    if (detail) hud.text(FontId::Medium, s.tipDetail, {800.0f, y + 30.0f}, 20.0f, alpha(kWhite, 0.75f * a), Align::Center);
}

// Temps de course en minutes, secondes et centièmes.
void formatTime(char* out, size_t size, float seconds) {
    const int centis = static_cast<int>(seconds * 100.0f); // tronqué, comme le chrono
    std::snprintf(out, size, "%d:%02d.%02d", centis / 6000, (centis / 100) % 60, centis % 100);
}

} // namespace

void drawRaceFlow(gfx::HudBatch& hud, const HudState& s) {
    char buf[48];

    // Voile de couleur : un bonus ramassé, un choc encaissé.
    if (s.flash.a > 0.01f) {
        const vec2 size = hud.screen / hud.scale;
        hud.anchor = Anchor::Left;
        hud.vAnchor = gfx::VAnchor::Top;
        hud.quad({0.0f, 0.0f}, {size.x, 0.0f}, {size.x, size.y}, {0.0f, size.y}, vec4(s.flash.r, s.flash.g, s.flash.b, s.flash.a));
    }

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
    if (s.showGhostGap) {
        // Écart avec le fantôme : vert en avance, rose en retard.
        const float y = s.showGap ? 280.0f : 244.0f;
        std::snprintf(buf, sizeof(buf), "%+.2f s", s.ghostGap);
        // Le libellé tient avant la colonne des valeurs, quelle que soit la langue.
        const float fit = std::min(20.0f, 20.0f * 104.0f / std::max(1.0f, hud.textWidth(FontId::Medium, tr("FANTÔME"), 20.0f)));
        hud.text(FontId::Medium, tr("FANTÔME"), {34.0f, y}, fit, alpha(kWhite, 0.75f));
        hud.text(FontId::Bold, buf, {146.0f, y + 1.0f}, 24.0f, s.ghostGap <= 0.0f ? vec4(0.45f, 1.0f, 0.5f, 1.0f) : kMagenta);
    }
    drawTip(hud, s);

    hud.anchor = Anchor::Center;
    hud.vAnchor = gfx::VAnchor::Middle;
    if (s.countdown > 0.0f) {
        std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::ceil(s.countdown)));
        float pulse = 1.0f + 0.25f * (s.countdown - std::floor(s.countdown));
        hud.text(FontId::Bold, buf, {800.0f, 400.0f}, 190.0f * pulse, kWhite, Align::Center,
                 0.2f, alpha(kCyan, 0.8f));
        // Appui trop tôt : le joueur le sait tout de suite. Un appui réussi
        // ne se révèle qu'au signal.
        if (s.startBoost == 2)
            hud.text(FontId::Bold, tr("TROP TÔT"), {800.0f, 520.0f}, 44.0f, kWhite, Align::Center, 0.2f,
                     alpha(kMagenta, 0.8f));
    } else if (s.gridStart && s.raceTime < 1.2f && !s.finished) {
        hud.text(FontId::Bold, tr("GO !"), {800.0f, 400.0f}, 170.0f,
                 alpha(kYellow, 1.0f - s.raceTime / 1.2f), Align::Center, 0.2f);
        if (s.startBoost == 1)
            hud.text(FontId::Bold, tr("DÉPART TURBO !"), {800.0f, 520.0f}, 48.0f,
                     alpha(kWhite, 1.0f - s.raceTime / 1.2f), Align::Center, 0.2f,
                     alpha(kCyan, 0.8f * (1.0f - s.raceTime / 1.2f)));
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
        hud.quad({0.0f, 232.0f}, {1600.0f, 232.0f}, {1600.0f, 540.0f}, {0.0f, 540.0f},
                 vec4(0.02f, 0.02f, 0.08f, 0.78f));
        hud.line({200.0f, 232.0f}, {1400.0f, 232.0f}, 2.0f, kCyan, 0.5f);
        hud.line({200.0f, 540.0f}, {1400.0f, 540.0f}, 2.0f, kMagenta, 0.5f);
        hud.text(FontId::Bold, tr("COURSE TERMINÉE"), {800.0f, 292.0f}, 50.0f, kWhite, Align::Center,
                 0.2f, alpha(kCyan, 0.6f));
        // La note à gauche, le détail à droite.
        const vec4 tint = gradeColor(s.score.grade);
        const char letter[2] = {s.score.grade, '\0'};
        hud.text(FontId::Bold, letter, {560.0f, 478.0f}, 190.0f, tint, Align::Center, 0.0f, alpha(tint, 0.3f));
        const float x = 700.0f;
        std::snprintf(buf, sizeof(buf), tr("POSITION %d / %d"), s.position, s.racers);
        hud.text(FontId::Medium, buf, {x, 362.0f}, 28.0f, kYellow, Align::Left);
        // Temps de la course, et meilleur temps sur ce morceau.
        char time[24];
        formatTime(time, sizeof(time), s.finishTime);
        std::snprintf(buf, sizeof(buf), tr("TEMPS %s"), time);
        hud.text(FontId::Medium, buf, {1060.0f, 362.0f}, 28.0f, kWhite, Align::Left);
        if (s.newBestTime) {
            hud.text(FontId::Bold, tr("MEILLEUR TEMPS"), {1060.0f, 504.0f}, 24.0f, kYellow, Align::Left);
        } else if (s.bestTime > 0.0f) {
            formatTime(time, sizeof(time), s.bestTime);
            std::snprintf(buf, sizeof(buf), tr("MEILLEUR %s"), time);
            hud.text(FontId::Medium, buf, {1060.0f, 504.0f}, 22.0f, alpha(kWhite, 0.75f), Align::Left);
        }
        std::snprintf(buf, sizeof(buf), tr("SCORE %d"), s.score.total);
        hud.text(FontId::Bold, buf, {x, 418.0f}, 48.0f, kWhite, Align::Left);
        char parts[160];
        std::snprintf(parts, sizeof(parts), "%s %d    %s %d    %s %d    %s %d", tr("PLACE"), s.score.place,
                      tr("TEMPO"), s.score.tempo, tr("PILOTAGE"), s.score.clean, tr("BONUS"), s.score.items);
        hud.text(FontId::Medium, parts, {x, 460.0f}, 19.0f, alpha(kWhite, 0.75f), Align::Left);
        if (s.newRecord) {
            hud.text(FontId::Bold, tr("NOUVEAU RECORD"), {x, 504.0f}, 24.0f, kYellow, Align::Left);
        } else if (s.bestScore > 0) {
            std::snprintf(buf, sizeof(buf), tr("RECORD %d (%c)"), s.bestScore, gradeOf(s.bestScore));
            hud.text(FontId::Medium, buf, {x, 504.0f}, 22.0f, alpha(kWhite, 0.75f), Align::Left);
        }
    }
}

vec4 gradeColor(char grade) {
    switch (grade) {
    case 'S': return kYellow;
    case 'A': return vec4(0.35f, 1.0f, 0.45f, 1.0f);
    case 'B': return kCyan;
    case 'C': return kMagenta;
    default: return vec4(0.65f, 0.65f, 0.75f, 1.0f);
    }
}

void buildHud(gfx::HudBatch& hud, const HudState& state, const glm::mat4& viewProj) {
    drawNameTags(hud, state, viewProj);
    drawStandings(hud, state);
    drawSpeed(hud, state);
    drawMinimap(hud, state);
    drawSongBar(hud, state);
    drawButtons(hud, state);
    drawAlerts(hud, state, viewProj);
    drawRaceFlow(hud, state);
}

} // namespace game

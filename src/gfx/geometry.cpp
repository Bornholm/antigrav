#include "gfx/geometry.h"

#include <algorithm>
#include <cmath>

namespace gfx {

namespace {

struct Rng {
    uint32_t state;
    explicit Rng(uint32_t seed) : state(seed) {}
    float next() {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(state >> 8) / 16777216.0f;
    }
    float range(float lo, float hi) { return lo + (hi - lo) * next(); }
};

// Couleur vers laquelle tirent les néons de la piste quand le morceau chauffe.
// La même valeur est dans shaders/road.frag.
const glm::vec3 kHot{1.0f, 0.32f, 0.06f};

// Bande longitudinale de la piste entre deux points de la section, donnés en
// décalage latéral et hauteur.
void addStrip(MeshData& mesh, const sim::Track& track, float lat0, float h0, float lat1,
              float h1, float kind, const glm::vec4& color) {
    const int count = static_cast<int>(track.frames.size());
    uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
    for (int i = 0; i < count; i++) {
        const sim::TrackFrame& f = track.frames[i];
        float dist = track.distOf(i);
        Vertex a, b;
        a.pos = f.pos + f.right * lat0;
        a.height = h0;
        a.normal = f.up;
        a.kind = kind;
        a.uv = {0.0f, dist};
        a.color = color;
        // Bandes lumineuses : elles virent au chaud avec l'intensité du morceau.
        if (kind < 0.5f) {
            a.color = glm::vec4(glm::mix(glm::vec3(color), kHot, 0.8f * f.heat), color.a * (1.0f + 0.5f * f.heat));
        }
        b = a;
        b.pos = f.pos + f.right * lat1;
        b.height = h1;
        b.uv = {1.0f, dist};
        mesh.add(a);
        mesh.add(b);
    }
    for (int i = 0; i + 1 < count; i++) {
        uint32_t v = base + static_cast<uint32_t>(i) * 2;
        mesh.quad(v, v + 1, v + 3, v + 2);
    }
}

// Face plane d'un maillage, à normale constante.
void addFace(MeshData& mesh, std::initializer_list<glm::vec3> points, float kind,
             const glm::vec4& color, const glm::vec3& outwardFrom,
             std::initializer_list<glm::vec2> uvs = {}) {
    const glm::vec3* p = points.begin();
    const int n = static_cast<int>(points.size());
    glm::vec3 normal = glm::normalize(glm::cross(p[1] - p[0], p[2] - p[0]));
    glm::vec3 center{0.0f};
    for (int i = 0; i < n; i++) center += p[i];
    center /= static_cast<float>(n);
    if (glm::dot(normal, center - outwardFrom) < 0.0f) normal = -normal;

    uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
    for (int i = 0; i < n; i++) {
        Vertex v;
        v.pos = p[i];
        v.normal = normal;
        v.kind = kind;
        v.color = color;
        if (static_cast<int>(uvs.size()) == n) v.uv = uvs.begin()[i];
        mesh.add(v);
    }
    for (int i = 1; i + 1 < n; i++) mesh.tri(base, base + i, base + i + 1);
}

} // namespace

MeshData buildRoad(const sim::Track& track) {
    MeshData mesh;
    const int count = static_cast<int>(track.frames.size());
    const float hw = track.halfWidth;
    const float lats[3] = {-hw, 0.0f, hw};
    for (int i = 0; i < count; i++) {
        const sim::TrackFrame& f = track.frames[i];
        float dist = track.distOf(i);
        for (float lat : lats) {
            Vertex v;
            v.pos = f.pos + f.right * lat;
            v.normal = f.up;
            v.uv = {lat, dist};
            v.color = {hw, track.length, f.heat, 0.0f};
            mesh.add(v);
        }
    }
    for (int i = 0; i + 1 < count; i++) {
        uint32_t v = static_cast<uint32_t>(i) * 3;
        mesh.quad(v, v + 1, v + 4, v + 3);
        mesh.quad(v + 1, v + 2, v + 5, v + 4);
    }
    return mesh;
}

MeshData buildRails(const sim::Track& track, const Theme& theme) {
    MeshData mesh;
    const float hw = track.halfWidth;
    const glm::vec3 kCyan = theme.neonA, kMagenta = theme.neonB;
    const glm::vec4 dark{kCyan, 1.0f};
    for (float side : {-1.0f, 1.0f}) {
        // Muret : face intérieure inclinée, bande lumineuse, face extérieure.
        addStrip(mesh, track, side * hw, 0.0f, side * (hw + 0.55f), 1.25f, 1.0f, dark);
        addStrip(mesh, track, side * (hw + 0.55f), 1.25f, side * (hw + 0.95f), 1.25f, 0.0f,
                 glm::vec4(kCyan, 7.0f));
        addStrip(mesh, track, side * (hw + 0.95f), 1.25f, side * (hw + 1.4f), -0.8f, 1.0f,
                 dark);
        // Rail flottant à l'extérieur de la piste.
        addStrip(mesh, track, side * (hw + 4.2f), 0.35f, side * (hw + 4.6f), 0.35f, 0.0f,
                 glm::vec4(kMagenta, 6.0f));
        addStrip(mesh, track, side * (hw + 4.2f), 0.35f, side * (hw + 4.2f), 0.05f, 0.0f,
                 glm::vec4(kMagenta, 6.0f));
    }
    return mesh;
}

// Les éléments de piste gardent les mêmes couleurs sur tous les circuits : le
// joueur doit les reconnaître avant d'avoir le temps de les regarder.
MeshData buildFeatures(const sim::Track& track) {
    MeshData mesh;
    const glm::vec3 kPad{1.0f, 0.80f, 0.15f};
    const glm::vec3 kDanger{1.0f, 0.07f, 0.10f};

    // Quadrilatère entre quatre points donnés en distance, décalage, hauteur.
    auto quad = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                    const glm::vec3& d, const glm::vec4& color) {
        uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
        for (const glm::vec3& p : {a, b, c, d}) {
            sim::TrackFrame f = track.at(p.x);
            Vertex v;
            v.pos = f.pos + f.right * p.y;
            v.height = p.z;
            v.normal = f.up;
            v.kind = 0.0f;
            v.uv = {0.0f, p.x};
            v.color = color;
            mesh.add(v);
        }
        mesh.quad(base, base + 1, base + 2, base + 3);
    };
    // Bande posée à plat, découpée assez fin pour suivre la piste.
    auto flat = [&](float d0, float d1, float lat0, float lat1, float h, const glm::vec4& color) {
        const int pieces = std::max(1, static_cast<int>(std::ceil((d1 - d0) / 6.0f)));
        for (int i = 0; i < pieces; i++) {
            float a = glm::mix(d0, d1, static_cast<float>(i) / pieces);
            float b = glm::mix(d0, d1, static_cast<float>(i + 1) / pieces);
            quad({a, lat0, h}, {a, lat1, h}, {b, lat1, h}, {b, lat0, h}, color);
        }
    };

    // Un trait en travers de la piste sur chaque temps du morceau, plus épais
    // sur le premier temps de la mesure : on voit le tempo défiler.
    {
        const glm::vec3 kBeat{0.75f, 0.85f, 1.0f};
        const float hw = track.halfWidth - 0.6f;
        for (size_t i = 0; i < track.beats.size(); i++) {
            const float at = track.distAt(track.beats[i]);
            if (at < 40.0f || at > track.length - 20.0f) continue;
            const bool down = i < track.downbeats.size() && track.downbeats[i];
            flat(at, at + (down ? 2.6f : 1.0f), -hw, hw, 0.04f, glm::vec4(kBeat, down ? 2.4f : 0.9f));
        }
    }

    for (const sim::Feature& f : track.features) {
        const float d0 = f.dist, d1 = f.dist + f.length;
        const float l0 = f.lateral - f.halfWidth, l1 = f.lateral + f.halfWidth;

        if (f.type == sim::FeatureType::ItemBox) {
            // Boîte à bonus : un grand losange lumineux dressé sur un socle,
            // surmonté d'un faisceau qu'on repère de loin. Rien à voir avec
            // une plaque d'accélération.
            const float mid = 0.5f * (d0 + d1);
            const float c = f.lateral, low = 0.5f, high = 8.5f, half = 3.4f, middle = 0.5f * (low + high);
            // Socle : un carré plein au sol, bordé d'un trait vif.
            flat(mid - 7.0f, mid + 7.0f, c - half, c + half, 0.05f, glm::vec4(kPad, 0.9f));
            for (float edge : {c - half, c + half - 0.4f}) flat(mid - 7.0f, mid + 7.0f, edge, edge + 0.4f, 0.07f, glm::vec4(kPad, 7.0f));
            flat(mid - 7.0f, mid - 6.6f, c - half, c + half, 0.07f, glm::vec4(kPad, 7.0f));
            flat(mid + 6.6f, mid + 7.0f, c - half, c + half, 0.07f, glm::vec4(kPad, 7.0f));
            // Losange : contour épais, cœur presque blanc, sur deux faces.
            for (float face : {mid, mid + 0.5f}) {
                quad({face, c - half, middle}, {face, c, low}, {face, c + half, middle}, {face, c, high}, glm::vec4(kPad, 8.0f));
                quad({face + 0.06f, c - half * 0.62f, middle}, {face + 0.06f, c, low + 1.5f}, {face + 0.06f, c + half * 0.62f, middle},
                     {face + 0.06f, c, high - 1.5f}, glm::vec4(1.0f, 1.0f, 0.85f, 14.0f));
            }
            // Faisceau vertical, en croix pour se voir sous tous les angles.
            const float top = 60.0f;
            quad({mid, c - 0.8f, high}, {mid, c + 0.8f, high}, {mid, c + 0.25f, top}, {mid, c - 0.25f, top}, glm::vec4(kPad, 6.0f));
            quad({mid - 0.8f, c, high}, {mid + 0.8f, c, high}, {mid + 0.25f, c, top}, {mid - 0.25f, c, top}, glm::vec4(kPad, 6.0f));
        } else if (f.type == sim::FeatureType::Jump) {
            // Tremplin : une rampe lumineuse sur toute la largeur, striée de
            // chevrons blancs, entre deux pylônes.
            const glm::vec3 kJump{0.75f, 0.95f, 1.0f};
            const float lift = 2.2f;
            quad({d0, l0, 0.06f}, {d0, l1, 0.06f}, {d1, l1, lift}, {d1, l0, lift}, glm::vec4(kJump, 0.5f));
            for (int c = 0; c < 4; c++) {
                const float a0 = d0 + (d1 - d0) * (static_cast<float>(c) + 0.15f) / 4.0f;
                const float a1 = a0 + (d1 - d0) * 0.12f;
                const float h0 = lift * (a0 - d0) / (d1 - d0) + 0.1f, h1 = lift * (a1 - d0) / (d1 - d0) + 0.1f;
                quad({a0, l0, h0}, {a0, l1, h0}, {a1, l1, h1}, {a1, l0, h1}, glm::vec4(kJump, 6.0f));
            }
            for (float post : {l0, l1}) {
                quad({d1, post - 0.5f, 0.0f}, {d1, post + 0.5f, 0.0f}, {d1, post + 0.5f, 26.0f}, {d1, post - 0.5f, 26.0f},
                     glm::vec4(kJump, 7.0f));
                quad({d1 - 0.5f, post, 0.0f}, {d1 + 0.5f, post, 0.0f}, {d1 + 0.5f, post, 26.0f}, {d1 - 0.5f, post, 26.0f},
                     glm::vec4(kJump, 7.0f));
            }
        } else {
            // Barrière : bloc sombre, bandes lumineuses face au pilote.
            const float top = 3.4f;
            const glm::vec4 body(kDanger, 0.012f), glow(kDanger, 7.0f);
            quad({d0, l0, 0.0f}, {d0, l1, 0.0f}, {d0, l1, top}, {d0, l0, top}, body);
            quad({d1, l0, 0.0f}, {d1, l1, 0.0f}, {d1, l1, top}, {d1, l0, top}, body);
            quad({d0, l0, top}, {d0, l1, top}, {d1, l1, top}, {d1, l0, top}, body);
            quad({d0, l0, 0.0f}, {d1, l0, 0.0f}, {d1, l0, top}, {d0, l0, top}, body);
            quad({d0, l1, 0.0f}, {d1, l1, 0.0f}, {d1, l1, top}, {d0, l1, top}, body);
            const float face = d0 - 0.06f;
            for (float h : {0.35f, 1.55f, 2.75f})
                quad({face, l0 + 0.3f, h}, {face, l1 - 0.3f, h}, {face, l1 - 0.3f, h + 0.5f},
                     {face, l0 + 0.3f, h + 0.5f}, glow);
            for (float edge : {l0, l1 - 0.3f})
                quad({face, edge, 0.0f}, {face, edge + 0.3f, 0.0f}, {face, edge + 0.3f, top},
                     {face, edge, top}, glow);
            // Bandeau d'annonce au sol, cent mètres avant.
            flat(d0 - 100.0f, d0 - 4.0f, l0, l0 + 0.3f, 0.07f, glm::vec4(kDanger, 3.0f));
            flat(d0 - 100.0f, d0 - 4.0f, l1 - 0.3f, l1, 0.07f, glm::vec4(kDanger, 3.0f));
        }
    }
    return mesh;
}

// Portiques lumineux au-dessus de la piste : celui de la ligne de départ, puis
// un tous les 900 m environ. Ils rythment le défilement.
MeshData buildGates(const sim::Track& track, const Theme& theme) {
    MeshData mesh;
    const float hw = track.halfWidth;
    const glm::vec3 kCyan = theme.neonA, kMagenta = theme.neonB;
    const int gateCount = std::max(1, static_cast<int>(std::round(track.length / 900.0f)));

    for (int g = 0; g < gateCount; g++) {
        float dist = track.length * static_cast<float>(g) / static_cast<float>(gateCount);
        bool start = g == 0;
        glm::vec3 tint = start ? theme.accent : (g % 2 ? kMagenta : kCyan);
        float intensity = start ? 9.0f : 6.0f;

        // Panneau vertical en travers de la piste, entre deux points de la section.
        auto panel = [&](float lat0, float h0, float lat1, float h1, float kind,
                         const glm::vec4& color) {
            for (float offset : {-0.5f, 0.5f}) {
                sim::TrackFrame f = track.at(dist + offset);
                uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
                const float lats[4] = {lat0, lat1, lat1, lat0};
                const float heights[4] = {h0, h0, h1, h1};
                for (int i = 0; i < 4; i++) {
                    Vertex v;
                    v.pos = f.pos + f.right * lats[i];
                    v.height = heights[i];
                    v.normal = f.up;
                    v.kind = kind;
                    v.uv = {static_cast<float>(i == 1 || i == 2), dist};
                    v.color = color;
                    mesh.add(v);
                }
                mesh.quad(base, base + 1, base + 2, base + 3);
            }
        };

        const float outer = hw + 2.6f, inner = hw + 1.7f, top = 11.0f;
        const glm::vec4 dark{tint, 1.0f};
        const glm::vec4 glow{tint, intensity};
        for (float side : {-1.0f, 1.0f}) {
            panel(side * inner, 0.0f, side * outer, top, 1.0f, dark);
            panel(side * (inner - 0.25f), 0.0f, side * inner, top, 0.0f, glow);
        }
        panel(-outer, top, outer, top + 1.3f, 1.0f, dark);
        panel(-outer, top - 0.3f, outer, top, 0.0f, glow);
    }

    // Station : la piste traverse de grands anneaux lumineux.
    if (theme.scenery == Scenery::Station) {
        const int ringCount = std::max(1, static_cast<int>(std::round(track.length / 450.0f)));
        const int segments = 28;
        for (int r = 0; r < ringCount; r++) {
            float dist = track.length * (static_cast<float>(r) + 0.5f) / ringCount;
            sim::TrackFrame f = track.at(dist);
            glm::vec3 tint = r % 3 == 0 ? kMagenta : kCyan;
            const float inner = hw + 22.0f, outer = hw + 24.5f, lift = 9.0f;
            uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
            for (int i = 0; i <= segments; i++) {
                float a = 6.2831853f * static_cast<float>(i) / segments;
                for (float radius : {inner, outer}) {
                    Vertex v;
                    v.pos = f.pos + f.right * (std::cos(a) * radius);
                    v.height = lift + std::sin(a) * radius;
                    v.normal = f.up;
                    v.uv = {0.0f, dist};
                    v.color = glm::vec4(tint, 5.0f);
                    mesh.add(v);
                }
            }
            for (int i = 0; i < segments; i++) {
                uint32_t v = base + static_cast<uint32_t>(i) * 2;
                mesh.quad(v, v + 1, v + 3, v + 2);
            }
        }
    }
    return mesh;
}

MeshData buildScenery(const sim::Track& track, const Theme& theme) {
    MeshData mesh;
    Rng rng(20261124u + static_cast<uint32_t>(theme.scenery) * 977u);
    const glm::vec3 kCyan = theme.neonA, kMagenta = theme.neonB;
    const glm::vec3 kThird = glm::mix(theme.neonA, theme.neonB, 0.5f);

    glm::vec3 lo{1e9f}, hi{-1e9f};
    for (const auto& f : track.frames) {
        lo = glm::min(lo, f.pos);
        hi = glm::max(hi, f.pos);
    }
    const float margin = 1900.0f;
    const float baseY = lo.y - 90.0f;
    const int frameCount = static_cast<int>(track.frames.size());
    const glm::vec3 trackCenter = (lo + hi) * 0.5f;

    // Distance horizontale à la piste, et hauteur de la piste au plus proche.
    auto nearestTrack = [&](float x, float z, float& trackY) {
        float best = 1e18f;
        for (int i = 0; i < frameCount; i += 20) {
            const glm::vec3& p = track.frames[i].pos;
            float dx = p.x - x, dz = p.z - z;
            float d = dx * dx + dz * dz;
            if (d < best) {
                best = d;
                trackY = p.y;
            }
        }
        return std::sqrt(best);
    };

    auto tower = [&](float x, float z, float width, float depth, float top, const glm::vec3& tint,
                     float seed, bool panel) {
        float tallness = top - baseY;
        glm::vec4 color{tint, tallness};
        float x0 = x - width * 0.5f, x1 = x + width * 0.5f;
        float z0 = z - depth * 0.5f, z1 = z + depth * 0.5f;
        glm::vec3 center{x, baseY + tallness * 0.5f, z};
        auto wall = [&](glm::vec3 a, glm::vec3 b, float length) {
            glm::vec3 a1{a.x, top, a.z}, b1{b.x, top, b.z};
            addFace(mesh, {a, b, b1, a1}, seed, color, center,
                    {{0.0f, 0.0f}, {length, 0.0f}, {length, tallness}, {0.0f, tallness}});
        };
        wall({x0, baseY, z0}, {x1, baseY, z0}, width);
        wall({x1, baseY, z0}, {x1, baseY, z1}, depth);
        wall({x1, baseY, z1}, {x0, baseY, z1}, width);
        wall({x0, baseY, z1}, {x0, baseY, z0}, depth);
        addFace(mesh, {{x0, top, z0}, {x1, top, z0}, {x1, top, z1}, {x0, top, z1}}, seed,
                glm::vec4(tint, 1e6f), center - glm::vec3(0.0f, 1e4f, 0.0f));
        if (!panel) return;

        // Panneau lumineux tourné vers la piste.
        float panelH = rng.range(22.0f, 55.0f);
        float y0 = baseY + tallness * rng.range(0.45f, 0.75f);
        float y1 = std::min(y0 + panelH, top - 6.0f);
        glm::vec4 pColor{rng.next() < 0.7f ? kMagenta : kCyan, 3.2f};
        const float off = 0.8f;
        if (std::abs(trackCenter.x - x) > std::abs(trackCenter.z - z)) {
            float px = trackCenter.x > x ? x1 + off : x0 - off;
            float a = z0 + depth * 0.12f, b = z1 - depth * 0.12f;
            addFace(mesh, {{px, y0, a}, {px, y0, b}, {px, y1, b}, {px, y1, a}}, -1.0f, pColor,
                    center, {{0.0f, y0}, {1.0f, y0}, {1.0f, y1}, {0.0f, y1}});
        } else {
            float pz = trackCenter.z > z ? z1 + off : z0 - off;
            float a = x0 + width * 0.12f, b = x1 - width * 0.12f;
            addFace(mesh, {{a, y0, pz}, {b, y0, pz}, {b, y1, pz}, {a, y1, pz}}, -1.0f, pColor,
                    center, {{0.0f, y0}, {1.0f, y0}, {1.0f, y1}, {0.0f, y1}});
        }
    };

    // Piton rocheux : prisme à sept pans, resserré par paliers.
    auto spire = [&](float x, float z, float radius, float top, float seed, bool mesa) {
        const int sides = 7, rings = 4;
        float tallness = top - baseY;
        const float heights[rings] = {0.0f, 0.38f, 0.78f, 1.0f};
        float radii[rings] = {1.0f, rng.range(0.70f, 0.90f), rng.range(0.45f, 0.70f),
                              mesa ? rng.range(0.40f, 0.60f) : rng.range(0.08f, 0.22f)};
        float twist = rng.range(0.0f, 6.28f);
        glm::vec3 pts[rings][sides];
        for (int r = 0; r < rings; r++)
            for (int k = 0; k < sides; k++) {
                float a = twist + 6.2831853f * static_cast<float>(k) / sides;
                float rr = radius * radii[r] * (0.85f + 0.3f * rng.next());
                pts[r][k] = {x + std::cos(a) * rr, baseY + tallness * heights[r],
                             z + std::sin(a) * rr};
            }
        glm::vec3 center{x, baseY + tallness * 0.5f, z};
        glm::vec4 color{kCyan, tallness};
        for (int r = 0; r + 1 < rings; r++)
            for (int k = 0; k < sides; k++) {
                int n = (k + 1) % sides;
                float y0 = tallness * heights[r], y1 = tallness * heights[r + 1];
                addFace(mesh, {pts[r][k], pts[r][n], pts[r + 1][n], pts[r + 1][k]}, 1000.0f + seed,
                        color, center, {{0.0f, y0}, {1.0f, y0}, {1.0f, y1}, {0.0f, y1}});
            }
        // Sommet.
        for (int k = 1; k + 1 < sides; k++)
            addFace(mesh, {pts[rings - 1][0], pts[rings - 1][k], pts[rings - 1][k + 1]},
                    1000.0f + seed, color, center - glm::vec3(0.0f, 1e4f, 0.0f),
                    {{0.0f, tallness}, {1.0f, tallness}, {1.0f, tallness}});
    };

    // Densité constante le long de la piste, quelle que soit sa longueur.
    const float perKm = theme.scenery == Scenery::Canyon ? 120.0f
                        : (theme.scenery == Scenery::Station ? 65.0f : 140.0f);
    const int wanted = static_cast<int>(perKm * track.length / 1000.0f);
    int placed = 0;
    for (int attempt = 0; attempt < wanted * 40 && placed < wanted; attempt++) {
        const size_t firstVertex = mesh.vertices.size();
        float x = rng.range(lo.x - margin, hi.x + margin);
        float z = rng.range(lo.z - margin, hi.z + margin);
        float trackY = 0.0f;
        float nearest = nearestTrack(x, z, trackY);
        float seed = rng.range(0.0f, 100.0f);

        if (theme.scenery == Scenery::Canyon) {
            // Des pitons serrés le long de la piste : on roule dans un défilé.
            float radius = rng.range(22.0f, 70.0f);
            if (nearest < 55.0f + radius || nearest > 1700.0f) continue;
            bool mesa = rng.next() < 0.35f;
            float t = rng.next();
            float rise = (mesa ? 60.0f : 90.0f) + 330.0f * t * t;
            rise *= std::clamp(nearest / 450.0f, 0.35f, 1.0f);
            spire(x, z, radius * (mesa ? 1.6f : 1.0f), trackY + rise, seed, mesa);
        } else if (theme.scenery == Scenery::Station) {
            // Mâts fins et très hauts, plus bas que la piste comme au-dessus.
            float width = rng.range(9.0f, 24.0f);
            if (nearest < 150.0f + width || nearest > 1900.0f) continue;
            float pick = rng.next();
            glm::vec3 tint = pick < 0.6f ? kCyan : kMagenta;
            float top = trackY + rng.range(-40.0f, 520.0f) * std::clamp(nearest / 500.0f, 0.3f, 1.0f);
            tower(x, z, width, width * rng.range(0.8f, 1.3f), std::max(top, baseY + 40.0f), tint,
                  seed, rng.next() < 0.10f);
        } else {
            float width = rng.range(28.0f, 85.0f);
            float depth = rng.range(28.0f, 85.0f);
            // Ni trop près, ni si loin que la brume les efface.
            float clearance = 230.0f + std::max(width, depth);
            if (nearest < clearance || nearest > 1900.0f) continue;
            float t = rng.next();
            float height = 70.0f + 380.0f * t * t;
            // Plus basses près de la piste, pour dégager la vue.
            height *= std::clamp(nearest / 700.0f, 0.4f, 1.0f);
            float top = baseY + height + rng.range(60.0f, 110.0f);
            float pick = rng.next();
            glm::vec3 tint = pick < 0.45f ? kCyan : (pick < 0.85f ? kMagenta : kThird);
            tower(x, z, width, depth, top, tint, seed, rng.next() < 0.22f);
        }
        // Égaliseur : chaque objet suit une des huit bandes de la musique. Le
        // shader lit dans height la bande et la hauteur au-dessus de la base.
        const float band = static_cast<float>(placed % 8);
        for (size_t v = firstVertex; v < mesh.vertices.size(); v++)
            mesh.vertices[v].height = std::clamp(mesh.vertices[v].pos.y - baseY, 0.0f, 4000.0f) + 4096.0f * band;
        placed++;
    }
    return mesh;
}

// Repère du vaisseau : +X à droite, +Y en haut, -Z vers l'avant.

namespace {

struct Nozzle {
    float x, y, halfW, halfH;
};

// Outils communs aux modèles. Chaque modèle décrit son côté droit, le côté
// gauche est obtenu par symétrie.
struct ShipBuilder {
    MeshData mesh;
    std::vector<Nozzle> nozzles;
    float side = 1.0f;
    const glm::vec3 core{0.0f, 0.5f, 0.0f};
    const glm::vec3 below{0.0f, -10.0f, 0.0f};
    const glm::vec4 light{0.75f, 0.80f, 1.0f, 1.0f};
    const glm::vec4 mid{0.42f, 0.46f, 0.66f, 1.0f};
    const glm::vec4 dark{0.20f, 0.22f, 0.36f, 1.0f};
    const glm::vec4 white{1.0f};

    glm::vec3 P(float x, float y, float z) const { return {x * side, y, z}; }

    void hull(std::initializer_list<glm::vec3> pts, const glm::vec4& color) {
        addFace(mesh, pts, 0.0f, color, core);
    }
    void top(std::initializer_list<glm::vec3> pts, const glm::vec4& color) {
        addFace(mesh, pts, 0.0f, color, below);
    }
    // Liseré lumineux posé juste au-dessus d'une arête.
    void strip(glm::vec3 a, glm::vec3 b, float width) {
        glm::vec3 along = glm::normalize(b - a);
        glm::vec3 across = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), along)) * width;
        if (glm::dot(across, glm::vec3(side, 0.0f, 0.0f)) > 0.0f) across = -across;
        const glm::vec3 lift{0.0f, 0.03f, 0.0f};
        addFace(mesh, {a + lift, b + lift, b + lift + across, a + lift + across}, 1.0f, white,
                below);
    }
    // Liseré sur une face verticale ou arrière, donné par ses quatre coins.
    void glow(std::initializer_list<glm::vec3> pts) { addFace(mesh, pts, 1.0f, white, core); }
    // Tuyère : rectangle lumineux à l'arrière, mémorisé pour y accrocher la flamme.
    void nozzle(float cx, float cy, float halfW, float halfH, float z) {
        addFace(mesh,
                {{cx - halfW, cy - halfH, z}, {cx + halfW, cy - halfH, z},
                 {cx + halfW, cy + halfH, z}, {cx - halfW, cy + halfH, z}},
                2.0f, white, core, {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}});
        nozzles.push_back({cx, cy, halfW, halfH});
    }
};

// Flèche : fuselage central relevé, ailes en deux panneaux dont le second
// plonge et dépasse derrière les tuyères.
void buildArrow(ShipBuilder& b) {
    for (float s : {-1.0f, 1.0f}) {
        b.side = s;
        glm::vec3 nose = b.P(0.0f, 0.36f, -4.0f), noseLow = b.P(0.0f, 0.10f, -3.5f);
        glm::vec3 canopyFront = b.P(0.0f, 0.62f, -2.2f), canopyTop = b.P(0.0f, 1.02f, -0.6f);
        glm::vec3 spine = b.P(0.0f, 1.12f, 1.75f), shoulder = b.P(0.62f, 0.46f, -1.9f);
        glm::vec3 flank = b.P(0.92f, 0.62f, -0.2f), rearTop = b.P(1.02f, 0.92f, 1.95f);
        glm::vec3 rearLow = b.P(1.12f, 0.06f, 2.05f), rearLowMid = b.P(0.0f, 0.06f, 2.05f);
        glm::vec3 belly = b.P(0.50f, 0.05f, -2.0f);
        b.hull({nose, shoulder, canopyFront}, b.mid);
        b.hull({canopyFront, shoulder, flank, canopyTop}, b.dark);
        b.hull({canopyTop, flank, rearTop, spine}, b.light);
        b.hull({nose, noseLow, belly, shoulder}, b.dark);
        b.hull({shoulder, belly, rearLow, flank}, b.mid);
        b.hull({flank, rearLow, rearTop}, b.mid);
        b.hull({spine, rearTop, rearLow, rearLowMid}, b.dark);
        b.hull({noseLow, belly, rearLow, rearLowMid}, b.dark);

        glm::vec3 rootFront = b.P(0.80f, 0.50f, -1.3f), rootRear = b.P(1.05f, 0.62f, 1.95f);
        glm::vec3 midFront = b.P(1.75f, 0.46f, 0.35f), midRear = b.P(1.85f, 0.50f, 2.30f);
        glm::vec3 tipFront = b.P(2.75f, 0.14f, 1.75f), tipRear = b.P(2.95f, 0.10f, 3.05f);
        const glm::vec3 thick{0.0f, -0.20f, 0.0f};
        b.top({rootFront, midFront, midRear, rootRear}, b.mid);
        b.top({midFront, tipFront, tipRear, midRear}, b.light);
        b.hull({rootRear, midRear, midRear + thick, rootRear + thick}, b.dark);
        b.hull({midRear, tipRear, tipRear + thick * 0.4f, midRear + thick}, b.dark);
        glm::vec3 finFront = b.P(2.72f, 0.15f, 2.0f), finTop = b.P(2.90f, 0.95f, 3.1f);
        b.hull({finFront, tipRear, finTop}, b.mid);

        b.strip(nose, shoulder, 0.07f);
        b.strip(shoulder, flank, 0.07f);
        b.strip(flank, rearTop, 0.07f);
        b.strip(midFront, tipFront, 0.10f);
        b.strip(tipFront, tipRear, 0.10f);
        b.strip(rootFront, midFront, 0.08f);
        b.strip(midRear, tipRear, 0.10f);
        b.glow({finFront + b.P(0.03f, 0.0f, 0.0f), finTop + b.P(0.03f, 0.0f, 0.0f),
                finTop + b.P(0.03f, -0.12f, -0.05f), finFront + b.P(0.03f, 0.0f, 0.16f)});
        for (float cx : {0.33f, 0.76f}) b.nozzle(cx * s, 0.37f, 0.17f, 0.21f, 2.06f);
    }
    b.glow({{-0.70f, 0.72f, 1.99f}, {0.70f, 0.72f, 1.99f}, {0.70f, 0.83f, 1.98f},
            {-0.70f, 0.83f, 1.98f}});
}

// Manta : aile volante large et plate, deux tuyères écartées, queue en fouet.
void buildManta(ShipBuilder& b) {
    glm::vec3 nose{0.0f, 0.30f, -3.1f}, canopy{0.0f, 0.82f, -0.9f}, tail{0.0f, 0.62f, 2.3f};
    glm::vec3 tailLow{0.0f, 0.10f, 2.4f};
    for (float s : {-1.0f, 1.0f}) {
        b.side = s;
        glm::vec3 p1 = b.P(1.25f, 0.36f, -2.2f), p2 = b.P(3.0f, 0.26f, -0.5f);
        glm::vec3 p3 = b.P(4.1f, 0.14f, 1.2f), p4 = b.P(2.7f, 0.26f, 2.1f);
        glm::vec3 p5 = b.P(1.15f, 0.42f, 2.4f), p5Low = b.P(1.15f, 0.10f, 2.45f);
        b.top({nose, p1, canopy}, b.mid);
        b.top({canopy, p1, p2}, b.light);
        b.top({canopy, p2, p3, p4}, b.mid);
        b.top({canopy, p4, p5, tail}, b.dark);
        b.hull({tail, p5, p5Low, tailLow}, b.dark);
        b.hull({p5, p4, p4 + glm::vec3(0.0f, -0.14f, 0.0f), p5Low}, b.dark);
        b.strip(nose, p1, 0.07f);
        b.strip(p1, p2, 0.09f);
        b.strip(p2, p3, 0.10f);
        b.strip(p3, p4, 0.10f);
        // Winglet relevé en bout d'aile.
        glm::vec3 w0 = b.P(3.75f, 0.16f, 0.6f), wTop = b.P(4.35f, 0.75f, 1.6f);
        b.hull({w0, p3, wTop}, b.mid);
        b.nozzle(0.72f * s, 0.27f, 0.36f, 0.13f, 2.46f);
    }
    // Queue lumineuse.
    b.glow({{-0.06f, 0.55f, 2.3f}, {0.06f, 0.55f, 2.3f}, {0.02f, 0.45f, 4.6f},
            {-0.02f, 0.45f, 4.6f}});
    b.glow({{-0.5f, 0.66f, 2.32f}, {0.5f, 0.66f, 2.32f}, {0.5f, 0.74f, 2.31f},
            {-0.5f, 0.74f, 2.31f}});
}

// Aiguille : fuselage long et étroit, petits plans avant, empennage en X et
// une seule grosse tuyère.
void buildNeedle(ShipBuilder& b) {
    glm::vec3 nose{0.0f, 0.40f, -5.4f};
    for (float s : {-1.0f, 1.0f}) {
        b.side = s;
        glm::vec3 topF = b.P(0.0f, 0.86f, -1.6f), sideF = b.P(0.56f, 0.46f, -1.6f);
        glm::vec3 lowF = b.P(0.0f, 0.08f, -1.6f);
        glm::vec3 topR = b.P(0.0f, 1.00f, 2.4f), sideR = b.P(0.66f, 0.48f, 2.4f);
        glm::vec3 lowR = b.P(0.0f, 0.04f, 2.4f);
        b.hull({nose, sideF, topF}, b.light);
        b.hull({nose, lowF, sideF}, b.dark);
        b.hull({topF, sideF, sideR, topR}, b.mid);
        b.hull({sideF, lowF, lowR, sideR}, b.dark);
        b.hull({topR, sideR, lowR}, b.dark);
        // Plan canard.
        glm::vec3 c0 = b.P(0.45f, 0.46f, -3.0f), c1 = b.P(1.75f, 0.40f, -2.1f);
        glm::vec3 c2 = b.P(0.55f, 0.46f, -1.8f);
        b.top({c0, c1, c2}, b.mid);
        b.strip(c0, c1, 0.07f);
        // Empennage en X : une dérive haute et une basse.
        glm::vec3 f0 = b.P(0.55f, 0.55f, 0.9f), f1 = b.P(2.35f, 1.45f, 3.1f);
        glm::vec3 f2 = b.P(0.62f, 0.60f, 2.4f);
        b.hull({f0, f1, f2}, b.mid);
        b.glow({f0, f1, f1 + glm::vec3(0.0f, -0.10f, -0.10f), f0 + glm::vec3(0.0f, -0.08f, 0.12f)});
        glm::vec3 g0 = b.P(0.55f, 0.40f, 0.9f), g1 = b.P(2.45f, 0.05f, 3.1f);
        glm::vec3 g2 = b.P(0.62f, 0.36f, 2.4f);
        b.hull({g0, g1, g2}, b.dark);
        b.strip(g0, g1, 0.09f);
        b.strip(nose, sideF, 0.06f);
        b.strip(sideF, sideR, 0.06f);
        b.nozzle(0.19f * s, 0.50f, 0.19f, 0.30f, 2.42f);
    }
}

// Jumeau : deux coques reliées par un pont, cockpit central, une tuyère par coque.
void buildTwin(ShipBuilder& b) {
    for (float s : {-1.0f, 1.0f}) {
        b.side = s;
        const float cx = 1.75f;
        glm::vec3 nose = b.P(cx, 0.34f, -3.6f);
        glm::vec3 topF = b.P(cx, 0.92f, -0.9f), inF = b.P(cx - 0.62f, 0.44f, -0.9f);
        glm::vec3 outF = b.P(cx + 0.62f, 0.44f, -0.9f), lowF = b.P(cx, 0.06f, -0.9f);
        glm::vec3 topR = b.P(cx, 1.02f, 2.3f), inR = b.P(cx - 0.70f, 0.46f, 2.3f);
        glm::vec3 outR = b.P(cx + 0.70f, 0.46f, 2.3f), lowR = b.P(cx, 0.04f, 2.3f);
        const glm::vec3 hullCore = b.P(cx, 0.5f, 0.5f);
        auto side = [&](std::initializer_list<glm::vec3> pts, const glm::vec4& color) {
            addFace(b.mesh, pts, 0.0f, color, hullCore);
        };
        side({nose, outF, topF}, b.light);
        side({nose, topF, inF}, b.mid);
        side({nose, lowF, outF}, b.dark);
        side({nose, inF, lowF}, b.dark);
        side({topF, outF, outR, topR}, b.mid);
        side({topF, topR, inR, inF}, b.light);
        side({outF, lowF, lowR, outR}, b.dark);
        side({inF, inR, lowR, lowF}, b.dark);
        side({topR, outR, lowR, inR}, b.dark);
        b.strip(nose, outF, 0.07f);
        b.strip(outF, outR, 0.08f);
        // Dérive courte au-dessus de la coque.
        glm::vec3 d0 = b.P(cx, 0.98f, 1.0f), d1 = b.P(cx, 1.75f, 2.6f), d2 = b.P(cx, 1.0f, 2.3f);
        side({d0, d1, d2}, b.mid);
        b.glow({d0, d1, d1 + glm::vec3(0.0f, -0.12f, -0.06f), d0 + glm::vec3(0.0f, 0.0f, 0.16f)});
        // Demi-pont vers le cockpit.
        glm::vec3 b0 = b.P(cx - 0.62f, 0.50f, -0.5f), b1 = b.P(0.0f, 0.56f, -0.9f);
        glm::vec3 b2 = b.P(0.0f, 0.58f, 1.7f), b3 = b.P(cx - 0.70f, 0.50f, 1.9f);
        b.top({b0, b1, b2, b3}, b.mid);
        b.hull({b3, b2, b2 + glm::vec3(0.0f, -0.2f, 0.0f), b3 + glm::vec3(0.0f, -0.2f, 0.0f)},
               b.dark);
        b.strip(b1, b0, 0.08f);
        // Cockpit en pyramide.
        glm::vec3 apex = b.P(0.0f, 1.05f, 0.2f), ck0 = b.P(0.0f, 0.58f, -1.2f);
        glm::vec3 ck1 = b.P(0.55f, 0.56f, 0.2f), ck2 = b.P(0.0f, 0.60f, 1.5f);
        b.hull({apex, ck0, ck1}, b.light);
        b.hull({apex, ck1, ck2}, b.dark);
        b.nozzle(cx * s, 0.48f, 0.40f, 0.30f, 2.32f);
    }
    b.glow({{-1.0f, 0.60f, 1.72f}, {1.0f, 0.60f, 1.72f}, {1.0f, 0.68f, 1.71f},
            {-1.0f, 0.68f, 1.71f}});
}

ShipBuilder buildVariant(int variant) {
    ShipBuilder b;
    switch (variant) {
    case 1: buildManta(b); break;
    case 2: buildNeedle(b); break;
    case 3: buildTwin(b); break;
    default: buildArrow(b); break;
    }
    return b;
}

} // namespace

MeshData buildShip(int variant) { return buildVariant(variant).mesh; }

MeshData buildTrail(int variant) {
    ShipBuilder b = buildVariant(variant);
    MeshData mesh;
    const glm::vec4 white{1.0f};
    const glm::vec3 none{0.0f, -100.0f, 0.0f};
    auto ribbon = [&](float cx, float cy, float halfW, float halfH, float z0, float length) {
        float z1 = z0 + length;
        addFace(mesh,
                {{cx - halfW, cy, z0}, {cx + halfW, cy, z0}, {cx + halfW, cy, z1},
                 {cx - halfW, cy, z1}},
                0.0f, white, none, {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}});
        addFace(mesh,
                {{cx, cy - halfH, z0}, {cx, cy + halfH, z0}, {cx, cy + halfH, z1},
                 {cx, cy - halfH, z1}},
                0.0f, white, none, {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}});
    };
    float lo = 1e9f, hi = -1e9f, y = 0.0f;
    for (const Nozzle& n : b.nozzles) {
        ribbon(n.x, n.y, n.halfW * 1.45f, n.halfH * 1.1f, 2.4f, 4.5f);
        lo = std::min(lo, n.x - n.halfW);
        hi = std::max(hi, n.x + n.halfW);
        y += n.y / static_cast<float>(b.nozzles.size());
    }
    // Halo large qui fond les flammes entre elles.
    ribbon((lo + hi) * 0.5f, y, (hi - lo) * 0.5f + 0.6f, 0.45f, 2.4f, 8.0f);
    return mesh;
}

// Bulle du bouclier : ellipsoïde autour du vaisseau, dessiné en additif.
MeshData buildShield() {
    MeshData mesh;
    const glm::vec3 center{0.0f, 0.55f, -0.3f};
    const glm::vec3 radii{4.1f, 1.9f, 5.0f};
    const int rings = 10, sectors = 20;
    for (int r = 0; r <= rings; r++) {
        float phi = 3.14159265f * static_cast<float>(r) / rings;
        for (int s = 0; s <= sectors; s++) {
            float theta = 6.2831853f * static_cast<float>(s) / sectors;
            glm::vec3 n{std::sin(phi) * std::cos(theta), std::cos(phi),
                        std::sin(phi) * std::sin(theta)};
            Vertex v;
            v.pos = center + n * radii;
            v.normal = glm::normalize(n / radii);
            v.kind = 1.0f;
            v.uv = {static_cast<float>(s) / sectors, static_cast<float>(r) / rings};
            mesh.add(v);
        }
    }
    for (int r = 0; r < rings; r++)
        for (int s = 0; s < sectors; s++) {
            uint32_t a = static_cast<uint32_t>(r * (sectors + 1) + s);
            uint32_t b = a + static_cast<uint32_t>(sectors) + 1;
            mesh.quad(a, a + 1, b + 1, b);
        }
    return mesh;
}

// Missile : fuseau lumineux à quatre ailettes, pointé vers -Z.
MeshData buildMissile() {
    MeshData mesh;
    const glm::vec3 core{0.0f};
    const glm::vec4 body{1.0f, 0.55f, 0.9f, 9.0f};
    const glm::vec4 fin{1.0f, 0.15f, 0.75f, 7.0f};
    const glm::vec3 nose{0.0f, 0.0f, -1.6f}, tail{0.0f, 0.0f, 1.3f};
    const float r = 0.22f;
    const glm::vec3 ring[4] = {{r, 0.0f, 0.2f}, {0.0f, r, 0.2f}, {-r, 0.0f, 0.2f}, {0.0f, -r, 0.2f}};
    for (int i = 0; i < 4; i++) {
        addFace(mesh, {nose, ring[i], ring[(i + 1) % 4]}, 0.0f, body, core);
        addFace(mesh, {tail, ring[i], ring[(i + 1) % 4]}, 0.0f, body, core);
        glm::vec3 out = ring[i] * 3.2f;
        addFace(mesh, {{ring[i].x, ring[i].y, 0.5f}, {out.x, out.y, 1.5f},
                       {ring[i].x * 0.6f, ring[i].y * 0.6f, 1.3f}},
                0.0f, fin, core + glm::vec3(1.0f, 1.0f, 0.0f) * 1e-3f);
    }
    return mesh;
}

std::vector<uint32_t> sortByChunk(MeshData& mesh, const sim::Track& track) {
    const int frameCount = static_cast<int>(track.frames.size());
    const float total = track.step * static_cast<float>(frameCount);
    const int chunks = std::max(1, static_cast<int>(std::ceil(total / kChunkLength)));

    // Grille horizontale des points de la piste, pour trouver le plus proche
    // sans tout parcourir.
    const float cell = 500.0f;
    glm::vec2 lo{1e9f}, hi{-1e9f};
    for (const sim::TrackFrame& f : track.frames) {
        lo = glm::min(lo, glm::vec2(f.pos.x, f.pos.z));
        hi = glm::max(hi, glm::vec2(f.pos.x, f.pos.z));
    }
    const int nx = static_cast<int>((hi.x - lo.x) / cell) + 1, nz = static_cast<int>((hi.y - lo.y) / cell) + 1;
    std::vector<std::vector<int>> grid(static_cast<size_t>(nx) * static_cast<size_t>(nz));
    auto cellOf = [&](float x, float z, int& cx, int& cz) {
        cx = std::clamp(static_cast<int>((x - lo.x) / cell), 0, nx - 1);
        cz = std::clamp(static_cast<int>((z - lo.y) / cell), 0, nz - 1);
    };
    for (int i = 0; i < frameCount; i += 8) {
        int cx, cz;
        cellOf(track.frames[i].pos.x, track.frames[i].pos.z, cx, cz);
        grid[static_cast<size_t>(cz) * static_cast<size_t>(nx) + static_cast<size_t>(cx)].push_back(i);
    }
    auto nearestFrame = [&](float x, float z) {
        int cx, cz;
        cellOf(x, z, cx, cz);
        int best = 0;
        float bestDist = 1e18f;
        // Anneaux de cellules de plus en plus larges ; un de plus après le
        // premier résultat, car un point plus proche peut s'y trouver.
        int stopAt = std::max(nx, nz);
        for (int ring = 0; ring <= stopAt; ring++) {
            for (int dz = -ring; dz <= ring; dz++)
                for (int dx = -ring; dx <= ring; dx++) {
                    if (std::max(std::abs(dx), std::abs(dz)) != ring) continue;
                    const int gx = cx + dx, gz = cz + dz;
                    if (gx < 0 || gz < 0 || gx >= nx || gz >= nz) continue;
                    for (int i : grid[static_cast<size_t>(gz) * static_cast<size_t>(nx) + static_cast<size_t>(gx)]) {
                        const glm::vec3& p = track.frames[i].pos;
                        const float d = (p.x - x) * (p.x - x) + (p.z - z) * (p.z - z);
                        if (d < bestDist) {
                            bestDist = d;
                            best = i;
                        }
                    }
                }
            if (bestDist < 1e17f && stopAt > ring + 1) stopAt = ring + 1;
        }
        return best;
    };

    const size_t triangles = mesh.indices.size() / 3;
    std::vector<int> chunkOf(triangles);
    std::vector<uint32_t> first(static_cast<size_t>(chunks) + 1, 0);
    for (size_t t = 0; t < triangles; t++) {
        const glm::vec3 c = (mesh.vertices[mesh.indices[t * 3]].pos + mesh.vertices[mesh.indices[t * 3 + 1]].pos +
                             mesh.vertices[mesh.indices[t * 3 + 2]].pos) / 3.0f;
        const int frame = nearestFrame(c.x, c.z);
        chunkOf[t] = std::min(chunks - 1, static_cast<int>(track.step * static_cast<float>(frame) / kChunkLength));
        first[static_cast<size_t>(chunkOf[t]) + 1] += 3;
    }
    for (int c = 0; c < chunks; c++) first[static_cast<size_t>(c) + 1] += first[static_cast<size_t>(c)];
    std::vector<uint32_t> sorted(mesh.indices.size());
    std::vector<uint32_t> cursor(first.begin(), first.end() - 1);
    for (size_t t = 0; t < triangles; t++) {
        uint32_t& at = cursor[static_cast<size_t>(chunkOf[t])];
        for (int k = 0; k < 3; k++) sorted[at + static_cast<uint32_t>(k)] = mesh.indices[t * 3 + static_cast<size_t>(k)];
        at += 3;
    }
    mesh.indices.swap(sorted);
    return first;
}

} // namespace gfx

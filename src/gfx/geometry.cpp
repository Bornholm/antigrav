#include "gfx/geometry.h"

#include <algorithm>
#include <cmath>

namespace gfx {

namespace {

const glm::vec3 kCyan{0.05f, 0.75f, 1.0f};
const glm::vec3 kMagenta{1.0f, 0.08f, 0.75f};
const glm::vec3 kViolet{0.45f, 0.15f, 1.0f};

struct Rng {
    uint32_t state;
    explicit Rng(uint32_t seed) : state(seed) {}
    float next() {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(state >> 8) / 16777216.0f;
    }
    float range(float lo, float hi) { return lo + (hi - lo) * next(); }
};

// Bande longitudinale de la piste entre deux points de la section, donnés en
// décalage latéral et hauteur.
void addStrip(MeshData& mesh, const sim::Track& track, float lat0, float h0, float lat1,
              float h1, float kind, const glm::vec4& color) {
    const int count = static_cast<int>(track.frames.size());
    uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
    for (int i = 0; i <= count; i++) {
        const sim::TrackFrame& f = track.frames[i % count];
        float dist = track.step * static_cast<float>(i);
        Vertex a, b;
        a.pos = f.pos + f.right * lat0;
        a.height = h0;
        a.normal = f.up;
        a.kind = kind;
        a.uv = {0.0f, dist};
        a.color = color;
        b = a;
        b.pos = f.pos + f.right * lat1;
        b.height = h1;
        b.uv = {1.0f, dist};
        mesh.add(a);
        mesh.add(b);
    }
    for (int i = 0; i < count; i++) {
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
    for (int i = 0; i <= count; i++) {
        const sim::TrackFrame& f = track.frames[i % count];
        float dist = track.step * static_cast<float>(i);
        for (float lat : lats) {
            Vertex v;
            v.pos = f.pos + f.right * lat;
            v.normal = f.up;
            v.uv = {lat, dist};
            v.color = {hw, track.length, 0.0f, 0.0f};
            mesh.add(v);
        }
    }
    for (int i = 0; i < count; i++) {
        uint32_t v = static_cast<uint32_t>(i) * 3;
        mesh.quad(v, v + 1, v + 4, v + 3);
        mesh.quad(v + 1, v + 2, v + 5, v + 4);
    }
    return mesh;
}

MeshData buildRails(const sim::Track& track) {
    MeshData mesh;
    const float hw = track.halfWidth;
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

// Portiques lumineux au-dessus de la piste : celui de la ligne de départ, puis
// un tous les 900 m environ. Ils rythment le défilement.
MeshData buildGates(const sim::Track& track) {
    MeshData mesh;
    const float hw = track.halfWidth;
    const int gateCount = std::max(1, static_cast<int>(std::round(track.length / 900.0f)));

    for (int g = 0; g < gateCount; g++) {
        float dist = track.length * static_cast<float>(g) / static_cast<float>(gateCount);
        bool start = g == 0;
        glm::vec3 tint = start ? glm::vec3(1.0f, 0.85f, 0.35f) : (g % 2 ? kMagenta : kCyan);
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
    return mesh;
}

MeshData buildCity(const sim::Track& track) {
    MeshData mesh;
    Rng rng(20261124u);

    glm::vec3 lo{1e9f}, hi{-1e9f};
    for (const auto& f : track.frames) {
        lo = glm::min(lo, f.pos);
        hi = glm::max(hi, f.pos);
    }
    const float margin = 1900.0f;
    const float baseY = lo.y - 90.0f;
    const int frameCount = static_cast<int>(track.frames.size());

    int placed = 0;
    for (int attempt = 0; attempt < 40000 && placed < 1500; attempt++) {
        float x = rng.range(lo.x - margin, hi.x + margin);
        float z = rng.range(lo.z - margin, hi.z + margin);
        float width = rng.range(28.0f, 85.0f);
        float depth = rng.range(28.0f, 85.0f);

        // Les tours restent à l'écart de la piste.
        float nearest = 1e9f;
        for (int i = 0; i < frameCount; i += 10) {
            const glm::vec3& p = track.frames[i].pos;
            float dx = p.x - x, dz = p.z - z;
            nearest = std::min(nearest, dx * dx + dz * dz);
        }
        nearest = std::sqrt(nearest);
        float clearance = 230.0f + std::max(width, depth);
        // Ni trop près, ni si loin que la brume les efface.
        if (nearest < clearance || nearest > 1900.0f) continue;

        float t = rng.next();
        float height = 70.0f + 380.0f * t * t;
        // Plus basses près de la piste, pour dégager la vue.
        height *= std::clamp(nearest / 700.0f, 0.4f, 1.0f);
        float top = baseY + height + rng.range(60.0f, 110.0f);
        float tallness = top - baseY;

        float pick = rng.next();
        glm::vec3 tint = pick < 0.45f ? kCyan : (pick < 0.85f ? kMagenta : kViolet);
        float seed = rng.range(0.0f, 100.0f);
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

        // Panneau lumineux sur une tour sur cinq, tourné vers la piste.
        if (rng.next() < 0.22f) {
            float panelH = rng.range(22.0f, 55.0f);
            float y0 = baseY + tallness * rng.range(0.45f, 0.75f);
            float y1 = std::min(y0 + panelH, top - 6.0f);
            glm::vec3 pTint = rng.next() < 0.7f ? kMagenta : kCyan;
            glm::vec4 pColor{pTint, 3.2f};
            const float off = 0.8f;
            glm::vec3 trackCenter = (lo + hi) * 0.5f;
            bool faceX = std::abs(trackCenter.x - x) > std::abs(trackCenter.z - z);
            if (faceX) {
                float px = trackCenter.x > x ? x1 + off : x0 - off;
                float a = z0 + depth * 0.12f, b = z1 - depth * 0.12f;
                addFace(mesh, {{px, y0, a}, {px, y0, b}, {px, y1, b}, {px, y1, a}}, -1.0f,
                        pColor, center,
                        {{0.0f, y0}, {1.0f, y0}, {1.0f, y1}, {0.0f, y1}});
            } else {
                float pz = trackCenter.z > z ? z1 + off : z0 - off;
                float a = x0 + width * 0.12f, b = x1 - width * 0.12f;
                addFace(mesh, {{a, y0, pz}, {b, y0, pz}, {b, y1, pz}, {a, y1, pz}}, -1.0f,
                        pColor, center,
                        {{0.0f, y0}, {1.0f, y0}, {1.0f, y1}, {0.0f, y1}});
            }
        }
        placed++;
    }
    return mesh;
}

// Repère du vaisseau : +X à droite, +Y en haut, -Z vers l'avant.
// Silhouette en flèche : fuselage central relevé, ailes en deux panneaux dont
// le second plonge et dépasse derrière les tuyères.
MeshData buildShip() {
    MeshData mesh;
    const glm::vec3 core{0.0f, 0.5f, 0.0f};
    const glm::vec3 below{0.0f, -10.0f, 0.0f};
    const glm::vec4 hullLight{0.75f, 0.80f, 1.0f, 1.0f};
    const glm::vec4 hullMid{0.42f, 0.46f, 0.66f, 1.0f};
    const glm::vec4 hullDark{0.20f, 0.22f, 0.36f, 1.0f};
    const glm::vec4 white{1.0f};

    for (float s : {-1.0f, 1.0f}) {
        auto P = [s](float x, float y, float z) { return glm::vec3(x * s, y, z); };
        auto strip = [&](glm::vec3 a, glm::vec3 b, float width) {
            // Liseré lumineux posé juste au-dessus d'une arête.
            glm::vec3 along = glm::normalize(b - a);
            glm::vec3 side = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), along)) * width;
            if (glm::dot(side, glm::vec3(s, 0.0f, 0.0f)) > 0.0f) side = -side;
            const glm::vec3 lift{0.0f, 0.03f, 0.0f};
            addFace(mesh, {a + lift, b + lift, b + lift + side, a + lift + side}, 1.0f, white,
                    below);
        };

        // Fuselage.
        glm::vec3 nose = P(0.0f, 0.36f, -4.0f);
        glm::vec3 noseLow = P(0.0f, 0.10f, -3.5f);
        glm::vec3 canopyFront = P(0.0f, 0.62f, -2.2f);
        glm::vec3 canopyTop = P(0.0f, 1.02f, -0.6f);
        glm::vec3 spine = P(0.0f, 1.12f, 1.75f);
        glm::vec3 shoulder = P(0.62f, 0.46f, -1.9f);
        glm::vec3 flank = P(0.92f, 0.62f, -0.2f);
        glm::vec3 rearTop = P(1.02f, 0.92f, 1.95f);
        glm::vec3 rearLow = P(1.12f, 0.06f, 2.05f);
        glm::vec3 rearLowMid = P(0.0f, 0.06f, 2.05f);
        glm::vec3 belly = P(0.50f, 0.05f, -2.0f);

        addFace(mesh, {nose, shoulder, canopyFront}, 0.0f, hullMid, core);
        addFace(mesh, {canopyFront, shoulder, flank, canopyTop}, 0.0f, hullDark, core);
        addFace(mesh, {canopyTop, flank, rearTop, spine}, 0.0f, hullLight, core);
        addFace(mesh, {nose, noseLow, belly, shoulder}, 0.0f, hullDark, core);
        addFace(mesh, {shoulder, belly, rearLow, flank}, 0.0f, hullMid, core);
        addFace(mesh, {flank, rearLow, rearTop}, 0.0f, hullMid, core);
        addFace(mesh, {spine, rearTop, rearLow, rearLowMid}, 0.0f, hullDark, core);
        addFace(mesh, {noseLow, belly, rearLow, rearLowMid}, 0.0f, hullDark, core);

        // Aile : panneau intérieur presque plat, panneau extérieur qui plonge.
        glm::vec3 rootFront = P(0.80f, 0.50f, -1.3f);
        glm::vec3 rootRear = P(1.05f, 0.62f, 1.95f);
        glm::vec3 midFront = P(1.75f, 0.46f, 0.35f);
        glm::vec3 midRear = P(1.85f, 0.50f, 2.30f);
        glm::vec3 tipFront = P(2.75f, 0.14f, 1.75f);
        glm::vec3 tipRear = P(2.95f, 0.10f, 3.05f);
        const glm::vec3 thick{0.0f, -0.20f, 0.0f};

        addFace(mesh, {rootFront, midFront, midRear, rootRear}, 0.0f, hullMid, below);
        addFace(mesh, {midFront, tipFront, tipRear, midRear}, 0.0f, hullLight, below);
        // Tranche arrière, visible depuis la caméra de poursuite.
        addFace(mesh, {rootRear, midRear, midRear + thick, rootRear + thick}, 0.0f, hullDark,
                core);
        addFace(mesh, {midRear, tipRear, tipRear + thick * 0.4f, midRear + thick}, 0.0f,
                hullDark, core);
        // Dessous.
        addFace(mesh, {rootFront + thick, midFront + thick, midRear + thick, rootRear + thick},
                0.0f, hullDark, core + glm::vec3(0.0f, 10.0f, 0.0f));

        // Dérive en bout d'aile.
        glm::vec3 finFront = P(2.72f, 0.15f, 2.0f);
        glm::vec3 finTop = P(2.90f, 0.95f, 3.1f);
        addFace(mesh, {finFront, tipRear, finTop}, 0.0f, hullMid, core);

        // Liserés.
        strip(nose, shoulder, 0.07f);
        strip(shoulder, flank, 0.07f);
        strip(flank, rearTop, 0.07f);
        strip(midFront, tipFront, 0.10f);
        strip(tipFront, tipRear, 0.10f);
        strip(rootFront, midFront, 0.08f);
        strip(midRear, tipRear, 0.10f);
        addFace(mesh, {finFront + P(0.03f, 0.0f, 0.0f), finTop + P(0.03f, 0.0f, 0.0f),
                       finTop + P(0.03f, -0.12f, -0.05f), finFront + P(0.03f, 0.0f, 0.16f)},
                1.0f, white, core);

        // Tuyères.
        for (float cx : {0.33f, 0.76f}) {
            float x0 = cx - 0.17f, x1 = cx + 0.17f;
            addFace(mesh,
                    {P(x0, 0.16f, 2.07f), P(x1, 0.16f, 2.07f), P(x1, 0.58f, 2.02f),
                     P(x0, 0.58f, 2.02f)},
                    2.0f, white, core, {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}});
        }
    }

    // Barre lumineuse arrière.
    addFace(mesh,
            {{-0.70f, 0.72f, 1.99f}, {0.70f, 0.72f, 1.99f}, {0.70f, 0.83f, 1.98f},
             {-0.70f, 0.83f, 1.98f}},
            1.0f, white, core);
    return mesh;
}

MeshData buildTrail() {
    MeshData mesh;
    const glm::vec4 white{1.0f};
    const glm::vec3 none{0.0f, -100.0f, 0.0f};
    auto ribbon = [&](float cx, float cy, float halfW, float halfH, float length) {
        float z0 = 2.06f, z1 = 2.06f + length;
        addFace(mesh,
                {{cx - halfW, cy, z0}, {cx + halfW, cy, z0}, {cx + halfW, cy, z1},
                 {cx - halfW, cy, z1}},
                0.0f, white, none, {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}});
        addFace(mesh,
                {{cx, cy - halfH, z0}, {cx, cy + halfH, z0}, {cx, cy + halfH, z1},
                 {cx, cy - halfH, z1}},
                0.0f, white, none, {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}});
    };
    for (float cx : {-0.76f, -0.33f, 0.33f, 0.76f}) ribbon(cx, 0.34f, 0.24f, 0.22f, 4.5f);
    // Halo large qui fond les quatre flammes.
    ribbon(0.0f, 0.34f, 1.5f, 0.45f, 8.0f);
    return mesh;
}

} // namespace gfx

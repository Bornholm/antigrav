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
            v.color = {hw, 0.0f, 0.0f, 0.0f};
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

MeshData buildCity(const sim::Track& track) {
    MeshData mesh;
    Rng rng(20261124u);

    glm::vec3 lo{1e9f}, hi{-1e9f};
    for (const auto& f : track.frames) {
        lo = glm::min(lo, f.pos);
        hi = glm::max(hi, f.pos);
    }
    const float margin = 1500.0f;
    const float baseY = lo.y - 90.0f;
    const int frameCount = static_cast<int>(track.frames.size());

    int placed = 0;
    for (int attempt = 0; attempt < 6000 && placed < 520; attempt++) {
        float x = rng.range(lo.x - margin, hi.x + margin);
        float z = rng.range(lo.z - margin, hi.z + margin);
        float width = rng.range(28.0f, 85.0f);
        float depth = rng.range(28.0f, 85.0f);

        // Les tours restent à l'écart de la piste.
        float nearest = 1e9f;
        for (int i = 0; i < frameCount; i += 6) {
            const glm::vec3& p = track.frames[i].pos;
            float dx = p.x - x, dz = p.z - z;
            nearest = std::min(nearest, dx * dx + dz * dz);
        }
        nearest = std::sqrt(nearest);
        float clearance = 230.0f + std::max(width, depth);
        if (nearest < clearance) continue;

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
MeshData buildShip() {
    MeshData mesh;
    const glm::vec3 core{0.0f, 0.45f, 0.0f};
    const glm::vec4 hull{0.55f, 0.6f, 0.8f, 1.0f};
    const glm::vec4 white{1.0f};

    for (float s : {-1.0f, 1.0f}) {
        auto P = [s](float x, float y, float z) { return glm::vec3(x * s, y, z); };
        glm::vec3 nose = P(0.0f, 0.34f, -3.7f);
        glm::vec3 noseLow = P(0.0f, 0.10f, -3.3f);
        glm::vec3 canopy = P(0.0f, 0.78f, -1.1f);
        glm::vec3 spine = P(0.0f, 0.98f, 1.7f);
        glm::vec3 shoulder = P(0.58f, 0.44f, -1.6f);
        glm::vec3 rearTop = P(0.98f, 0.80f, 1.95f);
        glm::vec3 rearLow = P(1.08f, 0.08f, 2.05f);
        glm::vec3 rearLowMid = P(0.0f, 0.08f, 2.05f);
        glm::vec3 belly = P(0.48f, 0.06f, -1.8f);

        // Fuselage.
        addFace(mesh, {nose, shoulder, canopy}, 0.0f, hull, core);
        addFace(mesh, {canopy, shoulder, rearTop, spine}, 0.0f, hull, core);
        addFace(mesh, {nose, noseLow, belly, shoulder}, 0.0f, hull, core);
        addFace(mesh, {shoulder, belly, rearLow, rearTop}, 0.0f, hull, core);
        addFace(mesh, {spine, rearTop, rearLow, rearLowMid}, 0.0f, hull, core);
        addFace(mesh, {noseLow, belly, rearLow, rearLowMid}, 0.0f, hull, core);

        // Aile en delta.
        glm::vec3 wingRootFront = P(0.72f, 0.36f, -0.9f);
        glm::vec3 wingTipFront = P(3.0f, 0.20f, 1.25f);
        glm::vec3 wingTipRear = P(3.1f, 0.18f, 2.35f);
        glm::vec3 wingRootRear = P(1.05f, 0.30f, 2.05f);
        const glm::vec3 below{0.0f, -10.0f, 0.0f};
        addFace(mesh, {wingRootFront, wingTipFront, wingTipRear, wingRootRear}, 0.0f, hull,
                below);

        // Dérive en bout d'aile.
        glm::vec3 finBase = P(2.98f, 0.20f, 1.5f);
        glm::vec3 finTop = P(3.08f, 1.05f, 2.45f);
        addFace(mesh, {finBase, wingTipRear, finTop}, 0.0f, hull, core);

        // Liserés lumineux : arêtes du fuselage, bord d'attaque, bord de fuite,
        // arête de dérive.
        const glm::vec3 lift{0.0f, 0.025f, 0.0f};
        const glm::vec3 back{0.0f, 0.0f, 0.13f};
        auto edge = [&](glm::vec3 a, glm::vec3 b) {
            glm::vec3 w = P(0.07f, 0.0f, 0.0f);
            addFace(mesh, {a + lift, b + lift, b + lift + w, a + lift + w}, 1.0f, white, below);
        };
        edge(nose, shoulder);
        edge(shoulder, rearTop);
        addFace(mesh, {wingRootFront + lift, wingTipFront + lift, wingTipFront + lift + back,
                       wingRootFront + lift + back},
                1.0f, white, below);
        addFace(mesh, {wingRootRear + lift, wingTipRear + lift, wingTipRear + lift - back,
                       wingRootRear + lift - back},
                1.0f, white, below);
        addFace(mesh, {finBase + P(0.03f, 0.0f, 0.0f), finTop + P(0.03f, 0.0f, 0.0f),
                       finTop + P(0.03f, 0.0f, -0.12f), finBase + P(0.03f, 0.0f, -0.12f)},
                1.0f, white, core);

        // Tuyères.
        for (float cx : {0.33f, 0.76f}) {
            float x0 = cx - 0.17f, x1 = cx + 0.17f;
            addFace(mesh,
                    {P(x0, 0.18f, 2.07f), P(x1, 0.18f, 2.07f), P(x1, 0.50f, 2.03f),
                     P(x0, 0.50f, 2.03f)},
                    2.0f, white, core, {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}});
        }
    }

    // Barre lumineuse arrière.
    addFace(mesh,
            {{-0.62f, 0.62f, 2.0f}, {0.62f, 0.62f, 2.0f}, {0.62f, 0.72f, 1.99f},
             {-0.62f, 0.72f, 1.99f}},
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

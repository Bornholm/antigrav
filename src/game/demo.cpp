#include "game/demo.h"

#include <algorithm>
#include <cmath>

namespace game {

namespace {

const glm::vec3 kCyan{0.05f, 0.75f, 1.0f};
const glm::vec3 kMagenta{1.0f, 0.08f, 0.75f};
const glm::vec3 kPink{1.0f, 0.25f, 0.55f};

struct ShipSetup {
    const char* name;
    float ahead;   // mètres devant le joueur
    float lateral; // décalage latéral moyen
    float sway;    // amplitude du balancement latéral
    float phase;
    glm::vec3 trim;
    glm::vec3 engine;
};

const ShipSetup kShips[] = {
    {"", 0.0f, 0.0f, 0.6f, 0.0f, kMagenta, kCyan},
    {"SynthFox", 24.0f, -9.5f, 1.0f, 1.3f, kMagenta, kPink},
    {"KazeVR", 70.0f, -1.5f, 1.5f, 2.9f, kPink, kMagenta},
    {"NovaBlade", 36.0f, 9.0f, 1.0f, 4.4f, kCyan, kCyan},
};

} // namespace

void Demo::init() {
    track = sim::Track::makeDemo();

    // Tracé vu de dessus pour la minimap, normalisé en gardant les proportions.
    glm::vec2 lo{1e9f}, hi{-1e9f};
    const int count = static_cast<int>(track.frames.size());
    const int stride = std::max(1, count / 140);
    for (int i = 0; i < count; i += stride) {
        glm::vec2 p{track.frames[i].pos.x, track.frames[i].pos.z};
        minimap.push_back(p);
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    glm::vec2 center = (lo + hi) * 0.5f;
    float extent = std::max(hi.x - lo.x, hi.y - lo.y) * 0.5f;
    for (glm::vec2& p : minimap) p = (p - center) / extent;
}

void Demo::fill(float time, const gfx::FontAtlas& fonts, glm::vec2 screen,
                gfx::FrameParams& out) {
    const float hover = 0.55f;
    float playerDist = 150.0f + speed * time;

    out.time = time;
    out.speed = speed;
    out.shipCount = 0;

    HudState state;
    float playerLateral = 0.0f;
    for (const ShipSetup& setup : kShips) {
        float dist = playerDist + setup.ahead + std::sin(time * 0.6f + setup.phase) * 3.0f;
        float lateral = setup.lateral + std::sin(time * 0.9f + setup.phase) * setup.sway;
        float roll = std::cos(time * 0.9f + setup.phase) * setup.sway * 0.06f;
        float bob = std::sin(time * 3.1f + setup.phase) * 0.05f;
        if (setup.ahead == 0.0f) playerLateral = lateral;

        sim::TrackFrame f = track.at(dist);
        glm::vec3 up = glm::normalize(f.up * std::cos(roll) + f.right * std::sin(roll));
        glm::vec3 right = glm::cross(f.tangent, up);
        glm::vec3 pos = f.pos + f.right * lateral + f.up * (hover + bob);

        gfx::ShipDraw& ship = out.ships[out.shipCount++];
        // Le maillage regarde vers -Z, donc son axe +Z pointe vers l'arrière.
        const float scale = 1.3f;
        ship.model = glm::mat4(glm::vec4(right * scale, 0.0f), glm::vec4(up * scale, 0.0f),
                               glm::vec4(-f.tangent * scale, 0.0f), glm::vec4(pos, 1.0f));
        ship.planePoint = f.pos;
        ship.planeNormal = f.up;
        ship.trim = glm::vec4(setup.trim, 6.0f);
        ship.engine = glm::vec4(setup.engine, 9.0f);

        if (setup.ahead > 0.0f) {
            NameTag& tag = state.tags[state.tagCount++];
            tag.worldPos = pos + f.up * 3.4f;
            tag.name = setup.name;
            tag.color = glm::vec4(glm::mix(setup.engine, glm::vec3(1.0f), 0.25f), 1.0f);
        }
    }

    // Classement : le plus loin devant est premier.
    for (int i = 0; i < state.tagCount; i++) {
        int rank = 1;
        for (int j = 0; j < state.tagCount; j++)
            if (kShips[j + 1].ahead > kShips[i + 1].ahead) rank++;
        state.tags[i].rank = rank;
    }

    // Caméra de poursuite : derrière et au-dessus du joueur, inclinée avec la
    // piste, et qui regarde loin devant.
    sim::TrackFrame camFrame = track.at(playerDist - 9.0f);
    out.camPos = camFrame.pos + camFrame.right * (playerLateral * 0.7f) + camFrame.up * 4.4f;
    out.camTarget = track.point(playerDist + 26.0f, playerLateral * 0.4f, 0.0f);
    out.camUp = camFrame.up;
    out.fovY = 1.12f;
    out.planePoint = camFrame.pos;
    out.planeNormal = camFrame.up;

    state.energy = 0.87f + 0.05f * std::sin(time * 0.7f);
    state.position = state.tagCount + 1;
    state.racers = state.tagCount + 1;
    state.lap = 2;
    state.laps = 3;
    state.speedKmh = speed * 3.6f + 1.0f + 6.0f * std::sin(time * 1.3f);
    state.minimap = &minimap;
    state.progress = std::fmod(playerDist, track.length) / track.length;

    hud.begin(fonts, screen.x, screen.y);
    buildHud(hud, state, gfx::viewProjection(out, screen.x / screen.y));
    out.hud = &hud;
}

} // namespace game

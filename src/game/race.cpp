#include "game/race.h"

#include <algorithm>
#include <cmath>

namespace game {

namespace {

const glm::vec3 kCyan{0.05f, 0.75f, 1.0f};
const glm::vec3 kMagenta{1.0f, 0.08f, 0.75f};
const glm::vec3 kPink{1.0f, 0.25f, 0.55f};

struct Racer {
    const char* name;
    float startAhead; // mètres devant le joueur sur la grille
    float startLateral;
    float lane;       // couloir préféré du pilote automatique
    float topSpeed;   // facteur sur la vitesse de croisière
    glm::vec3 trim;
    glm::vec3 engine;
};

// Le joueur est l'indice 0.
const Racer kRacers[] = {
    {"", 0.0f, 0.0f, 0.0f, 1.0f, kMagenta, kCyan},
    {"SynthFox", 24.0f, -9.5f, -5.0f, 0.985f, kMagenta, kPink},
    {"KazeVR", 70.0f, -1.5f, 0.0f, 1.0f, kPink, kMagenta},
    {"NovaBlade", 36.0f, 9.0f, 5.0f, 0.992f, kCyan, kCyan},
};
constexpr int kRacerCount = sizeof(kRacers) / sizeof(kRacers[0]);
constexpr float kHover = 0.55f;
constexpr float kShipScale = 1.15f;

float mixf(float a, float b, float t) { return a + (b - a) * t; }

} // namespace

void Race::init(bool gridStart) {
    track = sim::Track::makeDemo();

    world = sim::World{};
    world.count = kRacerCount;
    world.laps = laps;
    world.countdown = gridStart ? 3.0f : 0.0f;
    for (int i = 0; i < kRacerCount; i++) {
        sim::Vehicle& v = world.vehicles[i];
        v.lateral = kRacers[i].startLateral;
        v.topSpeed = sim::kCruiseSpeed * kRacers[i].topSpeed;
        if (gridStart) {
            // Grille juste derrière la ligne, le joueur en dernière position.
            v.dist = -80.0f + kRacers[i].startAhead * 0.5f;
            v.speed = 0.0f;
            v.energy = 1.0f;
        } else {
            v.dist = 150.0f + kRacers[i].startAhead;
            v.speed = v.topSpeed;
            v.energy = 0.87f;
        }
    }
    previous = world;

    // Tracé vu de dessus pour la minimap, normalisé en gardant les proportions.
    glm::vec2 lo{1e9f}, hi{-1e9f};
    const int count = static_cast<int>(track.frames.size());
    const int stride = std::max(1, count / 160);
    minimap.clear();
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

void Race::update(float dt, const Controls& input) {
    controls = input;
    // Une image très longue ne doit pas déclencher une rafale de pas.
    accumulator += std::min(dt, 0.1f);
    while (accumulator >= sim::kFixedDt) {
        previous = world;
        sim::Input inputs[sim::kMaxVehicles];
        for (int i = 0; i < world.count; i++)
            inputs[i] = sim::autopilot(world.vehicles[i], track, kRacers[i].lane);
        // Après l'arrivée, le pilote automatique reprend la main.
        if (!autopilot && !world.vehicles[0].finished) {
            inputs[0].steer = input.steer;
            inputs[0].boost = input.pressed[static_cast<int>(Button::Boost)];
        }
        sim::step(world, track, inputs);
        accumulator -= sim::kFixedDt;
    }

    // Caméra : suit le joueur avec un léger retard, s'élargit avec la vitesse.
    const sim::Vehicle& player = world.vehicles[0];
    float follow = 1.0f - std::exp(-dt * 6.0f);
    camLateral += (player.lateral - camLateral) * follow;
    float speedRatio = std::clamp((player.speed - 250.0f) / (sim::kBoostSpeed - 250.0f), 0.0f, 1.2f);
    float fovTarget = 1.02f + 0.20f * speedRatio + (player.boosting ? 0.06f : 0.0f);
    camFov += (fovTarget - camFov) * (1.0f - std::exp(-dt * 3.0f));
    float shakeTarget = player.scraping ? 1.0f : (player.boosting ? 0.25f : 0.0f);
    camShake += (shakeTarget - camShake) * (1.0f - std::exp(-dt * 10.0f));
}

void Race::fill(const gfx::FontAtlas& fonts, glm::vec2 screen, gfx::FrameParams& out) {
    const float alpha = accumulator / sim::kFixedDt;
    const float time = static_cast<float>(world.time) + accumulator;

    out.time = time;
    out.shipCount = 0;

    HudState state;
    float playerDist = 0.0f;
    for (int i = 0; i < world.count; i++) {
        const sim::Vehicle& a = previous.vehicles[i];
        const sim::Vehicle& b = world.vehicles[i];
        float dist = mixf(a.dist, b.dist, alpha);
        float lateral = mixf(a.lateral, b.lateral, alpha);
        float roll = mixf(a.roll, b.roll, alpha);
        float bob = std::sin(time * 3.1f + static_cast<float>(i) * 1.7f) * 0.05f;
        if (i == 0) playerDist = dist;

        sim::TrackFrame f = track.at(dist);
        glm::vec3 up = glm::normalize(f.up * std::cos(roll) + f.right * std::sin(roll));
        glm::vec3 right = glm::cross(f.tangent, up);
        glm::vec3 pos = f.pos + f.right * lateral + f.up * (kHover + bob);

        gfx::ShipDraw& ship = out.ships[out.shipCount++];
        // Le maillage regarde vers -Z, donc son axe +Z pointe vers l'arrière.
        ship.model = glm::mat4(glm::vec4(right * kShipScale, 0.0f),
                               glm::vec4(up * kShipScale, 0.0f),
                               glm::vec4(-f.tangent * kShipScale, 0.0f), glm::vec4(pos, 1.0f));
        ship.planePoint = f.pos;
        ship.planeNormal = f.up;
        ship.trim = glm::vec4(kRacers[i].trim, 6.0f);
        ship.engine = glm::vec4(kRacers[i].engine, b.boosting ? 16.0f : 9.0f);

        if (i > 0) {
            NameTag& tag = state.tags[state.tagCount++];
            tag.worldPos = pos + f.up * 3.4f;
            tag.name = kRacers[i].name;
            tag.rank = sim::rankOf(world, i);
            tag.color = glm::vec4(glm::mix(kRacers[i].engine, glm::vec3(1.0f), 0.25f), 1.0f);
        }
    }

    const sim::Vehicle& player = world.vehicles[0];
    out.speed = player.speed;

    // Caméra de poursuite : derrière et au-dessus du joueur, inclinée avec la
    // piste, et qui regarde loin devant.
    sim::TrackFrame camFrame = track.at(playerDist - 10.0f);
    glm::vec3 shake = camFrame.right * (std::sin(time * 71.0f) * 0.05f * camShake) +
                      camFrame.up * (std::sin(time * 83.0f) * 0.04f * camShake);
    out.camPos = camFrame.pos + camFrame.right * (camLateral * 0.82f) + camFrame.up * 4.8f + shake;
    out.camTarget = track.point(playerDist + 26.0f, camLateral * 0.55f, 0.0f);
    out.camUp = camFrame.up;
    out.fovY = camFov;
    out.planePoint = camFrame.pos;
    out.planeNormal = camFrame.up;

    state.energy = player.energy;
    state.position = sim::rankOf(world, 0);
    state.racers = world.count;
    state.lap = sim::lapOf(world, track, player);
    state.laps = laps;
    state.countdown = world.countdown;
    state.raceTime = player.finished ? player.finishTime : world.raceTime;
    state.finished = player.finished;
    state.speedKmh = player.speed * 3.6f;
    state.minimap = &minimap;
    float wrapped = std::fmod(playerDist, track.length);
    if (wrapped < 0.0f) wrapped += track.length;
    state.progress = wrapped / track.length;
    state.boosting = player.boosting;
    for (int b = 0; b < static_cast<int>(Button::Count); b++) state.pressed[b] = controls.pressed[b];

    hud.begin(fonts, screen.x, screen.y);
    buildHud(hud, state, gfx::viewProjection(out, screen.x / screen.y));
    out.hud = &hud;
}

} // namespace game

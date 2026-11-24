#include "game/race.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace game {

namespace {

// Couleurs d'un emplacement sur la grille : liserés, puis réacteurs.
struct SlotColors {
    glm::vec3 trim;
    glm::vec3 engine;
};
const SlotColors kSlotColors[sim::kMaxVehicles] = {
    {{1.0f, 0.08f, 0.75f}, {0.05f, 0.75f, 1.0f}},  // magenta, cyan
    {{0.05f, 0.75f, 1.0f}, {0.05f, 0.75f, 1.0f}},  // cyan
    {{1.0f, 0.55f, 0.08f}, {1.0f, 0.25f, 0.10f}},  // ambre
    {{0.35f, 1.0f, 0.30f}, {0.60f, 1.0f, 0.20f}},  // vert
    {{1.0f, 0.25f, 0.55f}, {1.0f, 0.08f, 0.75f}},  // rose
    {{0.55f, 0.35f, 1.0f}, {0.45f, 0.20f, 1.0f}},  // violet
    {{1.0f, 0.95f, 0.30f}, {1.0f, 0.70f, 0.10f}},  // jaune
    {{0.90f, 0.95f, 1.0f}, {0.60f, 0.80f, 1.0f}},  // blanc
};

constexpr float kHover = 0.55f;
constexpr float kShipScale = 1.15f;

float mixf(float a, float b, float t) { return a + (b - a) * t; }

// Couloir préféré d'un pilote automatique, réparti sur la largeur.
float laneOf(int index, int count, float halfWidth) {
    if (count <= 1) return 0.0f;
    float t = static_cast<float>(index) / static_cast<float>(count - 1);
    return (t - 0.5f) * halfWidth * 0.75f;
}

} // namespace

RaceConfig defaultConfig(int track, bool gridStart) {
    RaceConfig config;
    config.track = track;
    config.laps = sim::trackInfo(track).laps;
    config.gridStart = gridStart;
    config.racerCount = 4;
    const char* names[4] = {"Vous", "SynthFox", "KazeVR", "NovaBlade"};
    for (int i = 0; i < 4; i++) {
        std::snprintf(config.racers[i].name, sizeof(config.racers[i].name), "%s", names[i]);
        config.racers[i].ship = i % sim::kShipCount;
        config.racers[i].profile = i == 0 ? kHuman : (i - 1) % sim::kProfileCount;
    }
    return config;
}

void Race::init(const RaceConfig& cfg, int local) {
    config = cfg;
    localIndex = std::clamp(local, 0, std::max(0, cfg.racerCount - 1));
    track = sim::Track::make(cfg.track);
    accumulator = 0.0f;
    camLateral = 0.0f;

    world = sim::World{};
    world.count = cfg.racerCount;
    world.laps = cfg.laps;
    world.countdown = cfg.gridStart ? 3.0f : 0.0f;
    const float side = track.halfWidth * 0.42f;
    for (int i = 0; i < cfg.racerCount; i++) {
        sim::Vehicle& v = world.vehicles[i];
        v.ship = cfg.racers[i].ship;
        // Deux par ligne, en quinconce.
        int row = i / 2;
        v.lateral = (i % 2 == 0 ? -side : side);
        if (cfg.gridStart) {
            v.dist = -40.0f - 24.0f * static_cast<float>(row) - (i % 2 ? 8.0f : 0.0f);
            v.speed = 0.0f;
            v.energy = 1.0f;
        } else {
            v.dist = 150.0f + 26.0f * static_cast<float>(cfg.racerCount - 1 - i);
            v.speed = sim::cruiseSpeed(v);
            v.energy = 0.87f;
        }
    }
    previous = world;
    for (int i = 0; i < sim::kMaxVehicles; i++) {
        remoteInputs[i] = sim::Input{};
        lastInputs[i] = sim::Input{};
        smoothDist[i] = smoothLateral[i] = 0.0f;
    }

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

void Race::gatherInputs(const Controls& input, sim::Input* out) {
    for (int i = 0; i < world.count; i++) {
        const RacerSetup& racer = config.racers[i];
        bool local = i == localIndex;
        // Après l'arrivée, le pilote automatique reprend la main.
        bool ai = racer.profile != kHuman || (local && autopilot) || world.vehicles[i].finished;
        if (ai) {
            int profile = racer.profile != kHuman ? racer.profile : 2;
            out[i] = sim::autopilot(world, i, track, sim::aiProfile(profile),
                                    laneOf(i, world.count, track.halfWidth));
        } else if (local) {
            out[i] = sim::Input{};
            out[i].steer = input.steer;
            out[i].boost = input.pressed[static_cast<int>(Button::Boost)];
            out[i].fire = input.pressed[static_cast<int>(Button::Missile)];
            out[i].shield = input.pressed[static_cast<int>(Button::Shield)];
        } else {
            out[i] = remoteInputs[i];
        }
    }
}

void Race::update(float dt, const Controls& input) {
    controls = input;
    if (!externalSim) {
        // Une image très longue ne doit pas déclencher une rafale de pas.
        accumulator += std::min(dt, 0.1f);
        while (accumulator >= sim::kFixedDt) {
            previous = world;
            float countdownBefore = world.countdown;
            gatherInputs(input, lastInputs);
            sim::step(world, track, lastInputs);
            accumulator -= sim::kFixedDt;

            // Un bip à chaque seconde du compte à rebours, un signal au départ.
            if (synth && countdownBefore > 0.0f) {
                if (world.countdown <= 0.0f) synth->beep(1);
                else if (std::ceil(world.countdown) != std::ceil(countdownBefore) ||
                         countdownBefore == 3.0f)
                    synth->beep(0);
            }
        }
    }
    updatePresentation(dt);
}

void Race::updatePresentation(float dt) {
    const sim::Vehicle& player = world.vehicles[localIndex];
    if (synth) synth->setEngine(player.speed, player.boosting, player.scraping);

    // Caméra : suit le joueur avec un léger retard, s'élargit avec la vitesse.
    float follow = 1.0f - std::exp(-dt * 6.0f);
    camLateral += (player.lateral - camLateral) * follow;
    float speedRatio = std::clamp((player.speed - 250.0f) / (sim::kBoostSpeed - 250.0f), 0.0f, 1.2f);
    float fovTarget = 1.02f + 0.20f * speedRatio + (player.boosting ? 0.06f : 0.0f);
    camFov += (fovTarget - camFov) * (1.0f - std::exp(-dt * 3.0f));
    float shakeTarget = player.scraping ? 1.0f : (player.boosting ? 0.25f : 0.0f);
    camShake += (shakeTarget - camShake) * (1.0f - std::exp(-dt * 10.0f));

    float decay = std::exp(-dt * 10.0f);
    for (int i = 0; i < world.count; i++) {
        smoothDist[i] *= decay;
        smoothLateral[i] *= decay;
    }
}

void Race::fill(const gfx::FontAtlas& fonts, glm::vec2 screen, gfx::FrameParams& out) {
    const float alpha = externalSim ? 1.0f : accumulator / sim::kFixedDt;
    const float time = static_cast<float>(world.time) + accumulator;

    out.time = time;
    out.shipCount = 0;

    HudState state;
    float playerDist = 0.0f;
    for (int i = 0; i < world.count; i++) {
        const sim::Vehicle& a = previous.vehicles[i];
        const sim::Vehicle& b = world.vehicles[i];
        float dist = mixf(a.dist, b.dist, alpha) + smoothDist[i];
        float lateral = mixf(a.lateral, b.lateral, alpha) + smoothLateral[i];
        float roll = mixf(a.roll, b.roll, alpha);
        // Touché par un missile : le vaisseau part en vrille.
        if (b.stunTime > 0.0f) roll += (b.stunTime / sim::kStunDuration) * 12.566f;
        float bob = std::sin(time * 3.1f + static_cast<float>(i) * 1.7f) * 0.05f;
        if (i == localIndex) playerDist = dist;

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
        ship.variant = b.ship;
        ship.trim = glm::vec4(kSlotColors[i].trim, 6.0f);
        ship.engine = glm::vec4(kSlotColors[i].engine, b.boosting ? 16.0f : 9.0f);
        // La bulle s'éteint en douceur sur sa dernière demi-seconde.
        ship.shield = std::min(1.0f, b.shieldTime * 2.0f);

        if (i != localIndex) {
            NameTag& tag = state.tags[state.tagCount++];
            tag.worldPos = pos + f.up * 3.4f;
            tag.name = config.racers[i].name;
            tag.rank = sim::rankOf(world, i);
            tag.color = glm::vec4(glm::mix(kSlotColors[i].engine, glm::vec3(1.0f), 0.25f), 1.0f);
        }
    }

    out.missileCount = 0;
    for (int i = 0; i < sim::kMaxMissiles; i++) {
        const sim::Missile& m = world.missiles[i];
        if (!m.active) continue;
        // Interpolé avec le pas précédent seulement si c'était déjà ce missile.
        const sim::Missile& before = previous.missiles[i];
        bool same = before.active && before.owner == m.owner && before.dist <= m.dist;
        float dist = same ? mixf(before.dist, m.dist, alpha) : m.dist;
        float lateral = same ? mixf(before.lateral, m.lateral, alpha) : m.lateral;
        sim::TrackFrame f = track.at(dist);
        gfx::MissileDraw& draw = out.missiles[out.missileCount++];
        draw.model = glm::mat4(glm::vec4(f.right, 0.0f), glm::vec4(f.up, 0.0f),
                               glm::vec4(-f.tangent, 0.0f),
                               glm::vec4(f.pos + f.right * lateral + f.up * 0.9f, 1.0f));
        draw.planePoint = f.pos;
        draw.planeNormal = f.up;
    }

    const sim::Vehicle& player = world.vehicles[localIndex];
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

    out.hud = nullptr;
    if (!showHud) return;

    state.energy = player.energy;
    state.position = sim::rankOf(world, localIndex);
    state.racers = world.count;
    state.lap = sim::lapOf(world, track, player);
    state.laps = world.laps;
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
    state.statsLine = statsLine;
    state.canFire = player.energy >= sim::kMissileCost && player.stunTime <= 0.0f;
    state.canShield = player.energy >= sim::kShieldCost && player.shieldTime <= 0.0f &&
                      player.stunTime <= 0.0f;
    state.shielded = player.shieldTime > 0.0f;
    state.gridStart = config.gridStart;

    hud.begin(fonts, screen.x, screen.y);
    buildHud(hud, state, gfx::viewProjection(out, screen.x / screen.y));
    out.hud = &hud;
}

} // namespace game

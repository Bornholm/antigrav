#include "sim/session.h"

#include <algorithm>

namespace sim {

namespace {

// Couloir préféré d'un pilote automatique, réparti sur la largeur.
float laneOf(int index, int count, float halfWidth) {
    if (count <= 1) return 0.0f;
    float t = static_cast<float>(index) / static_cast<float>(count - 1);
    return (t - 0.5f) * halfWidth * 0.75f;
}

} // namespace

void Session::start(const RaceConfig& cfg, int local) {
    config = cfg;
    localIndex = std::clamp(local, 0, std::max(0, cfg.racerCount - 1));
    track = Track::make(cfg.track);
    if (useFeatures) track.features = givenFeatures;
    config.track = std::clamp(cfg.track, 0, kSongSlots - 1);

    world = World{};
    world.count = cfg.racerCount;
    world.ownClock = cfg.ownClock;
    world.countdown = cfg.gridStart ? 3.0f : 0.0f;
    const float side = track.halfWidth * 0.42f;
    for (int i = 0; i < cfg.racerCount; i++) {
        Vehicle& v = world.vehicles[i];
        v.ship = cfg.racers[i].ship;
        // Deux par ligne, en quinconce.
        v.lateral = i % 2 == 0 ? -side : side;
        if (cfg.gridStart) {
            v.dist = -40.0f - 24.0f * static_cast<float>(i / 2) - (i % 2 ? 8.0f : 0.0f);
            v.speed = 0.0f;
        } else {
            v.dist = 150.0f + 26.0f * static_cast<float>(cfg.racerCount - 1 - i);
            v.speed = cruiseSpeed(v, track);
        }
    }
    previous = world;
    for (int i = 0; i < kMaxVehicles; i++) {
        remoteInputs[i] = Input{};
        lastInputs[i] = Input{};
    }
}

void Session::gatherInputs(const Input& local, Input* out) const {
    for (int i = 0; i < world.count; i++) {
        const RacerSetup& racer = config.racers[i];
        bool isLocal = i == localIndex;
        // Après l'arrivée, le pilote automatique reprend la main.
        bool ai = racer.profile != kHuman || (isLocal && autopilot) || world.vehicles[i].finished;
        if (ai) {
            int profile = racer.profile != kHuman ? racer.profile : 2;
            out[i] = sim::autopilot(world, i, track, aiProfile(profile),
                                    laneOf(i, world.count, track.halfWidth));
        } else {
            out[i] = isLocal ? local : remoteInputs[i];
        }
    }
}

void Session::tick(const Input& local) {
    previous = world;
    gatherInputs(local, lastInputs);
    step(world, track, lastInputs);
}

} // namespace sim

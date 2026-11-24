#include "sim/world.h"

#include <algorithm>
#include <cmath>

namespace sim {

namespace {

constexpr float kAccel = 45.0f;       // m/s²
constexpr float kBoostAccel = 110.0f;
constexpr float kDecel = 30.0f;
constexpr float kSteerSpeed = 27.0f;  // vitesse latérale à fond de braquage, m/s
constexpr float kSteerResponse = 9.0f;
constexpr float kDrift = 0.040f;      // part de l'accélération centrifuge subie
constexpr float kBoostDrain = 1.0f / 5.0f;   // énergie par seconde
constexpr float kEnergyRegen = 1.0f / 24.0f;

void stepVehicle(Vehicle& v, const Input& in, const Track& track, float dt) {
    TrackFrame f = track.at(v.dist);
    float steer = std::clamp(in.steer, -1.0f, 1.0f);

    v.boosting = in.boost && v.energy > 0.0f;
    float target = v.boosting ? kBoostSpeed * (v.topSpeed / kCruiseSpeed) : v.topSpeed;
    float delta = target - v.speed;
    v.speed += std::clamp(delta, -kDecel * dt, (v.boosting ? kBoostAccel : kAccel) * dt);

    v.energy += (v.boosting ? -kBoostDrain : kEnergyRegen) * dt;

    // Braquage, plus la dérive vers l'extérieur du virage.
    v.lateralVel += (steer * kSteerSpeed - v.lateralVel) * std::min(1.0f, kSteerResponse * dt);
    float drift = -f.curvature * v.speed * v.speed * kDrift;
    v.lateral += (v.lateralVel + drift) * dt;

    // Murets : rebond mou, perte de vitesse et d'énergie.
    float limit = track.halfWidth - kShipHalfWidth;
    v.scraping = false;
    if (std::abs(v.lateral) > limit) {
        float side = v.lateral > 0.0f ? 1.0f : -1.0f;
        v.lateral = side * limit;
        if (v.lateralVel * side > 0.0f) v.lateralVel *= -0.35f;
        v.speed -= v.speed * 0.55f * dt;
        v.energy -= 0.12f * dt;
        v.scraping = true;
    }
    v.energy = std::clamp(v.energy, 0.0f, 1.0f);

    // À l'intérieur d'un virage, la même vitesse couvre plus de piste.
    float pathScale = 1.0f / std::max(0.5f, 1.0f - f.curvature * v.lateral);
    v.dist += v.speed * pathScale * dt;

    float rollTarget = -steer * 0.38f - f.curvature * v.speed * 0.25f;
    v.roll += (rollTarget - v.roll) * std::min(1.0f, 7.0f * dt);
}

} // namespace

void step(World& world, const Track& track, const Input* inputs) {
    for (int i = 0; i < world.count; i++) stepVehicle(world.vehicles[i], inputs[i], track, kFixedDt);

    // Contacts entre vaisseaux : ils s'écartent, sans blocage dur.
    for (int i = 0; i < world.count; i++) {
        for (int j = i + 1; j < world.count; j++) {
            Vehicle& a = world.vehicles[i];
            Vehicle& b = world.vehicles[j];
            float along = b.dist - a.dist;
            float across = b.lateral - a.lateral;
            const float lengthLimit = 8.5f, widthLimit = kShipHalfWidth * 2.0f;
            if (std::abs(along) > lengthLimit || std::abs(across) > widthLimit) continue;
            float overlap = widthLimit - std::abs(across);
            float push = (across >= 0.0f ? 1.0f : -1.0f) * overlap * 0.5f;
            a.lateral -= push;
            b.lateral += push;
            float exchange = (a.lateralVel - b.lateralVel) * 0.5f;
            a.lateralVel -= exchange;
            b.lateralVel += exchange;
            // Celui de derrière cède un peu de vitesse.
            (along > 0.0f ? a : b).speed *= 0.992f;
        }
    }
    world.time += kFixedDt;
}

Input autopilot(const Vehicle& v, const Track& track, float laneOffset) {
    // Regarde devant pour prendre le virage à temps.
    TrackFrame here = track.at(v.dist);
    TrackFrame ahead = track.at(v.dist + v.speed * 0.6f);
    float limit = track.halfWidth - kShipHalfWidth - 1.5f;
    float line = std::clamp(ahead.curvature * 4200.0f + laneOffset, -limit, limit);

    Input in;
    float drift = -here.curvature * v.speed * v.speed * kDrift;
    float wanted = (line - v.lateral) * 1.6f - drift;
    in.steer = std::clamp(wanted / kSteerSpeed, -1.0f, 1.0f);

    bool straight = std::abs(here.curvature) < 0.0006f && std::abs(ahead.curvature) < 0.0008f;
    in.boost = straight && (v.boosting ? v.energy > 0.25f : v.energy > 0.85f);
    return in;
}

int rankOf(const World& world, int index) {
    int rank = 1;
    for (int i = 0; i < world.count; i++)
        if (world.vehicles[i].dist > world.vehicles[index].dist) rank++;
    return rank;
}

} // namespace sim

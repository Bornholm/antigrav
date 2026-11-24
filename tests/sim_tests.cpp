// Tests de la simulation. Aucun cadre de test : un échec affiche la ligne et
// fait sortir le programme avec un code non nul.
#include "sim/track.h"
#include "sim/world.h"

#include <cmath>
#include <cstdio>

namespace {

int failures = 0;

#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::printf("ECHEC %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
            failures++;                                                         \
        }                                                                       \
    } while (0)

void testTrackGeometry() {
    sim::Track track = sim::Track::makeDemo();
    CHECK(track.length > 9000.0f && track.length < 13000.0f);
    CHECK(!track.frames.empty());

    const int count = static_cast<int>(track.frames.size());
    for (int i = 0; i < count; i++) {
        const sim::TrackFrame& f = track.frames[i];
        // Repère orthonormé en chaque point.
        CHECK(std::abs(glm::length(f.tangent) - 1.0f) < 1e-3f);
        CHECK(std::abs(glm::length(f.up) - 1.0f) < 1e-3f);
        CHECK(std::abs(glm::dot(f.tangent, f.up)) < 1e-3f);
        CHECK(std::abs(glm::dot(f.right, f.up)) < 1e-3f);
        // Pas constant, bouclage compris.
        const sim::TrackFrame& next = track.frames[(i + 1) % count];
        float gap = glm::length(next.pos - f.pos);
        CHECK(std::abs(gap - track.step) < track.step * 0.05f);
        // Aucun virage plus serré que 150 m de rayon.
        CHECK(std::abs(f.curvature) < 1.0f / 150.0f);
    }
}

void testTrackWrap() {
    sim::Track track = sim::Track::makeDemo();
    sim::TrackFrame a = track.at(123.0f);
    sim::TrackFrame b = track.at(123.0f + track.length);
    sim::TrackFrame c = track.at(123.0f - 2.0f * track.length);
    CHECK(glm::length(a.pos - b.pos) < 0.05f);
    CHECK(glm::length(a.pos - c.pos) < 0.05f);
    // Continuité au passage de la ligne.
    sim::TrackFrame before = track.at(track.length - 0.01f);
    sim::TrackFrame after = track.at(0.01f);
    CHECK(glm::length(before.pos - after.pos) < 0.1f);
}

sim::World makeGrid() {
    sim::World world;
    world.count = 4;
    for (int i = 0; i < world.count; i++) {
        world.vehicles[i].dist = 150.0f + 20.0f * static_cast<float>(i);
        world.vehicles[i].lateral = -9.0f + 6.0f * static_cast<float>(i);
        world.vehicles[i].speed = sim::kCruiseSpeed;
    }
    return world;
}

void run(sim::World& world, const sim::Track& track, int steps) {
    const float lanes[4] = {-5.0f, 0.0f, 5.0f, 2.0f};
    for (int s = 0; s < steps; s++) {
        sim::Input inputs[sim::kMaxVehicles];
        for (int i = 0; i < world.count; i++)
            inputs[i] = sim::autopilot(world.vehicles[i], track, lanes[i]);
        sim::step(world, track, inputs);
    }
}

void testDeterminism() {
    sim::Track track = sim::Track::makeDemo();
    sim::World a = makeGrid();
    sim::World b = makeGrid();
    run(a, track, 3600);
    run(b, track, 3600);
    // Mêmes entrées, même état au bit près sur une même machine. Comparaison
    // champ par champ : les octets de remplissage ne sont pas définis.
    CHECK(a.time == b.time);
    for (int i = 0; i < a.count; i++) {
        const sim::Vehicle& va = a.vehicles[i];
        const sim::Vehicle& vb = b.vehicles[i];
        CHECK(va.dist == vb.dist && va.lateral == vb.lateral && va.speed == vb.speed);
        CHECK(va.lateralVel == vb.lateralVel && va.energy == vb.energy && va.roll == vb.roll);
        CHECK(va.boosting == vb.boosting && va.scraping == vb.scraping);
    }
}

void testAutopilotLaps() {
    sim::Track track = sim::Track::makeDemo();
    sim::World world = makeGrid();
    const float limit = track.halfWidth - sim::kShipHalfWidth;
    int scrapeSteps = 0;
    const float lanes[4] = {-5.0f, 0.0f, 5.0f, 2.0f};
    const int steps = 60 * 90; // 90 secondes
    for (int s = 0; s < steps; s++) {
        sim::Input inputs[sim::kMaxVehicles];
        for (int i = 0; i < world.count; i++)
            inputs[i] = sim::autopilot(world.vehicles[i], track, lanes[i]);
        sim::step(world, track, inputs);
        for (int i = 0; i < world.count; i++) {
            const sim::Vehicle& v = world.vehicles[i];
            CHECK(std::abs(v.lateral) <= limit + 1e-3f);
            CHECK(v.energy >= 0.0f && v.energy <= 1.0f);
            CHECK(std::isfinite(v.dist) && std::isfinite(v.speed));
            if (v.scraping) scrapeSteps++;
        }
    }
    for (int i = 0; i < world.count; i++) {
        // Deux tours et demi au moins, donc le pilote tient la piste.
        CHECK(world.vehicles[i].dist > 2.5f * track.length);
        CHECK(world.vehicles[i].speed > 250.0f);
    }
    // Le pilote automatique ne doit presque jamais toucher les murets.
    CHECK(scrapeSteps < steps * world.count / 50);
}

void testWallAndBoost() {
    sim::Track track = sim::Track::makeDemo();
    sim::World world;
    world.count = 1;
    world.vehicles[0].dist = 200.0f;
    world.vehicles[0].speed = sim::kCruiseSpeed;

    // Braquage à fond : le vaisseau frotte le muret et ralentit.
    sim::Input hardRight;
    hardRight.steer = 1.0f;
    for (int s = 0; s < 120; s++) sim::step(world, track, &hardRight);
    CHECK(world.vehicles[0].scraping);
    CHECK(world.vehicles[0].speed < sim::kCruiseSpeed);

    // Le boost accélère, vide la jauge, puis s'arrête de lui-même.
    world.vehicles[0] = sim::Vehicle{};
    world.vehicles[0].dist = 200.0f;
    world.vehicles[0].speed = sim::kCruiseSpeed;
    sim::Input boost;
    boost.boost = true;
    for (int s = 0; s < 60; s++) sim::step(world, track, &boost);
    CHECK(world.vehicles[0].speed > sim::kCruiseSpeed + 20.0f);
    CHECK(world.vehicles[0].energy < 0.9f);
    for (int s = 0; s < 60 * 8; s++) sim::step(world, track, &boost);
    CHECK(world.vehicles[0].energy < 0.05f);
}

void testRanking() {
    sim::World world;
    world.count = 3;
    world.vehicles[0].dist = 100.0f;
    world.vehicles[1].dist = 300.0f;
    world.vehicles[2].dist = 200.0f;
    CHECK(sim::rankOf(world, 0) == 3);
    CHECK(sim::rankOf(world, 1) == 1);
    CHECK(sim::rankOf(world, 2) == 2);
}

} // namespace

int main() {
    testTrackGeometry();
    testTrackWrap();
    testDeterminism();
    testAutopilotLaps();
    testWallAndBoost();
    testRanking();
    if (failures == 0) std::printf("simulation: tous les tests passent\n");
    return failures == 0 ? 0 : 1;
}

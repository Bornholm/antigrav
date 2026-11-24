// Tests de la simulation. Aucun cadre de test : un échec affiche la ligne et
// fait sortir le programme avec un code non nul.
#include "game/synth.h"
#include "sim/track.h"
#include "sim/world.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

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

void testCountdownAndFinish() {
    sim::Track track = sim::Track::makeDemo();
    sim::World world = makeGrid();
    world.laps = 1;
    world.countdown = 3.0f;
    float startDist = world.vehicles[0].dist;

    // Personne ne bouge pendant le compte à rebours.
    run(world, track, 170);
    CHECK(world.vehicles[0].dist == startDist);
    CHECK(world.raceTime == 0.0f);

    run(world, track, 60 * 45);
    for (int i = 0; i < world.count; i++) {
        const sim::Vehicle& v = world.vehicles[i];
        CHECK(v.finished);
        // Un tour à la vitesse de croisière prend une trentaine de secondes.
        CHECK(v.finishTime > 20.0f && v.finishTime < 45.0f);
        CHECK(sim::lapOf(world, track, v) == 1);
    }
    // Le classement final suit les temps d'arrivée.
    for (int i = 0; i < world.count; i++)
        for (int j = 0; j < world.count; j++)
            if (world.vehicles[i].finishTime < world.vehicles[j].finishTime)
                CHECK(sim::rankOf(world, i) < sim::rankOf(world, j));
}

void testSynth() {
    game::Synth synth;
    std::vector<float> buffer(48000 * 2);

    // À l'arrêt, le moteur tourne au ralenti : faible mais pas muet.
    synth.setEngine(0.0f, false, false);
    synth.render(buffer.data(), 48000);
    float idlePeak = 0.0f;
    for (float s : buffer) idlePeak = std::max(idlePeak, std::abs(s));
    CHECK(idlePeak > 0.005f && idlePeak < 0.5f);

    // À pleine vitesse avec boost, frottement et bip : plus fort, jamais saturé.
    synth.setEngine(400.0f, true, true);
    synth.beep(1);
    for (int pass = 0; pass < 3; pass++) synth.render(buffer.data(), 48000);
    float peak = 0.0f;
    double energy = 0.0;
    for (float s : buffer) {
        CHECK(std::isfinite(s));
        peak = std::max(peak, std::abs(s));
        energy += static_cast<double>(s) * s;
    }
    CHECK(peak > idlePeak);
    CHECK(peak <= 1.0f);
    CHECK(energy / buffer.size() > 1e-4);
}

void testMissileAndShield() {
    sim::Track track = sim::Track::makeDemo();
    auto setup = [&](bool shielded) {
        sim::World world;
        world.count = 2;
        for (int i = 0; i < 2; i++) {
            world.vehicles[i].dist = 300.0f + 80.0f * static_cast<float>(i);
            world.vehicles[i].speed = sim::kCruiseSpeed;
        }
        sim::Input inputs[2];
        inputs[0].fire = true;
        inputs[1].shield = shielded;
        float minSpeed = sim::kCruiseSpeed;
        bool stunned = false;
        for (int s = 0; s < 120; s++) {
            sim::step(world, track, inputs);
            minSpeed = std::min(minSpeed, world.vehicles[1].speed);
            stunned = stunned || world.vehicles[1].stunTime > 0.0f;
        }
        // Un seul missile part, même en gardant le bouton enfoncé.
        CHECK(world.vehicles[0].energy > 1.0f - sim::kMissileCost - 0.01f);
        return std::pair<bool, float>(stunned, minSpeed);
    };

    auto hit = setup(false);
    CHECK(hit.first);
    CHECK(hit.second < sim::kCruiseSpeed * 0.75f);

    auto blocked = setup(true);
    CHECK(!blocked.first);
    CHECK(blocked.second > sim::kCruiseSpeed * 0.95f);

    // Sans énergie, ni missile ni bouclier.
    sim::World world;
    world.count = 1;
    world.vehicles[0].speed = sim::kCruiseSpeed;
    world.vehicles[0].energy = 0.05f;
    sim::Input in;
    in.fire = true;
    in.shield = true;
    sim::step(world, track, &in);
    CHECK(world.vehicles[0].shieldTime == 0.0f);
    for (const sim::Missile& m : world.missiles) CHECK(!m.active);
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
    testCountdownAndFinish();
    testSynth();
    testMissileAndShield();
    if (failures == 0) std::printf("simulation: tous les tests passent\n");
    return failures == 0 ? 0 : 1;
}

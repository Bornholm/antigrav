// Tests de la simulation. Aucun cadre de test : un échec affiche la ligne et
// fait sortir le programme avec un code non nul.
#include "game/synth.h"
#include "sim/track.h"
#include "sim/generate.h"
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

// Piste à vitesse de synchronisation constante, pour les tests de physique
// qui raisonnent sur la vitesse de croisière de référence.
sim::Track flatTrack() {
    sim::Track track = sim::Track::make(0);
    for (sim::TrackFrame& f : track.frames) f.pace = sim::kCruiseSpeed * sim::kSyncEdge;
    return track;
}

void testTrackGeometry() {
    for (int id = 0; id < sim::kTrackCount; id++) {
        sim::Track track = sim::Track::make(id);
        // 100 secondes de morceau entre 970 et 1 400 km/h.
        CHECK(track.length > 26000.0f && track.length < 40000.0f);
        CHECK(track.halfWidth == sim::trackInfo(id).halfWidth);
        CHECK(std::abs(track.duration - 100.0f) < 0.01f);

        const int count = static_cast<int>(track.frames.size());
        for (int i = 0; i < count; i++) {
            const sim::TrackFrame& f = track.frames[i];
            // Repère orthonormé en chaque point.
            CHECK(std::abs(glm::length(f.tangent) - 1.0f) < 1e-3f);
            CHECK(std::abs(glm::length(f.up) - 1.0f) < 1e-3f);
            CHECK(std::abs(glm::dot(f.tangent, f.up)) < 1e-3f);
            CHECK(std::abs(glm::dot(f.right, f.up)) < 1e-3f);
            if (i + 1 < count) {
                // Pas constant, et le temps du morceau avance avec la piste.
                const sim::TrackFrame& next = track.frames[i + 1];
                float gap = glm::length(next.pos - f.pos);
                CHECK(std::abs(gap - track.step) < track.step * 0.05f);
                CHECK(next.time > f.time);
            }
            CHECK(f.pace >= 260.0f && f.pace <= 395.0f);
            // Aucun virage plus serré que 120 m de rayon, aucune pente raide.
            CHECK(std::abs(f.curvature) < 1.0f / 120.0f);
            CHECK(std::abs(f.tangent.y) < 0.18f);
        }

        // Deux portions de piste ne se touchent jamais.
        const int stride = 5;
        const int apart = static_cast<int>(200.0f / track.step);
        for (int i = 0; i < count; i += stride) {
            for (int j = i + apart; j < count; j += stride) {
                glm::vec3 d = track.frames[j].pos - track.frames[i].pos;
                float flat = std::sqrt(d.x * d.x + d.z * d.z);
                CHECK(flat > 2.0f * track.halfWidth + 6.0f || std::abs(d.y) > 25.0f);
            }
        }
    }
}

void testTrackEnds() {
    sim::Track track = sim::Track::make(0);
    // Le morceau commence sur la ligne de départ et finit sur l'arrivée.
    CHECK(std::abs(track.at(0.0f).time) < 0.02f);
    CHECK(std::abs(track.at(track.length).time - track.duration) < 0.05f);
    CHECK(std::abs(track.distAt(0.0f)) < 4.0f);
    CHECK(std::abs(track.distAt(track.duration) - track.length) < 12.0f);
    for (float t = 1.0f; t < track.duration; t += 7.3f)
        CHECK(std::abs(track.at(track.distAt(t)).time - t) < 0.02f);
    // Au-delà des extrémités, la piste continue tout droit.
    sim::TrackFrame far = track.at(track.length + 5000.0f);
    sim::TrackFrame farther = track.at(track.length + 5100.0f);
    CHECK(std::abs(glm::length(farther.pos - far.pos) - 100.0f) < 0.5f);
    sim::TrackFrame behind = track.at(-2000.0f);
    CHECK(std::isfinite(behind.pos.x) && behind.time < 0.0f);
    // On ne repasse jamais par un point déjà franchi.
    CHECK(track.ahead(100.0f, 250.0f) == 150.0f);
    CHECK(track.ahead(250.0f, 100.0f) > 1e6f);

    // La piste est plus haute dans les passages intenses que dans les calmes.
    {
        const music::Partition& song = sim::song(0);
        double high = 0.0, low = 0.0;
        int highCount = 0, lowCount = 0;
        for (float t = 8.0f; t < track.duration - 4.0f; t += 0.5f) {
            const float y = track.at(track.distAt(t)).pos.y;
            // L'altitude suit l'énergie avec quelques secondes de retard.
            const float e = song.energyAt(t - 3.0f);
            if (e > 0.7f) high += y, highCount++;
            if (e < 0.35f) low += y, lowCount++;
        }
        CHECK(highCount > 10 && lowCount > 10);
        CHECK(high / highCount > low / lowCount + 80.0);
    }

    // Le même morceau donne toujours la même piste.
    sim::Track again = sim::Track::make(0);
    CHECK(again.features.size() == track.features.size());
    CHECK(again.frames.back().pos == track.frames.back().pos);

    // Chaque plaque de boost tombe sur un premier temps de mesure.
    const music::Partition& song = sim::song(0);
    for (const sim::Feature& f : track.features) {
        if (f.type != sim::FeatureType::BoostPad) continue;
        float time = track.at(f.dist + 0.5f * f.length).time;
        float best = 1e9f;
        for (const music::Beat& b : song.beats)
            if (b.bar == 0) best = std::min(best, std::abs(b.time - time));
        CHECK(best < 0.02f);
    }
}

sim::World makeGrid() {
    sim::World world;
    world.ownClock = true; // pas d'élastique : ces tests mesurent la physique seule
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
            inputs[i] = sim::autopilot(world, i, track, sim::aiProfile(2), lanes[i]);
        sim::step(world, track, inputs);
    }
}

void testDeterminism() {
    sim::Track track = flatTrack();
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

void testAutopilotRuns() {
    for (int id = 0; id < sim::kTrackCount; id++) {
        sim::Track track = sim::Track::make(id);
        sim::World world = makeGrid();
        for (int i = 0; i < world.count; i++) world.vehicles[i].ship = i;
        const float limit = track.halfWidth - sim::kShipHalfWidth;
        int scrapeSteps = 0;
        const float lanes[4] = {-5.0f, 0.0f, 5.0f, 2.0f};
        const int steps = 60 * 90; // 90 secondes
        for (int s = 0; s < steps; s++) {
            sim::Input inputs[sim::kMaxVehicles];
            for (int i = 0; i < world.count; i++)
                inputs[i] = sim::autopilot(world, i, track, sim::aiProfile(i), lanes[i]);
            sim::step(world, track, inputs);
            for (int i = 0; i < world.count; i++) {
                const sim::Vehicle& v = world.vehicles[i];
                CHECK(std::abs(v.lateral) <= limit + 1e-3f);
                CHECK(v.energy >= 0.0f && v.energy <= 1.0f);
                CHECK(std::isfinite(v.dist) && std::isfinite(v.speed));
                if (v.scraping) scrapeSteps++;
            }
        }
        // Chaque modèle et chaque personnalité tient le rythme du morceau.
        for (int i = 0; i < world.count; i++) CHECK(world.vehicles[i].dist > 0.8f * track.distAt(90.0f));
        // Les murets restent l'exception.
        CHECK(scrapeSteps < steps * world.count / 12);
    }
}

void testWallAndBoost() {
    sim::Track track = flatTrack();
    track.features.clear(); // piste nue : ce test ne porte pas sur les éléments
    sim::World world;
    world.ownClock = true; // pas d'élastique : ces tests mesurent la physique seule
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
    for (int s = 0; s < 90; s++) sim::step(world, track, &boost);
    CHECK(world.vehicles[0].speed > sim::kCruiseSpeed + 20.0f);
    CHECK(world.vehicles[0].energy < 0.9f);
    for (int s = 0; s < 60 * 8; s++) sim::step(world, track, &boost);
    CHECK(world.vehicles[0].energy < 0.05f);
}

void testCountdownAndFinish() {
    sim::Track track = sim::Track::make(0);
    sim::World world = makeGrid();
    world.ownClock = false; // horloge commune : l'élastique tient le peloton
    world.countdown = 3.0f;
    float startDist = world.vehicles[0].dist;

    // Personne ne bouge pendant le compte à rebours.
    run(world, track, 170);
    CHECK(world.vehicles[0].dist == startDist);
    CHECK(world.raceTime == 0.0f);

    run(world, track, 60 * 130);
    for (int i = 0; i < world.count; i++) {
        const sim::Vehicle& v = world.vehicles[i];
        CHECK(v.finished);
        // Sans piloter, on frotte les murets : l'élastique ramène quand même
        // tout le monde à quelques secondes de la dernière note.
        CHECK(v.finishTime > track.duration - 8.0f && v.finishTime < track.duration + 8.0f);
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
    sim::Track track = flatTrack();
    track.features.clear(); // piste nue : ce test ne porte pas sur les éléments
    auto setup = [&](bool shielded) {
        sim::World world;
        world.ownClock = true;
    world.ownClock = true; // pas d'élastique : ces tests mesurent la physique seule
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
    world.ownClock = true; // pas d'élastique : ces tests mesurent la physique seule
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

// Avance un vaisseau seul, en ligne droite, jusqu'à la distance donnée.
sim::Vehicle driveTo(const sim::Track& track, sim::Vehicle v, float until) {
    sim::World world;
    world.ownClock = true; // pas d'élastique : ces tests mesurent la physique seule
    world.count = 1;
    world.vehicles[0] = v;
    sim::Input none;
    while (world.vehicles[0].dist < until) sim::step(world, track, &none);
    return world.vehicles[0];
}

void testFeatures() {
    sim::Track track = flatTrack();
    sim::Vehicle start;
    start.dist = 100.0f;
    start.speed = sim::kCruiseSpeed;

    // Plaque de boost : plus vite que le boost, sans toucher à la jauge.
    track.features = {{sim::FeatureType::BoostPad, 200.0f, 36.0f, 0.0f, 3.6f}};
    sim::Vehicle v = driveTo(track, start, 240.0f);
    CHECK(v.speed > sim::kCruiseSpeed + 30.0f);
    CHECK(v.energy >= 1.0f);
    // À côté de la plaque, rien.
    sim::Vehicle aside = start;
    aside.lateral = 8.0f;
    CHECK(driveTo(track, aside, 240.0f).speed < sim::kCruiseSpeed + 1.0f);

    // Champ de freinage : ralentit et vide la jauge, sauf sous bouclier.
    track.features = {{sim::FeatureType::SlowField, 200.0f, 50.0f, 0.0f, 5.0f}};
    v = driveTo(track, start, 255.0f);
    CHECK(v.speed < sim::kCruiseSpeed * 0.85f);
    CHECK(v.energy < 1.0f);
    sim::Vehicle shielded = start;
    shielded.shieldTime = 2.0f;
    CHECK(driveTo(track, shielded, 255.0f).speed > sim::kCruiseSpeed * 0.98f);

    // Barrière : choc et perte de contrôle. Le pas de simulation est plus long
    // que la barrière, elle ne doit pas être traversée pour autant.
    track.features = {{sim::FeatureType::Barrier, 200.0f, 3.0f, 0.0f, 4.0f}};
    v = driveTo(track, start, 210.0f);
    CHECK(v.speed < sim::kCruiseSpeed * 0.6f);
    CHECK(v.stunTime > 0.0f);
    CHECK(driveTo(track, aside, 210.0f).stunTime == 0.0f);
    // Le bouclier encaisse le choc et se brise.
    v = driveTo(track, shielded, 210.0f);
    CHECK(v.stunTime == 0.0f);
    CHECK(v.shieldTime == 0.0f);
    CHECK(v.speed > sim::kCruiseSpeed * 0.95f);

    // Une barrière arrête un missile.
    {
        sim::World world;
        world.ownClock = true;
    world.ownClock = true; // pas d'élastique : ces tests mesurent la physique seule
        world.count = 2;
        world.vehicles[0].dist = 100.0f;
        world.vehicles[1].dist = 320.0f;
        world.vehicles[0].speed = world.vehicles[1].speed = sim::kCruiseSpeed;
        world.vehicles[0].lateral = world.vehicles[1].lateral = 8.0f;
        track.features = {{sim::FeatureType::Barrier, 200.0f, 3.0f, 0.0f, 4.0f}};
        // Le tireur passe à côté de la barrière, le missile vise à travers.
        world.missiles[0] = {true, 0, 150.0f, 0.0f, 500.0f, 3.0f};
        world.vehicles[1].lateral = 0.0f;
        world.vehicles[1].dist = 215.0f;
        sim::Input inputs[2];
        bool stunned = false;
        for (int s = 0; s < 30; s++) {
            sim::step(world, track, inputs);
            stunned = stunned || world.vehicles[1].stunTime > 0.0f;
        }
        CHECK(!stunned);
        CHECK(!world.missiles[0].active);
    }

    // Chaque circuit a des éléments des trois sortes, laisse la grille de
    // départ libre et garde toujours un passage entre ses barrières.
    for (int id = 0; id < sim::kTrackCount; id++) {
        sim::Track t = sim::Track::make(id);
        int counts[3] = {};
        for (const sim::Feature& f : t.features) {
            counts[static_cast<int>(f.type)]++;
            CHECK(f.dist > 250.0f && f.dist + f.length < t.length - 100.0f);
            CHECK(std::abs(f.lateral) + f.halfWidth <= t.halfWidth + 0.01f);
            if (f.type != sim::FeatureType::Barrier) continue;
            // Largeur fermée à cette distance.
            float closed = 0.0f;
            for (const sim::Feature& g : t.features)
                if (g.type == sim::FeatureType::Barrier && std::abs(g.dist - f.dist) < 20.0f)
                    closed += 2.0f * g.halfWidth;
            CHECK(2.0f * t.halfWidth - closed >= 4.0f * sim::kShipHalfWidth);
        }
        for (int c : counts) CHECK(c >= 4);
    }

    // Les pilotes automatiques esquivent : un morceau sans toucher une barrière.
    for (int id = 0; id < sim::kTrackCount; id++) {
        sim::Track t = sim::Track::make(id);
        sim::World world;
        world.ownClock = true;
    world.ownClock = true; // pas d'élastique : ces tests mesurent la physique seule
        world.count = 1;
            world.vehicles[0].speed = sim::kCruiseSpeed;
        int stuns = 0;
        world.vehicles[0].speed = sim::cruiseSpeed(world.vehicles[0], t);
        for (int s = 0; s < 60 * 140 && !world.vehicles[0].finished; s++) {
            sim::Input in = sim::autopilot(world, 0, t, sim::aiProfile(2), 0.0f);
            bool before = world.vehicles[0].stunTime > 0.0f;
            sim::step(world, t, &in);
            if (!before && world.vehicles[0].stunTime > 0.0f) stuns++;
        }
        CHECK(world.vehicles[0].finished);
        CHECK(stuns == 0);
    }
}

// Règles de la course calée sur la musique.
void testRhythm() {
    sim::Track track = flatTrack();
    track.features.clear();
    const float beat = track.beats[20], next = track.beats[21];

    // Un boost lancé sur un temps est un accord : gratuit un instant, plus
    // vif, et il rend de l'énergie. Lancé entre deux temps, c'est un boost
    // ordinaire.
    auto boostAt = [&](float time) {
        sim::World world;
        world.ownClock = true;
        world.count = 1;
        sim::Vehicle& v = world.vehicles[0];
        v.dist = track.distAt(time) - sim::kCruiseSpeed * sim::kFixedDt;
        v.speed = sim::kCruiseSpeed;
        v.energy = 0.5f;
        sim::Input boost;
        boost.boost = true;
        for (int s = 0; s < 24; s++) sim::step(world, track, &boost);
        return v;
    };
    sim::Vehicle onBeat = boostAt(beat), offBeat = boostAt(0.5f * (beat + next));
    CHECK(onBeat.accord > 0.0f && offBeat.accord == 0.0f);
    CHECK(onBeat.energy > 0.5f);
    CHECK(offBeat.energy < 0.5f);
    CHECK(onBeat.speed > offBeat.speed + 5.0f);
    CHECK(onBeat.accordCooldown > 1.0f);

    // Marteler le boost ne donne pas un accord à chaque temps.
    {
        sim::World world;
        world.ownClock = true;
        world.count = 1;
        sim::Vehicle& v = world.vehicles[0];
        v.dist = track.distAt(beat) - 400.0f;
        v.speed = sim::kCruiseSpeed;
        v.energy = 0.5f;
        int accords = 0;
        for (int s = 0; s < 60 * 6; s++) {
            sim::Input in;
            in.boost = s % 4 < 2;
            bool before = v.accord > 0.0f;
            sim::step(world, track, &in);
            if (!before && v.accord > 0.0f) accords++;
        }
        CHECK(accords >= 2 && accords <= 4);
    }

    // Horloge commune : l'élastique freine qui devance la musique et aspire
    // qui traîne. Horloge par joueur : pas d'élastique.
    auto cruiseWithGap = [&](float gap, bool own) {
        sim::World world;
        world.ownClock = own;
        world.count = 1;
        world.raceTime = 30.0f;
        sim::Vehicle& v = world.vehicles[0];
        v.dist = track.distAt(30.0f + gap);
        v.speed = sim::kCruiseSpeed;
        sim::Input none;
        for (int s = 0; s < 120; s++) sim::step(world, track, &none);
        return v.speed;
    };
    CHECK(cruiseWithGap(2.0f, false) < sim::kCruiseSpeed * 0.96f);
    CHECK(cruiseWithGap(-2.0f, false) > sim::kCruiseSpeed * 1.06f);
    const float level = cruiseWithGap(0.0f, false);
    CHECK(level > cruiseWithGap(2.0f, false) + 5.0f && level < cruiseWithGap(-2.0f, false) - 5.0f);
    CHECK(std::abs(cruiseWithGap(2.0f, true) - cruiseWithGap(-2.0f, true)) < 6.0f);

    // Une plaque rend de l'énergie, une fois.
    track.features = {{sim::FeatureType::BoostPad, 1000.0f, 36.0f, 0.0f, 3.6f}};
    sim::World world;
    world.ownClock = true;
    world.count = 1;
    world.vehicles[0].dist = 900.0f;
    world.vehicles[0].speed = sim::kCruiseSpeed;
    world.vehicles[0].energy = 0.5f;
    sim::Input none;
    for (int s = 0; s < 40; s++) sim::step(world, track, &none);
    float gained = world.vehicles[0].energy - 0.5f;
    CHECK(gained > sim::kPadEnergy * 0.9f && gained < sim::kPadEnergy + 0.03f);
}

void testRanking() {
    sim::World world;
    world.ownClock = true; // pas d'élastique : ces tests mesurent la physique seule
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
    testTrackEnds();
    testDeterminism();
    testAutopilotRuns();
    testWallAndBoost();
    testFeatures();
    testRhythm();
    testRanking();
    testCountdownAndFinish();
    testSynth();
    testMissileAndShield();
    if (failures == 0) std::printf("simulation: tous les tests passent\n");
    return failures == 0 ? 0 : 1;
}

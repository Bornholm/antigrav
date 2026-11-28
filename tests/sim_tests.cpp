// Tests de la simulation. Aucun cadre de test : un échec affiche la ligne et
// fait sortir le programme avec un code non nul.
#include "core/update.h"
#include "game/ghost.h"
#include "game/score.h"
#include "game/synth.h"
#include "gfx/adaptive.h"
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
            // Les descentes sont plus raides que les montées.
            CHECK(f.tangent.y > -0.34f && f.tangent.y < 0.16f);
            CHECK(f.heat >= 0.0f && f.heat <= 1.0f);
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

    // La piste est plus basse dans les passages intenses que dans les calmes.
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
        CHECK(high / highCount < low / lowCount - 150.0);
    }

    // Grands moments : la piste grimpe avant chacun, bascule dessus et plonge.
    CHECK(track.drops.size() >= 2);
    for (float drop : track.drops) {
        const float before = track.at(track.distAt(drop - 6.0f)).pos.y;
        const float top = track.at(track.distAt(drop - 0.3f)).pos.y;
        const float after = track.at(track.distAt(drop + 5.0f)).pos.y;
        CHECK(top > before + 40.0f);
        CHECK(after < top - 150.0f);
        // Et la piste passe du froid au chaud.
        CHECK(track.at(track.distAt(drop + 3.0f)).heat > track.at(track.distAt(drop - 4.0f)).heat + 0.3f);
    }

    // Vrilles : la piste fait un tour complet sur elle-même, puis se remet à plat.
    {
        music::Partition loud = sim::metronome(150.0f, 80.0f);
        for (float& d : loud.drive) d = 0.9f;
        loud.drive.assign(loud.energy.size(), 0.9f);
        const sim::Track twisted = sim::generateTrack(loud, 15.0f, 3u);
        float most = 0.0f;
        bool upsideDown = false;
        for (const sim::TrackFrame& f : twisted.frames) {
            most = std::max(most, std::abs(f.twist));
            upsideDown = upsideDown || f.up.y < -0.8f;
            CHECK(std::abs(glm::dot(f.right, f.up)) < 1e-3f);
        }
        CHECK(most > 6.0f);
        CHECK(upsideDown);
        CHECK(twisted.frames.back().up.y > 0.9f);
        // Aucune boîte dans une vrille ni juste après : elle paraîtrait couchée en travers.
        int boxes = 0;
        for (const sim::Feature& f : twisted.features) {
            if (f.type != sim::FeatureType::ItemBox) continue;
            boxes++;
            for (float d = f.dist - sim::kTwistClear; d <= f.dist + f.length; d += 10.0f) {
                const float w = std::abs(twisted.at(d).twist);
                CHECK(w < 0.03f || w > 6.25f);
            }
        }
        CHECK(boxes >= 8);
        // Un morceau doux n'en a pas.
        for (const sim::TrackFrame& f : track.frames) CHECK(f.twist == 0.0f);
    }

    // Le même morceau donne toujours la même piste.
    sim::Track again = sim::Track::make(0);
    CHECK(again.features.size() == track.features.size());
    CHECK(again.frames.back().pos == track.frames.back().pos);

    // Chaque plaque d'énergie tombe sur un temps du morceau.
    const music::Partition& song = sim::song(0);
    for (const sim::Feature& f : track.features) {
        if (f.type != sim::FeatureType::ItemBox) continue;
        float time = track.at(f.dist + 0.5f * f.length).time;
        float best = 1e9f;
        for (const music::Beat& b : song.beats) best = std::min(best, std::abs(b.time - time));
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
        CHECK(va.lateralVel == vb.lateralVel && va.item == vb.item && va.roll == vb.roll);
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

void testWall() {
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
}

// Départ turbo : un appui net dans la fenêtre lance le turbo au signal, un
// appui trop tôt ou répété le fait rater, un appui très en avance ne compte pas.
// Fantôme : relevé à pas réguliers, relu au même endroit, et fichier qui
// survit à l'aller-retour.
// Vrille : écart sur le côté, tirs qui passent au travers, attente entre deux.
void testRoll() {
    sim::Track track = flatTrack();
    auto setup = [](sim::World& world) {
        world = sim::World{};
        world.ownClock = true; // pas d'élastique : ce test mesure la physique seule
        world.count = 2;
        world.vehicles[0].dist = 100.0f;
        world.vehicles[1].dist = 300.0f;
        world.vehicles[0].speed = world.vehicles[1].speed = sim::kCruiseSpeed;
        world.missiles[0] = {true, sim::MissileKind::Shot, 0, 150.0f, 0.0f, sim::kCruiseSpeed + 170.0f, 4.0f, 0};
    };
    sim::World world;
    setup(world);
    sim::Input inputs[2];
    // Sans vrille, le tir touche.
    const float impact = sim::shotImpactTime(world, 1, world.missiles[0]);
    CHECK(impact > 0.8f && impact < 1.0f);
    CHECK(sim::shotImpactTime(world, 0, world.missiles[0]) < 0.0f); // le tireur ne craint rien
    bool stunned = false;
    for (int s = 0; s < 90; s++) {
        sim::step(world, track, inputs);
        stunned = stunned || world.vehicles[1].stunTime > 0.0f;
    }
    CHECK(stunned);

    // Vrille juste avant l'impact : le tir passe au travers et file plus loin.
    setup(world);
    stunned = false;
    float before = 0.0f;
    for (int s = 0; s < 90; s++) {
        const float left = sim::shotImpactTime(world, 1, world.missiles[0]);
        inputs[1].roll = left >= 0.0f && left < 0.25f ? 1 : 0;
        if (inputs[1].roll && before == 0.0f) before = world.vehicles[1].lateral;
        sim::step(world, track, inputs);
        stunned = stunned || world.vehicles[1].stunTime > 0.0f;
    }
    CHECK(!stunned);
    CHECK(world.vehicles[1].lateral - before > 5.0f); // l'écart sur le côté
    CHECK(world.vehicles[1].rollTime == 0.0f && world.vehicles[1].rollDir == 0);

    // Un appui tenu ne relance pas la vrille, et il faut attendre entre deux.
    sim::Vehicle v;
    v.speed = sim::kCruiseSpeed;
    sim::Input hold;
    hold.roll = -1;
    sim::World empty;
    empty.ownClock = true;
    sim::stepVehicle(v, hold, track, empty);
    CHECK(v.rollDir == -1 && v.rollTime > 0.0f);
    for (int s = 0; s < 60; s++) sim::stepVehicle(v, hold, track, empty);
    CHECK(v.rollTime == 0.0f);
    CHECK(!sim::startRoll(v, 1)); // encore en attente
    for (int s = 0; s < 120; s++) sim::stepVehicle(v, {}, track, empty);
    CHECK(sim::startRoll(v, 1));
    // Sonné, pas de vrille.
    sim::Vehicle dazed;
    dazed.stunTime = 0.5f;
    CHECK(!sim::startRoll(dazed, 1));
}

void testGhost() {
    sim::Track track = sim::Track::make(0);
    sim::World world = makeGrid();
    world.ownClock = false;
    game::Ghost ghost;
    ghost.record(world.vehicles[0], world.raceTime);
    for (int s = 0; s < 60 * 20; s++) {
        run(world, track, 1);
        ghost.record(world.vehicles[0], world.raceTime);
    }
    // Vingt secondes à vingt relevés par seconde, plus celui du départ, à
    // l'arrondi près du temps de course cumulé.
    CHECK(ghost.samples.size() == 400 || ghost.samples.size() == 401);
    ghost.finishTime = world.raceTime;
    ghost.trackLength = track.length;
    CHECK(ghost.valid());
    // Relevé exact sur un pas, interpolé entre deux.
    CHECK(ghost.at(0.0f).dist == ghost.samples[0].dist);
    CHECK(std::abs(ghost.at(2.0f).dist - ghost.samples[40].dist) < 0.05f);
    const float mid = ghost.at(2.025f).dist;
    CHECK(mid > ghost.samples[40].dist && mid < ghost.samples[41].dist);
    // Au-delà du dernier relevé, il reste sur le dernier.
    CHECK(ghost.at(999.0f).dist == ghost.samples.back().dist);
    // L'instant retrouvé par la distance est celui du relevé.
    for (float t : {0.5f, 3.33f, 12.0f, 19.9f}) CHECK(std::abs(ghost.timeAt(ghost.at(t).dist) - t) < 0.01f);
    CHECK(ghost.timeAt(ghost.samples.back().dist + 10.0f) < 0.0f);
    // Aller-retour du fichier, puis fichiers abîmés.
    const std::vector<uint8_t> bytes = ghost.encode();
    game::Ghost back;
    CHECK(back.decode(bytes.data(), bytes.size()));
    CHECK(back.samples.size() == ghost.samples.size() && back.finishTime == ghost.finishTime);
    CHECK(back.samples[200].lateral == ghost.samples[200].lateral);
    CHECK(!back.decode(bytes.data(), bytes.size() - 1));
    CHECK(!back.valid());
    std::vector<uint8_t> bad = bytes;
    bad[0] ^= 1;
    CHECK(!back.decode(bad.data(), bad.size()));
}

void testStartBoost() {
    sim::Track track = sim::Track::make(0);
    // Appuis du vaisseau 0 aux instants donnés du compte à rebours, en secondes
    // avant le signal. Rend son état au premier pas de course.
    auto start = [&](std::initializer_list<float> presses) {
        sim::World world = makeGrid();
        world.countdown = 3.0f;
        while (world.countdown > 0.0f) {
            sim::Input inputs[sim::kMaxVehicles] = {};
            for (float at : presses)
                if (std::abs(world.countdown - at) < sim::kFixedDt * 0.5f) inputs[0].use = true;
            sim::step(world, track, inputs);
        }
        return world.vehicles[0];
    };
    const sim::Vehicle clean = start({0.15f});
    CHECK(clean.startBoost == 1 && clean.turboTime == sim::kStartTurbo);
    const sim::Vehicle none = start({});
    CHECK(none.startBoost == 0 && none.turboTime == 0.0f);
    const sim::Vehicle early = start({0.7f});
    CHECK(early.startBoost == 2 && early.turboTime == 0.0f);
    // Trop tôt puis au bon moment : le départ reste raté.
    const sim::Vehicle retry = start({0.7f, 0.15f});
    CHECK(retry.startBoost == 2 && retry.turboTime == 0.0f);
    // Marteler le bouton dans la fenêtre ne marche pas non plus.
    const sim::Vehicle mash = start({0.25f, 0.15f});
    CHECK(mash.startBoost == 2 && mash.turboTime == 0.0f);
    // Un appui en tout début de compte à rebours est oublié.
    const sim::Vehicle forgiven = start({2.5f, 0.15f});
    CHECK(forgiven.startBoost == 1 && forgiven.turboTime == sim::kStartTurbo);

    // Le turbo de départ donne une avance, modeste, au bout de trois secondes.
    auto distanceAfter = [&](bool boost) {
        sim::World world = makeGrid();
        world.countdown = 3.0f;
        for (int i = 0; i < 60 * 6; i++) {
            sim::Input inputs[sim::kMaxVehicles] = {};
            inputs[0].use = boost && world.countdown > 0.0f && world.countdown <= 0.15f;
            sim::step(world, track, inputs);
        }
        return world.vehicles[0].dist;
    };
    const float gain = distanceAfter(true) - distanceAfter(false);
    std::printf("départ turbo : %.1f m d'avance après 3 s de course\n", gain);
    CHECK(gain > 5.0f && gain < 200.0f);
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

// Bonus : chacun des cinq fait ce qu'il doit, et un bouclier encaisse un choc.
void testItems() {
    sim::Track track = flatTrack();
    track.features.clear(); // piste nue : ce test ne porte pas sur les éléments

    // Deux vaisseaux, le second `gap` mètres devant. Le premier déclenche son
    // bonus au premier pas, puis la course tourne `steps` pas.
    struct Result {
        sim::World world;
        bool stunned = false;
        float minSpeed = 1e9f, maxSpeed = 0.0f;
    };
    auto play = [&](sim::Item item, float gap, int steps, bool targetShielded = false) {
        Result r;
        sim::World& world = r.world;
        world.ownClock = true;
        world.count = 2;
        for (int i = 0; i < 2; i++) {
            world.vehicles[i].dist = 300.0f + gap * static_cast<float>(i);
            world.vehicles[i].speed = sim::kCruiseSpeed;
        }
        world.vehicles[0].item = item;
        if (targetShielded) world.vehicles[1].shieldTime = 5.0f;
        for (int s = 0; s < steps; s++) {
            sim::Input inputs[2];
            inputs[0].use = true; // gardé enfoncé : un seul déclenchement
            sim::step(world, track, inputs);
            r.stunned = r.stunned || world.vehicles[1].stunTime > 0.0f;
            r.minSpeed = std::min(r.minSpeed, world.vehicles[1].speed);
            r.maxSpeed = std::max(r.maxSpeed, world.vehicles[0].speed);
        }
        return r;
    };
    auto activeMissiles = [](const sim::World& world) {
        int count = 0;
        for (const sim::Missile& m : world.missiles) count += m.active;
        return count;
    };

    // Tir : il rattrape le vaisseau de devant, l'étourdit et le ralentit.
    {
        Result r = play(sim::Item::Shot, 80.0f, 120);
        CHECK(r.stunned);
        CHECK(r.minSpeed < sim::kCruiseSpeed * 0.8f);
        CHECK(r.world.vehicles[0].item == sim::Item::None);
        CHECK(activeMissiles(r.world) == 0);
        // Sur un bouclier, le tir s'éteint et le bouclier avec.
        r = play(sim::Item::Shot, 80.0f, 120, true);
        CHECK(!r.stunned && r.minSpeed > sim::kCruiseSpeed * 0.95f);
        CHECK(r.world.vehicles[1].shieldTime == 0.0f);
    }
    // Mine : posée derrière soi, elle ne gêne pas celui de devant.
    {
        Result r = play(sim::Item::Mine, 80.0f, 60);
        CHECK(!r.stunned);
        CHECK(activeMissiles(r.world) == 1);
        // Mais celui qui suit roule dessus. Son poseur ne craint rien.
        sim::World world;
        world.ownClock = true;
        world.count = 2;
        world.vehicles[0].dist = 400.0f;
        world.vehicles[1].dist = 300.0f;
        world.vehicles[0].speed = world.vehicles[1].speed = sim::kCruiseSpeed;
        world.vehicles[0].item = sim::Item::Mine;
        bool follower = false, owner = false;
        for (int s = 0; s < 60; s++) {
            sim::Input inputs[2];
            inputs[0].use = s == 0;
            sim::step(world, track, inputs);
            follower = follower || world.vehicles[1].stunTime > 0.0f;
            owner = owner || world.vehicles[0].stunTime > 0.0f;
        }
        CHECK(follower && !owner);
        CHECK(activeMissiles(world) == 0);
    }
    // Onde : touche les voisins proches, pas les lointains.
    CHECK(play(sim::Item::Pulse, 40.0f, 10).stunned);
    CHECK(!play(sim::Item::Pulse, sim::kPulseRange + 60.0f, 10).stunned);
    // Turbo : une pointe de vitesse, qui retombe.
    {
        Result r = play(sim::Item::Turbo, 500.0f, 60);
        CHECK(r.maxSpeed > sim::kCruiseSpeed * 1.12f);
        CHECK(r.world.vehicles[0].boosting);
        r = play(sim::Item::Turbo, 500.0f, 60 * 6);
        CHECK(!r.world.vehicles[0].boosting);
        CHECK(std::abs(r.world.vehicles[0].speed - sim::kCruiseSpeed) < 8.0f);
    }
    // Bouclier : levé pour un temps.
    {
        Result r = play(sim::Item::Shield, 500.0f, 30);
        CHECK(r.world.vehicles[0].shieldTime > sim::kShieldTime - 1.0f);
        r = play(sim::Item::Shield, 500.0f, static_cast<int>(60 * (sim::kShieldTime + 1.0f)));
        CHECK(r.world.vehicles[0].shieldTime == 0.0f);
    }
    // Comète : elle remonte la piste et frappe chacun au passage, sans
    // s'arrêter au premier, puis s'éteint une fois le meneur dépassé.
    {
        sim::World world;
        world.ownClock = true;
        world.count = 4;
        for (int i = 0; i < 4; i++) {
            world.vehicles[i].dist = 300.0f + 150.0f * static_cast<float>(i);
            world.vehicles[i].lateral = i % 2 ? 6.0f : -6.0f;
            world.vehicles[i].speed = sim::kCruiseSpeed;
        }
        world.vehicles[0].item = sim::Item::Comet;
        world.vehicles[2].shieldTime = 5.0f;
        bool stunned[4] = {};
        for (int s = 0; s < 60 * 6; s++) {
            sim::Input inputs[4];
            inputs[0].use = s == 0;
            sim::step(world, track, inputs);
            for (int i = 0; i < 4; i++) stunned[i] = stunned[i] || world.vehicles[i].stunTime > 0.0f;
        }
        CHECK(!stunned[0]);                // son lanceur ne craint rien
        CHECK(stunned[1] && stunned[3]);   // les autres y passent tous
        CHECK(!stunned[2] && world.vehicles[2].shieldTime == 0.0f); // sauf sous bouclier
        CHECK(activeMissiles(world) == 0);
    }
    // Elle n'est donnée qu'au dernier.
    {
        int comets[3] = {};
        for (uint32_t tick = 1; tick < 4000; tick++) {
            sim::World world;
            world.ownClock = true;
            world.count = 3;
            world.tick = tick;
            for (int i = 0; i < 3; i++) {
                world.vehicles[i].dist = 300.0f + 200.0f * static_cast<float>(i);
                world.vehicles[i].speed = sim::kCruiseSpeed;
                world.vehicles[i].boxHit = true;
            }
            sim::Input inputs[3];
            sim::step(world, track, inputs);
            for (int i = 0; i < 3; i++) comets[i] += world.vehicles[i].item == sim::Item::Comet;
        }
        CHECK(comets[0] > 500 && comets[0] < 1300); // environ une boîte sur cinq
        CHECK(comets[1] == 0 && comets[2] == 0);
    }

    // Sans bonus, le bouton ne fait rien.
    CHECK(activeMissiles(play(sim::Item::None, 80.0f, 30).world) == 0);
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

    // Boîte à bonus : passer dessus les mains vides donne un bonus.
    track.features = {{sim::FeatureType::ItemBox, 200.0f, 36.0f, 0.0f, 3.6f}};
    sim::Vehicle v = driveTo(track, start, 260.0f);
    CHECK(v.item != sim::Item::None);
    CHECK(std::abs(v.speed - sim::kCruiseSpeed) < 3.0f);
    // À côté de la boîte, rien.
    sim::Vehicle aside = start;
    aside.lateral = 8.0f;
    CHECK(driveTo(track, aside, 260.0f).item == sim::Item::None);
    // On ne porte qu'un bonus : la boîte ne remplace pas celui qu'on tient.
    sim::Vehicle holding = start;
    holding.item = sim::Item::Mine;
    CHECK(driveTo(track, holding, 260.0f).item == sim::Item::Mine);
    sim::Vehicle shielded = start;
    shielded.shieldTime = 5.0f;

    // Barrière : choc et perte de contrôle. Le pas de simulation est plus long
    // que la barrière, elle ne doit pas être traversée pour autant.
    track.features = {{sim::FeatureType::Barrier, 200.0f, 3.0f, 0.0f, 4.0f}};
    v = driveTo(track, start, 210.0f);
    CHECK(v.speed < sim::kCruiseSpeed * 0.6f);
    CHECK(v.stunTime > 0.0f);
    CHECK(driveTo(track, aside, 210.0f).stunTime == 0.0f);
    // Seul le cœur du vaisseau touche : une aile peut mordre sur la barrière.
    sim::Vehicle grazing = start;
    grazing.lateral = 4.0f + sim::kHitRadius + 0.3f;
    CHECK(grazing.lateral < 4.0f + sim::kShipHalfWidth);
    CHECK(driveTo(track, grazing, 210.0f).stunTime == 0.0f);
    grazing.lateral = 4.0f + sim::kHitRadius - 0.3f;
    CHECK(driveTo(track, grazing, 210.0f).stunTime > 0.0f);
    // Tremplin : le vaisseau décolle, survole une barrière, puis retombe.
    {
        sim::Track jumpTrack = track;
        jumpTrack.features = {{sim::FeatureType::Jump, 200.0f, 12.0f, 0.0f, 15.0f},
                              {sim::FeatureType::Barrier, 320.0f, 3.0f, 0.0f, 4.0f}};
        sim::Vehicle flying = driveTo(jumpTrack, start, 260.0f);
        CHECK(flying.airTime > 0.0f && sim::airHeight(flying) > 3.0f);
        flying = driveTo(jumpTrack, start, 400.0f);
        CHECK(flying.stunTime == 0.0f); // passé au-dessus de la barrière
        flying = driveTo(jumpTrack, start, 200.0f + sim::kCruiseSpeed * (sim::kAirTime + 0.3f));
        CHECK(flying.airTime == 0.0f && sim::airHeight(flying) == 0.0f);
        // Le saut rapporte un turbo qui dure un peu après l'atterrissage.
        CHECK(flying.turboTime > 0.0f && flying.speed > sim::kCruiseSpeed * 1.05f);
        // Un tremplin sur un seul couloir : à côté, on reste au sol.
        jumpTrack.features = {{sim::FeatureType::Jump, 200.0f, 12.0f, -10.0f, 5.0f}};
        CHECK(driveTo(jumpTrack, start, 260.0f).airTime == 0.0f);
        sim::Vehicle onLane = start;
        onLane.lateral = -10.0f;
        CHECK(driveTo(jumpTrack, onLane, 260.0f).airTime > 0.0f);
    }

    // Le bouclier encaisse la barrière et s'éteint.
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
        world.missiles[0] = {true, sim::MissileKind::Shot, 0, 150.0f, 0.0f, 500.0f, 3.0f, 0};
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
        CHECK(counts[0] >= 4 && counts[1] >= 4);
        // Un tremplin par grand moment, et rien sur l'aire d'atterrissage.
        CHECK(counts[2] >= 1 && counts[2] <= static_cast<int>(t.drops.size()));
        for (const sim::Feature& jump : t.features) {
            if (jump.type != sim::FeatureType::Jump) continue;
            for (const sim::Feature& f : t.features)
                if (f.type != sim::FeatureType::Jump) CHECK(f.dist + f.length < jump.dist - 50.0f || f.dist > jump.dist + 300.0f);
        }

        // Une barrière ne suit jamais une plaque : après une plaque, son
        // couloir reste libre une seconde et demie.
        for (const sim::Feature& pad : t.features) {
            if (pad.type != sim::FeatureType::ItemBox) continue;
            for (const sim::Feature& f : t.features) {
                if (f.type != sim::FeatureType::Barrier) continue;
                const float after = f.dist - (pad.dist + pad.length);
                if (after < 0.0f || after > t.at(pad.dist).pace * sim::kClearAfterPad) continue;
                CHECK(std::abs(f.lateral - pad.lateral) >= f.halfWidth + sim::kClearHalfWidth);
            }
        }
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
void testPace() {
    sim::Track track = flatTrack();
    track.features.clear();

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

// Résolution adaptative : elle baisse sous la charge, remonte au calme, et
// espace ses essais quand le palier du dessus ne tient pas.
// Qualité automatique : elle baisse quand la résolution reste basse, monte
// quand le GPU a de la marge, et cesse de monter après une montée ratée.
void testQualityGovernor() {
    const float budget = 1000.0f / 60.0f;
    auto run = [&](gfx::QualityGovernor& g, int step, float gpuMs, float seconds) {
        int changes = 0;
        for (float t = 0.0f; t < seconds; t += budget * 0.001f)
            if (g.update(budget, budget, budget, step, gpuMs)) changes++;
        return changes;
    };
    gfx::QualityGovernor g;
    // Charge normale : rien ne bouge.
    CHECK(run(g, 1, budget * 0.7f, 60.0f) == 0 && g.level == 2);
    // Résolution tombée à 60 % : le niveau baisse, un cran toutes les 3 s.
    run(g, 3, budget, 3.5f);
    CHECK(g.level == 1);
    run(g, 4, budget, 3.5f);
    CHECK(g.level == 0);
    CHECK(run(g, 4, budget, 30.0f) == 0 && g.level == 0);
    // Sans mesure du GPU, pas de montée.
    CHECK(run(g, 0, 0.0f, 60.0f) == 0 && g.level == 0);
    // Large marge pendant 20 s : il monte d'un cran.
    run(g, 0, budget * 0.3f, 21.0f);
    CHECK(g.level == 1);
    // Cette montée ne tient pas : il redescend et ne réessaie plus.
    run(g, 3, budget, 3.5f);
    CHECK(g.level == 0 && g.raiseBlocked);
    CHECK(run(g, 0, budget * 0.3f, 120.0f) == 0 && g.level == 0);
    // Écran à 60 Hz : jamais de plafond.
    CHECK(!g.cap60);

    // Écran à 120 Hz : la qualité baisse d'abord, puis la cadence passe à 60.
    const float screen = 1000.0f / 120.0f;
    auto run120 = [&](gfx::QualityGovernor& q, int step, float gpuMs, float seconds) {
        for (float t = 0.0f; t < seconds;) {
            const float target = q.cap60 ? budget : screen;
            q.update(target, target, screen, step, gpuMs);
            t += target * 0.001f;
        }
    };
    gfx::QualityGovernor h;
    run120(h, 3, screen, 3.5f);
    run120(h, 3, screen, 3.5f);
    CHECK(h.level == 0 && !h.cap60);
    run120(h, 3, screen, 3.5f);
    CHECK(h.level == 0 && h.cap60);
    // À 60 images par seconde, de la marge sous le budget de 60 mais pas sous
    // celui de 120 : la qualité remonte, la cadence reste plafonnée.
    run120(h, 0, budget * 0.3f, 21.0f);
    CHECK(h.level == 1 && h.cap60);
    // Assez de marge pour l'écran entier : la cadence repart en premier.
    run120(h, 0, screen * 0.3f, 21.0f);
    CHECK(h.level == 1 && !h.cap60);
    // Cadence imposée par le joueur : le régulateur n'y touche pas.
    gfx::QualityGovernor m;
    m.ownsCadence = false;
    run120(m, 4, screen, 30.0f);
    CHECK(m.level == 0 && !m.cap60);
}

// Comparaison de versions pour la recherche de mise à jour.
void testVersions() {
    CHECK(core::newerVersion("v0.8.1", "v0.8.0"));
    CHECK(core::newerVersion("v0.10.0", "v0.9.9"));
    CHECK(core::newerVersion("v1.0.0", "v0.12.3"));
    CHECK(!core::newerVersion("v0.8.0", "v0.8.0"));
    CHECK(!core::newerVersion("v0.7.9", "v0.8.0"));
    // Version de développement ou texte inattendu : jamais de message.
    CHECK(!core::newerVersion("v0.9.0", "v0.8.0-3-gabc1234"));
    CHECK(!core::newerVersion("v0.9.0", "v0.8.0-dirty"));
    CHECK(!core::newerVersion("nightly", "v0.8.0"));
    CHECK(!core::newerVersion("v0.9", "v0.8.0"));
    CHECK(core::tagFromRelease("{\"url\":\"x\",\"tag_name\": \"v0.8.1\",\"name\":\"v0.8.1\"}") == "v0.8.1");
    CHECK(core::tagFromRelease("{\"message\":\"Not Found\"}").empty());
}

void testAdaptiveScale() {
    const float budget = 1000.0f / 60.0f;
    auto run = [&](gfx::AdaptiveScale& a, float frameMs, float gpuMs, float seconds) {
        int changes = 0;
        for (float t = 0.0f; t < seconds; t += frameMs * 0.001f)
            if (a.update(frameMs, budget, gpuMs)) changes++;
        return changes;
    };
    gfx::AdaptiveScale a;
    CHECK(run(a, budget, 0.0f, 30.0f) == 0 && a.step == 0);
    // Une image sur deux manquée : l'échelle descend, sans dépasser le dernier palier.
    run(a, budget * 2.0f, 0.0f, 1.0f);
    CHECK(a.step == 1);
    run(a, budget * 2.0f, 0.0f, 60.0f);
    CHECK(a.step == gfx::AdaptiveScale::kSteps - 1);
    // Un à-coup isolé ne compte pas.
    gfx::AdaptiveScale b;
    CHECK(!b.update(budget * 20.0f, budget, 0.0f) && b.step == 0);
    // Au calme, elle remonte palier par palier.
    run(a, budget, 0.0f, 60.0f);
    CHECK(a.step == 0);
    // Le GPU mesuré est déjà au bord du budget : pas de remontée.
    gfx::AdaptiveScale c;
    run(c, budget * 2.0f, 0.0f, 1.0f);
    CHECK(c.step == 1);
    CHECK(run(c, budget, budget * 0.9f, 60.0f) == 0 && c.step == 1);
    // Un palier qui ne tient jamais : les essais s'espacent au lieu de battre.
    gfx::AdaptiveScale d;
    int changes = 0;
    for (float t = 0.0f; t < 240.0f;) {
        const float ms = d.step == 0 ? budget * 2.0f : budget;
        if (d.update(ms, budget, 0.0f)) changes++;
        t += ms * 0.001f;
    }
    CHECK(changes >= 3 && changes <= 14);
}

// Score : borné, croissant avec la place, et la note S hors de portée en facile.
void testScore() {
    game::ScoreTally perfect;
    for (int i = 0; i < 120 * 60; i++) perfect.step(1.0f / 60.0f, 1.0f, false);
    perfect.pickups = 20;
    const game::Score best = game::computeScore(perfect, 1, 4, 2);
    CHECK(best.total == 10000 && best.grade == 'S');
    // La note S est réservée au niveau difficile.
    CHECK(game::computeScore(perfect, 1, 4, 0).grade == 'A');
    CHECK(game::computeScore(perfect, 1, 4, 1).grade == 'A');
    CHECK(game::computeScore(perfect, 2, 4, 2).total < best.total);
    CHECK(game::computeScore(perfect, 4, 4, 2).place == 0);
    game::ScoreTally rough = perfect;
    rough.hits = 10;
    rough.scrape = 30.0f;
    CHECK(game::computeScore(rough, 1, 4, 2).clean < best.clean);
    // En retard de trois secondes sur la musique, la part de tempo est nulle.
    game::ScoreTally late;
    for (int i = 0; i < 600; i++) late.step(1.0f / 60.0f, -3.0f, false);
    CHECK(game::computeScore(late, 4, 4, 2).tempo == 0);
    CHECK(game::gradeOf(0) == 'D' && game::gradeOf(4000) == 'C' && game::gradeOf(6000) == 'B' &&
          game::gradeOf(7500) == 'A' && game::gradeOf(9000) == 'S');
    // Une course vide ne divise pas par zéro.
    CHECK(game::computeScore(game::ScoreTally{}, 1, 1, 1).total >= 0);
}

} // namespace

int main() {
    testScore();
    testAdaptiveScale();
    testVersions();
    testQualityGovernor();
    testTrackGeometry();
    testTrackEnds();
    testDeterminism();
    testAutopilotRuns();
    testWall();
    testFeatures();
    testPace();
    testRanking();
    testCountdownAndFinish();
    testStartBoost();
    testGhost();
    testRoll();
    testSynth();
    testItems();
    if (failures == 0) std::printf("simulation: tous les tests passent\n");
    return failures == 0 ? 0 : 1;
}

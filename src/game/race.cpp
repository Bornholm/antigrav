#include "game/race.h"

#include "game/i18n.h"
#include "game/music.h"
#include "gfx/theme.h"
#include "game/songs.h"
#include "sim/generate.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <SDL3/SDL.h>

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

} // namespace

RaceConfig defaultConfig(int track, bool gridStart) {
    RaceConfig config;
    config.track = track;
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

void Race::initBlank() {
    sim::setSong(sim::kCustomTrack, sim::metronome(120.0f, 14.0f));
    RaceConfig cfg = defaultConfig(sim::kCustomTrack, false);
    cfg.racerCount = 1;
    cfg.racers[0].profile = 2;
    theme = 0;
    songSerial = -2;
    start(cfg, 0);
    tally = {};
    score = {};
    scored = false;
    bestScore = 0;
    newRecord = false;
    accumulator = 0.0f;
    camLateral = world.vehicles[localIndex].lateral;
    for (int i = 0; i < sim::kMaxVehicles; i++) smoothDist[i] = smoothLateral[i] = 0.0f;
    minimap.clear();
    ghost.clear();
    showGhost = false;
    recording.clear();
    recordGhost = false;
    autopilot = true;
    orbit = true;
    showHud = false;
}

void Race::init(const RaceConfig& cfg, int local) {
    // La piste se génère à partir de la partition du morceau.
    // Celle du morceau du joueur est déjà en place, posée par la bibliothèque.
    const bool custom = cfg.track == sim::kCustomTrack;
    if (!custom && !keepSong) sim::setSong(cfg.track, Music::partition(cfg.track));
    theme = custom ? customTheme() : cfg.track;
    songSerial = custom ? 100 + customSerial() : cfg.track;
    start(cfg, local);
    tally = {};
    score = {};
    scored = false;
    bestScore = 0;
    newRecord = false;
    hudParts = kHudAll;
    countdownY = 400.0f;
    ghost.clear();
    showGhost = false;
    recording.clear();
    recordGhost = false;
    bestTime = 0.0f;
    newBestTime = false;
    accumulator = 0.0f;
    camLateral = world.vehicles[localIndex].lateral;
    for (int i = 0; i < sim::kMaxVehicles; i++) smoothDist[i] = smoothLateral[i] = 0.0f;
    lastTap[0] = lastTap[1] = -10.0f;
    tapDown[0] = tapDown[1] = false;
    rollHold = 0.0f;
    rollWanted = 0;
    alertWait = 0.0f;

    minimap.clear();
}

void Race::spawnFx(const Fx& effect) {
    // La place libre, ou à défaut celle de l'effet le plus avancé.
    Fx* slot = &fx[0];
    for (Fx& f : fx) {
        if (f.age >= f.life) {
            slot = &f;
            break;
        }
        if (f.age / std::max(0.01f, f.life) > slot->age / std::max(0.01f, slot->life)) slot = &f;
    }
    *slot = effect;
    slot->age = 0.0f;
}

void Race::react(const sim::World& before) {
    const glm::vec3 kShot{1.0f, 0.25f, 0.75f}, kMine{1.0f, 0.22f, 0.12f}, kPulse{0.55f, 0.40f, 1.0f};
    const glm::vec3 kTurbo{1.0f, 0.75f, 0.2f}, kShield{0.2f, 0.85f, 1.0f}, kComet{1.0f, 0.5f, 0.1f};
    const sim::Vehicle& me = world.vehicles[localIndex];
    for (int i = 0; i < world.count; i++) {
        const sim::Vehicle& was = before.vehicles[i];
        const sim::Vehicle& now = world.vehicles[i];
        const bool mine = i == localIndex;
        // Les autres s'entendent de près seulement, et moins fort.
        const float near = std::abs(now.dist - me.dist);
        const float volume = mine ? 1.0f : near < 250.0f ? 0.5f * (1.0f - near / 250.0f) : 0.0f;
        auto sound = [&](Sfx sfx) {
            if (synth && volume > 0.02f) synth->play(sfx, volume);
        };
        auto burst = [&](const glm::vec3& color, float from, float to, float life, float gain) {
            Fx e;
            e.follow = i;
            e.color = color;
            // Sur son propre vaisseau la caméra est dans l'orbe : il reste
            // discret, le voile de couleur fait le retour.
            e.from = from;
            e.to = mine ? std::min(to, 8.0f) : to;
            e.life = life;
            e.gain = mine ? gain * 0.3f : gain;
            spawnFx(e);
        };
        auto screen = [&](const glm::vec3& color, float amount) {
            if (!mine) return;
            flashColor = color;
            flash = std::max(flash, amount);
        };

        // Relevé du score : chocs encaissés et bonus ramassés par le joueur.
        if (mine && !now.finished) {
            if (now.stunTime > was.stunTime + 0.05f) tally.hits++;
            if (was.item == sim::Item::None && now.item != sim::Item::None) tally.pickups++;
        }
        // Départ turbo réussi : il part au signal.
        if (before.countdown > 0.0f && world.countdown <= 0.0f && now.startBoost == 1) {
            sound(Sfx::Turbo);
            burst(kTurbo, 2.0f, 10.0f, 0.4f, 3.0f);
            screen(kTurbo, 0.22f);
        }
        // Bonus ramassé.
        if (was.item == sim::Item::None && now.item != sim::Item::None) {
            sound(Sfx::Pickup);
            burst({1.0f, 0.85f, 0.3f}, 2.0f, 9.0f, 0.35f, 2.2f);
            screen({1.0f, 0.85f, 0.3f}, 0.10f);
        }
        // Bonus déclenché.
        if (was.item != sim::Item::None && now.item == sim::Item::None) {
            switch (was.item) {
            case sim::Item::Shot:
                sound(Sfx::Shot);
                burst(kShot, 1.5f, 8.0f, 0.25f, 4.0f);
                break;
            case sim::Item::Mine:
                sound(Sfx::Mine);
                burst(kMine, 1.0f, 6.0f, 0.3f, 3.0f);
                break;
            case sim::Item::Pulse:
                sound(Sfx::Pulse);
                // Deux ondes : un front vif, puis un halo plus large.
                burst(kPulse, 4.0f, sim::kPulseRange, 0.55f, 5.0f);
                burst(kPulse, 2.0f, sim::kPulseRange * 0.6f, 0.8f, 2.0f);
                screen(kPulse, 0.30f);
                break;
            case sim::Item::Turbo:
                sound(Sfx::Turbo);
                burst(kTurbo, 3.0f, 16.0f, 0.4f, 4.0f);
                screen(kTurbo, 0.22f);
                break;
            case sim::Item::Shield:
                sound(Sfx::ShieldUp);
                burst(kShield, 9.0f, 5.0f, 0.45f, 3.0f);
                screen(kShield, 0.20f);
                break;
            case sim::Item::Comet:
                sound(Sfx::Comet);
                burst(kComet, 4.0f, 30.0f, 0.6f, 6.0f);
                screen(kComet, 0.35f);
                break;
            default: break;
            }
        }
        // Choc : explosion sur le vaisseau, et l'écran du joueur en tremble.
        if (was.stunTime <= 0.0f && now.stunTime > 0.0f) {
            sound(Sfx::Hit);
            burst({1.0f, 0.9f, 0.7f}, 2.0f, 18.0f, 0.4f, 7.0f);
            burst({1.0f, 0.35f, 0.1f}, 4.0f, 30.0f, 0.6f, 2.5f);
            screen({1.0f, 0.15f, 0.1f}, 0.32f);
            if (mine) hitShake = 1.0f;
        }
        // Bouclier qui cède sous un choc, et non parce qu'il a fait son temps.
        if (was.shieldTime > 0.2f && now.shieldTime <= 0.0f) {
            sound(Sfx::ShieldBreak);
            burst(kShield, 6.0f, 22.0f, 0.4f, 5.0f);
            screen(kShield, 0.30f);
        }
        if (was.airTime <= 0.0f && now.airTime > 0.0f) sound(Sfx::Jump);
        if (was.rollTime <= 0.0f && now.rollTime > 0.0f) sound(Sfx::Roll);
    }
}

void Race::update(float dt, const Controls& input) {
    controls = input;
    sim::Input local;
    local.steer = input.steer;
    local.use = input.pressed[static_cast<int>(Button::Use)];
    // Double appui sur une direction : vrille de ce côté. Appuyer de l'autre
    // côté entre-temps annule le premier appui, pour qu'un pouce qui passe
    // d'un bouton à l'autre ne vrille pas.
    tapClock += dt;
    for (int side = 0; side < 2; side++) {
        const bool down = input.pressed[static_cast<int>(side ? Button::Right : Button::Left)];
        if (down && !tapDown[side]) {
            if (tapClock - lastTap[side] <= kDoubleTap) {
                rollWanted = static_cast<int8_t>(side ? 1 : -1);
                rollHold = 0.12f;
                lastTap[side] = -10.0f;
            } else {
                lastTap[side] = tapClock;
            }
            lastTap[1 - side] = -10.0f;
        }
        tapDown[side] = down;
    }
    if (rollHold > 0.0f) local.roll = rollWanted;
    rollHold -= dt;

    // Une image très longue ne doit pas déclencher une rafale de pas.
    accumulator += std::min(dt, 0.1f);
    while (accumulator >= sim::kFixedDt) {
        float countdownBefore = world.countdown;
        if (clientTick) clientTick(clientUser, *this, local);
        else tick(local);
        accumulator -= sim::kFixedDt;
        react(previous);
        if (recordGhost && !previous.vehicles[localIndex].finished) {
            // Relevé du fantôme, du départ jusqu'à la ligne d'arrivée comprise.
            const sim::Vehicle& me = world.vehicles[localIndex];
            recording.record(me, world.raceTime);
            if (me.finished) {
                recording.finishTime = me.finishTime;
                recording.ship = me.ship;
                recording.trackLength = track.length;
            }
        }
        {
            const sim::Vehicle& me = world.vehicles[localIndex];
            if (world.countdown <= 0.0f && !me.finished)
                tally.step(sim::kFixedDt, sim::gapOf(world, track, me), me.scraping);
            if (me.finished && !scored) {
                scored = true;
                const int rank = sim::rankOf(world, localIndex);
                score = computeScore(tally, rank, world.count, config.difficulty);
                // Musique de fin : victoire sur le podium, qui compte deux
                // places à partir de quatre concurrents et trois à partir de six.
                const int podium = world.count >= 6 ? 3 : world.count >= 4 ? 2 : 1;
                if (synth) synth->jingle(rank <= podium ? 0 : 1);
                SDL_Log("Score %d (%c) : place %d, tempo %d, pilotage %d, bonus %d ; %d chocs, %.1f s de frottement, %d bonus en %.0f s",
                        score.total, score.grade, score.place, score.tempo, score.clean, score.items,
                        tally.hits, tally.scrape, tally.pickups, tally.time);
            }
        }
        if (pinToSync && world.countdown <= 0.0f) {
            // Démonstration : le vaisseau reste sur le point de synchronisation,
            // pour vérifier que la piste tombe sur la musique.
            sim::Vehicle& v = world.vehicles[localIndex];
            v.dist = track.distAt(world.raceTime);
            v.speed = track.at(v.dist).pace;
        }

        // Un bip à chaque seconde du compte à rebours, un signal au départ.
        if (synth && countdownBefore > 0.0f) {
            if (world.countdown <= 0.0f) synth->beep(1);
            else if (std::ceil(world.countdown) != std::ceil(countdownBefore) ||
                     countdownBefore == 3.0f)
                synth->beep(0);
        }
    }
    updatePresentation(dt);
}

float Race::shotThreat() const {
    float best = -1.0f;
    for (const sim::Missile& m : world.missiles) {
        const float t = sim::shotImpactTime(world, localIndex, m);
        if (t >= 0.0f && (best < 0.0f || t < best)) best = t;
    }
    return best;
}

void Race::updatePresentation(float dt) {
    const sim::Vehicle& player = world.vehicles[localIndex];

    // Un tir approche : des bips graves de plus en plus serrés, comme un radar
    // de recul, jusqu'au moment de vriller.
    {
        const float threat = shotThreat();
        alertWait -= dt;
        if (threat >= 0.0f && threat < kAlertLead && alertWait <= 0.0f) {
            if (synth) synth->play(Sfx::Alert, threat <= kDodgeLead ? 1.0f : 0.75f);
            alertWait = std::clamp(threat * 0.22f, 0.07f, 0.45f);
        }
        if (threat < 0.0f) alertWait = 0.0f;
    }

    // Niveaux de la musique pour le décor : montée vive, descente plus lente,
    // comme les barres d'un égaliseur.
    float target[music::Partition::kSpectrumBands];
    const bool playing = world.countdown <= 0.0f && world.raceTime < track.duration;
    sim::song(config.track).spectrumAt(playing ? sim::songTime(world, track, player) : -1.0f, target);
    for (int k = 0; k < music::Partition::kSpectrumBands; k++) {
        const float rate = target[k] > bands[k] ? 30.0f : 5.0f;
        bands[k] += (target[k] - bands[k]) * (1.0f - std::exp(-dt * rate));
    }
    if (synth) synth->setEngine(player.speed, player.boosting, player.scraping);

    // Caméra : suit le joueur avec un léger retard, s'élargit avec la vitesse.
    float follow = 1.0f - std::exp(-dt * 6.0f);
    camLateral += (player.lateral - camLateral) * follow;
    float speedRatio = std::clamp((player.speed - 250.0f) / (sim::kBoostSpeed - 250.0f), 0.0f, 1.2f);
    float fovTarget = 1.02f + 0.20f * speedRatio + (player.boosting ? 0.06f : 0.0f);
    camFov += (fovTarget - camFov) * (1.0f - std::exp(-dt * 3.0f));
    float shakeTarget = std::max(player.scraping ? 1.0f : (player.boosting ? 0.45f : 0.0f), 2.2f * hitShake);
    camShake += (shakeTarget - camShake) * (1.0f - std::exp(-dt * 10.0f));

    for (Fx& e : fx)
        if (e.age < e.life) e.age += dt;
    flash *= std::exp(-dt * 8.0f);
    hitShake *= std::exp(-dt * 4.0f);

    float decay = std::exp(-dt * 10.0f);
    for (int i = 0; i < world.count; i++) {
        smoothDist[i] *= decay;
        smoothLateral[i] *= decay;
    }
}

void Race::fill(const gfx::FontAtlas& fonts, glm::vec2 screen, gfx::FrameParams& out) {
    const float alpha = accumulator / sim::kFixedDt;
    const float time = static_cast<float>(world.time) + accumulator;

    out.time = time;
    out.shipCount = 0;

    HudState state;
    float playerDist = 0.0f;
    glm::vec3 shipPos[sim::kMaxVehicles];
    out.fxCount = 0;
    auto orb = [&](const glm::vec3& at, float radius, const glm::vec3& color, float gain) {
        if (out.fxCount >= gfx::kMaxFx || radius <= 0.0f || gain <= 0.0f) return;
        gfx::FxDraw& draw = out.fx[out.fxCount++];
        draw.model = glm::scale(glm::translate(glm::mat4(1.0f), at), glm::vec3(radius));
        draw.color = glm::vec4(color, gain);
    };
    for (int i = 0; i < world.count; i++) {
        const sim::Vehicle& a = previous.vehicles[i];
        const sim::Vehicle& b = world.vehicles[i];
        float dist = mixf(a.dist, b.dist, alpha) + smoothDist[i];
        float lateral = mixf(a.lateral, b.lateral, alpha) + smoothLateral[i];
        float roll = mixf(a.roll, b.roll, alpha);
        // Touché par un missile : le vaisseau part en vrille.
        if (b.stunTime > 0.0f) roll += (b.stunTime / sim::kStunDuration) * 12.566f;
        // Vrille : un tour complet sur le côté choisi, vif au début.
        if (b.rollDir != 0) {
            const float left = mixf(a.rollDir == b.rollDir ? a.rollTime : sim::kRollTime, b.rollTime, alpha);
            const float u = 1.0f - std::clamp(left / sim::kRollTime, 0.0f, 1.0f);
            roll -= static_cast<float>(b.rollDir) * 6.2832f * (1.0f - (1.0f - u) * (1.0f - u));
        }
        float bob = std::sin(time * 3.1f + static_cast<float>(i) * 1.7f) * 0.05f;
        if (i == localIndex) playerDist = dist;

        sim::TrackFrame f = track.at(dist);
        glm::vec3 up = glm::normalize(f.up * std::cos(roll) + f.right * std::sin(roll));
        glm::vec3 right = glm::cross(f.tangent, up);
        // En plein saut, le vaisseau quitte la piste.
        const float air = mixf(sim::airHeight(a), sim::airHeight(b), alpha);
        glm::vec3 pos = f.pos + f.right * lateral + f.up * (kHover + bob + air);
        if (i == localIndex) state.shipPos = pos + f.up * 0.6f;
        shipPos[i] = pos + f.up * 0.8f;
        // Turbo : un halo chaud enveloppe le vaisseau.
        if (b.boosting) orb(shipPos[i] - f.tangent * 3.0f, 5.5f + 0.8f * std::sin(time * 30.0f), {1.0f, 0.7f, 0.2f}, 2.2f);

        gfx::ShipDraw& ship = out.ships[out.shipCount++];
        // Le maillage regarde vers -Z, donc son axe +Z pointe vers l'arrière.
        ship.model = glm::mat4(glm::vec4(right * kShipScale, 0.0f),
                               glm::vec4(up * kShipScale, 0.0f),
                               glm::vec4(-f.tangent * kShipScale, 0.0f), glm::vec4(pos, 1.0f));
        ship.planePoint = f.pos;
        ship.planeNormal = f.up;
        ship.variant = b.ship;
        ship.trim = glm::vec4(kSlotColors[i].trim, 6.0f);
        ship.engine = glm::vec4(b.boosting ? glm::vec3(1.0f, 0.7f, 0.25f) : kSlotColors[i].engine, b.boosting ? 30.0f : 9.0f);
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

    // Fantôme : la trajectoire du meilleur temps, rejouée à l'heure de la course.
    out.ghostLevel = 0.0f;
    if (showGhost && ghost.valid()) {
        const float t = mixf(previous.raceTime, world.raceTime, alpha);
        // Arrivé, il s'efface en une demi-seconde.
        const float fade = std::clamp(1.0f - (t - ghost.finishTime) / 0.5f, 0.0f, 1.0f);
        const Ghost::Sample g = ghost.at(t);
        auto smooth = [](float a, float b, float x) {
            const float u = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
            return u * u * (3.0f - 2.0f * u);
        };
        // Il disparaît quand il se confond avec le vaisseau du joueur, comme
        // sur la grille, et quand il passe sous la caméra, dix mètres derrière.
        const sim::Vehicle& pa = previous.vehicles[localIndex];
        const sim::Vehicle& pb = world.vehicles[localIndex];
        const float d = g.dist - playerDist;
        const float across = g.lateral - (mixf(pa.lateral, pb.lateral, alpha) + smoothLateral[localIndex]);
        const float near = smooth(2.0f, 14.0f, std::sqrt(d * d + across * across));
        const float camera = d < 0.0f ? smooth(3.0f, 8.0f, std::abs(d + 10.0f)) : 1.0f;
        const float level = fade * near * camera;
        if (level > 0.01f) {
            const sim::TrackFrame f = track.at(g.dist);
            const glm::vec3 up = glm::normalize(f.up * std::cos(g.roll) + f.right * std::sin(g.roll));
            const glm::vec3 right = glm::cross(f.tangent, up);
            const glm::vec3 pos = f.pos + f.right * g.lateral + f.up * (kHover + g.height);
            out.ghost.model = glm::mat4(glm::vec4(right * kShipScale, 0.0f), glm::vec4(up * kShipScale, 0.0f),
                                        glm::vec4(-f.tangent * kShipScale, 0.0f), glm::vec4(pos, 1.0f));
            out.ghost.variant = ghost.ship;
            out.ghost.trim = glm::vec4(0.55f, 0.85f, 1.0f, 1.0f);
            out.ghostLevel = 3.0f * level;
            // Loin devant ou derrière, une étiquette le signale.
            if (std::abs(d) > 30.0f && state.tagCount < 8) {
                NameTag& tag = state.tags[state.tagCount++];
                tag.worldPos = pos + f.up * 3.4f;
                tag.name = tr("FANTÔME");
                tag.rank = 0;
                tag.color = glm::vec4(0.55f, 0.85f, 1.0f, fade);
            }
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
        // La comète est bien plus grosse qu'un tir, une mine un peu plus trapue.
        const float size = m.kind == sim::MissileKind::Comet ? 3.5f : m.kind == sim::MissileKind::Mine ? 1.6f : 1.0f;
        draw.model = glm::mat4(glm::vec4(f.right * size, 0.0f), glm::vec4(f.up * size, 0.0f),
                               glm::vec4(-f.tangent * size, 0.0f),
                               glm::vec4(f.pos + f.right * lateral + f.up * (0.9f * size), 1.0f));
        draw.planePoint = f.pos;
        draw.planeNormal = f.up;
        // Chaque projectile a son halo, pour qu'on le voie venir de loin.
        const glm::vec3 at = f.pos + f.right * lateral + f.up * (0.9f * size);
        if (m.kind == sim::MissileKind::Comet) {
            draw.flame = glm::vec4(1.0f, 0.5f, 0.1f, 30.0f);
            draw.flameScale = 2.6f;
            orb(at, 7.0f + 1.0f * std::sin(time * 24.0f), {1.0f, 0.55f, 0.12f}, 6.0f);
            orb(at - f.tangent * 14.0f, 13.0f, {1.0f, 0.3f, 0.05f}, 1.6f);
        } else if (m.kind == sim::MissileKind::Mine) {
            draw.flameScale = 0.0f;
            // Elle clignote, de plus en plus vite à mesure qu'elle s'épuise.
            const float blink = 0.5f + 0.5f * std::sin(time * (6.0f + 20.0f * std::max(0.0f, 1.0f - m.life / 6.0f)));
            orb(at, 2.6f + 1.2f * blink, {1.0f, 0.2f, 0.1f}, 2.0f + 4.0f * blink);
        } else {
            draw.flameScale = 0.9f;
            orb(at, 2.4f, {1.0f, 0.3f, 0.8f}, 5.0f);
        }
    }
    // Décor vivant : des objets volent autour de la piste, dans une fenêtre
    // qui suit le joueur. Une quarantaine en ville et autour de la station,
    // trois bandes de quatre créatures dans le canyon. Leur trajectoire ne dépend que de leur
    // numéro et du temps, donc elle est la même pour tous les joueurs.
    out.flyerCount = 0;
    {
        const gfx::Theme& look = gfx::themeFor(theme);
        const float window = 3400.0f, behind = 300.0f;
        auto hash = [](int i, int salt) {
            uint32_t h = static_cast<uint32_t>(i) * 0x9E3779B1u ^ static_cast<uint32_t>(salt) * 0x85EBCA77u;
            h ^= h >> 15;
            h *= 0x2C1B3C6Du;
            h ^= h >> 12;
            return static_cast<float>(h & 0xFFFFu) / 65535.0f;
        };
        // En décor allégé : deux bandes au lieu de trois, vingt objets au lieu de quarante.
        const int count = look.scenery == gfx::Scenery::Canyon ? (lightDecor ? 8 : 12) : (lightDecor ? 20 : 40);
        for (int i = 0; i < count && out.flyerCount < gfx::kMaxFlyers; i++) {
            gfx::FlyerDraw& draw = out.flyers[out.flyerCount];
            float lateral, height, speed, size, flap = 1.0f, spin = 0.0f;
            float start = hash(i, 9) * window;
            const float side = i % 2 ? 1.0f : -1.0f;
            if (look.scenery == gfx::Scenery::City) {
                // Circulation aérienne : des files à trois altitudes, dans les deux sens.
                draw.kind = 0;
                const int lane = i % 6;
                lateral = side * (34.0f + 26.0f * static_cast<float>(lane / 2) + 70.0f * hash(i, 1));
                height = 16.0f + 26.0f * static_cast<float>(lane % 3) + 8.0f * hash(i, 2);
                speed = (lane % 2 ? -1.0f : 1.0f) * (90.0f + 80.0f * hash(i, 3));
                size = 3.2f + 2.4f * hash(i, 4);
                draw.trim = glm::vec4(lane % 2 ? look.neonB : look.neonA, 8.0f);
                draw.light = glm::vec4(lane % 2 ? glm::vec3(1.0f, 0.9f, 0.7f) : glm::vec3(1.0f, 0.15f, 0.1f), 9.0f);
            } else if (look.scenery == gfx::Scenery::Canyon) {
                // Créatures : trois bandes de quatre, en chevron, lentes, qui
                // ondulent et battent des ailes. Les bandes sont réparties le
                // long de la fenêtre pour qu'on en croise une de temps en temps.
                draw.kind = 1;
                const int flock = i / 4;
                const float place = static_cast<float>(i % 4) - 1.5f;
                lateral = (flock % 2 ? 1.0f : -1.0f) * (50.0f + 130.0f * hash(flock, 1)) + 22.0f * place;
                height = 34.0f + 70.0f * hash(flock, 2) + 5.0f * std::abs(place) +
                         12.0f * std::sin(time * 0.5f + static_cast<float>(flock));
                start = (static_cast<float>(flock) + hash(flock, 9) * 0.6f) / 3.0f * window - 16.0f * std::abs(place);
                speed = 55.0f + 30.0f * hash(flock, 3);
                size = 3.5f + 2.5f * hash(flock, 4);
                flap = std::sin(time * (4.5f + 2.0f * hash(flock, 5)) + place * 0.7f);
                draw.trim = glm::vec4(look.neonA, 7.0f);
                draw.light = glm::vec4(look.neonB, 4.0f);
            } else {
                // Drones : ils dérivent lentement et tournent sur eux-mêmes.
                draw.kind = 2;
                lateral = side * (32.0f + 170.0f * hash(i, 1));
                height = -20.0f + 110.0f * hash(i, 2) + 8.0f * std::sin(time * 0.8f + static_cast<float>(i));
                speed = 20.0f + 60.0f * hash(i, 3);
                size = 3.0f + 4.0f * hash(i, 4);
                spin = time * (0.6f + 1.4f * hash(i, 5));
                const bool blink = std::fmod(time * (0.8f + hash(i, 6)) + hash(i, 7), 1.0f) < 0.5f;
                draw.trim = glm::vec4(i % 3 ? look.neonA : look.neonB, 5.0f);
                draw.light = glm::vec4(1.0f, 0.3f, 0.2f, blink ? 12.0f : 1.0f);
            }
            // Position le long de la piste, ramenée dans la fenêtre autour du joueur.
            float along = start + speed * time - playerDist + behind;
            along = along - std::floor(along / window) * window;
            const float dist = playerDist - behind + along;
            const sim::TrackFrame f = track.at(dist);
            // Repère à plat : dans une vrille, le décor ne tourne pas avec la piste.
            glm::vec3 forward = glm::normalize(glm::vec3(f.tangent.x, 0.0f, f.tangent.z)) * (speed < 0.0f ? -1.0f : 1.0f);
            const glm::vec3 up{0.0f, 1.0f, 0.0f};
            const glm::vec3 right = glm::normalize(glm::cross(forward, up));
            if (spin != 0.0f) forward = forward * std::cos(spin) + right * std::sin(spin);
            const glm::vec3 across = glm::normalize(glm::cross(forward, up));
            const glm::vec3 pos = f.pos + right * (speed < 0.0f ? -lateral : lateral) + up * height;
            draw.model = glm::mat4(glm::vec4(across * size, 0.0f), glm::vec4(up * (size * flap), 0.0f),
                                   glm::vec4(-forward * size, 0.0f), glm::vec4(pos, 1.0f));
            out.flyerCount++;
        }
    }
    // Explosions, ondes et éclats.
    for (const Fx& e : fx) {
        if (e.age >= e.life) continue;
        const float u = e.age / e.life, ease = 1.0f - (1.0f - u) * (1.0f - u);
        const glm::vec3 at = e.follow >= 0 && e.follow < world.count ? shipPos[e.follow]
                                                                  : track.point(e.dist, e.lateral, e.height);
        orb(at, e.from + (e.to - e.from) * ease, e.color, e.gain * (1.0f - u) * (1.0f - u));
    }

    const sim::Vehicle& player = world.vehicles[localIndex];
    out.speed = player.speed;
    out.trackDist = playerDist;
    for (int k = 0; k < 8; k++) out.bands[k] = bands[k];
    // Battement : 1 sur chaque temps du morceau, plus fort sur le premier de
    // la mesure, puis retombe.
    {
        const std::vector<music::Beat>& beats = sim::song(config.track).beats;
        const float songTime = sim::songTime(world, track, player);
        auto next = std::upper_bound(beats.begin(), beats.end(), songTime,
                                     [](float t, const music::Beat& b) { return t < b.time; });
        out.pulse = 0.0f;
        if (next != beats.begin() && world.countdown <= 0.0f && songTime < track.duration) {
            const music::Beat& last = *(next - 1);
            out.pulse = (last.bar == 0 ? 1.0f : 0.55f) * std::exp(-(songTime - last.time) * 7.0f);
        }
        // Un morceau hargneux cogne plus fort : néons plus vifs, et la caméra
        // encaisse chaque temps.
        const float drive = sim::song(config.track).driveAt(songTime);
        beatKick = out.pulse * drive;
        out.pulse *= 0.6f + 0.9f * drive;
    }

    // Caméra de poursuite : derrière et au-dessus du joueur, inclinée avec la
    // piste, et qui regarde loin devant.
    sim::TrackFrame camFrame = track.at(playerDist - 10.0f);
    glm::vec3 shake = camFrame.right * (std::sin(time * 71.0f) * 0.05f * camShake) +
                      camFrame.up * (std::sin(time * 83.0f) * 0.04f * camShake);
    out.camPos = camFrame.pos + camFrame.right * (camLateral * 0.82f) + camFrame.up * 4.8f + shake;
    out.camTarget = track.point(playerDist + 26.0f, camLateral * 0.55f, 0.0f);
    out.camUp = camFrame.up;
    out.fovY = camFov + 0.05f * beatKick;
    // La caméra accompagne le saut, un peu en retrait : on voit la piste se dérober.
    {
        const float air = sim::airHeight(player);
        out.camPos += camFrame.up * (0.75f * air);
        out.camTarget += camFrame.up * (0.45f * air);
        out.fovY += 0.004f * air;
    }
    out.camPos += camFrame.up * (0.45f * beatKick) - camFrame.tangent * (0.8f * beatKick);
    if (orbit) {
        // Vue de présentation : la caméra tourne autour du vaisseau.
        sim::TrackFrame f = track.at(playerDist);
        glm::vec3 center = f.pos + f.right * player.lateral + f.up * 1.2f;
        out.camPos = center + (f.right * std::sin(orbitAngle) - f.tangent * std::cos(orbitAngle)) * 16.0f +
                     f.up * 11.0f;
        glm::vec3 view = glm::normalize(center - out.camPos);
        glm::vec3 side = glm::normalize(glm::cross(view, f.up));
        out.camTarget = center - side * orbitShift.x + f.up * orbitShift.y;
        out.camUp = f.up;
        out.fovY = 0.85f;
    }
    out.planePoint = camFrame.pos;
    out.planeNormal = camFrame.up;

    out.hud = nullptr;
    if (!showHud) return;

    state.parts = hudParts;
    state.countdownY = countdownY;
    state.position = sim::rankOf(world, localIndex);
    state.racers = world.count;
    state.remaining = std::max(0.0f, track.duration - sim::songTime(world, track, player));
    state.showGap = !world.ownClock && world.countdown <= 0.0f && !player.finished;
    state.gap = sim::gapOf(world, track, player);
    state.countdown = world.countdown;
    state.startBoost = player.startBoost;
    state.raceTime = player.finished ? player.finishTime : world.raceTime;
    state.finished = player.finished;
    state.score = score;
    state.bestScore = bestScore;
    state.newRecord = newRecord;
    state.finishTime = player.finishTime;
    state.bestTime = bestTime;
    state.newBestTime = newBestTime;
    // Écart avec le fantôme : le temps de course moins celui qu'il avait au
    // même endroit. Négatif quand le joueur est en avance.
    state.showGhostGap = false;
    if (showGhost && ghost.valid() && world.countdown <= 0.0f && !player.finished) {
        const float at = ghost.timeAt(player.dist);
        if (at >= 0.0f) {
            state.showGhostGap = true;
            state.ghostGap = world.raceTime - at;
        }
    }
    state.speedKmh = player.speed * 3.6f;
    // Radar : la piste à venir vue de dessus, dans le repère du vaisseau.
    {
        const sim::TrackFrame here = track.at(playerDist);
        const glm::vec3 forward = glm::normalize(glm::vec3(here.tangent.x, 0.0f, here.tangent.z));
        const glm::vec3 side{-forward.z, 0.0f, forward.x};
        const float behind = 250.0f, range = 2600.0f;
        minimap.clear();
        for (float d = -behind; d <= range; d += 60.0f) {
            const glm::vec3 rel = track.at(playerDist + d).pos - here.pos;
            // L'avant du vaisseau pointe vers le haut du cadre.
            minimap.push_back({glm::dot(rel, side) / (range * 0.5f),
                               0.8f - 1.7f * (glm::dot(rel, forward) + behind) / (range + behind)});
        }
        state.minimap = &minimap;
        state.progress = behind / (range + behind);
    }
    // Avancement de chaque vaisseau dans le morceau.
    state.markCount = world.count;
    for (int i = 0; i < world.count; i++) {
        state.marks[i] = std::clamp(world.vehicles[i].dist / track.length, 0.0f, 1.0f);
        state.markColors[i] = glm::vec4(kSlotColors[i].trim, 1.0f);
    }
    state.localMark = localIndex;
    state.boosting = player.boosting;
    for (int b = 0; b < static_cast<int>(Button::Count); b++) state.pressed[b] = controls.pressed[b];
    state.statsLine = statsLine;
    state.item = static_cast<int>(player.item);
    state.flash = glm::vec4(flashColor, flash);
    state.shielded = player.shieldTime > 0.0f;
    // Un tir arrive : il est derrière, proche, et personne ne le sépare du joueur.
    state.incoming = false;
    for (const sim::Missile& m : world.missiles) {
        if (!m.active || m.kind == sim::MissileKind::Mine || m.owner == localIndex) continue;
        const float behind = player.dist - m.dist;
        if (behind <= 0.0f || behind > 450.0f) continue;
        bool covered = false;
        for (int i = 0; i < world.count; i++)
            covered = covered || (i != localIndex && i != m.owner && world.vehicles[i].dist > m.dist &&
                                  world.vehicles[i].dist < player.dist);
        if (!covered) state.incoming = true;
    }
    state.threat = shotThreat();
    state.rollReady = player.rollCooldown <= 0.0f && player.stunTime <= 0.0f;
    state.rolling = player.rollTime > 0.0f;
    state.gridStart = config.gridStart;

    hud.begin(fonts, screen.x, screen.y);
    buildHud(hud, state, gfx::viewProjection(out, screen.x / screen.y));
    out.hud = &hud;
}

} // namespace game

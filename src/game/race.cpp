#include "game/race.h"

#include "game/music.h"
#include "game/songs.h"
#include "sim/generate.h"

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

void Race::init(const RaceConfig& cfg, int local) {
    // La piste se génère à partir de la partition du morceau.
    // Celle du morceau du joueur est déjà en place, posée par la bibliothèque.
    const bool custom = cfg.track == sim::kCustomTrack;
    if (!custom && !keepSong) sim::setSong(cfg.track, Music::partition(cfg.track));
    theme = custom ? customTheme() : cfg.track;
    songSerial = custom ? 100 + customSerial() : cfg.track;
    start(cfg, local);
    accumulator = 0.0f;
    camLateral = world.vehicles[localIndex].lateral;
    for (int i = 0; i < sim::kMaxVehicles; i++) smoothDist[i] = smoothLateral[i] = 0.0f;

    minimap.clear();
}

void Race::update(float dt, const Controls& input) {
    controls = input;
    sim::Input local;
    local.steer = input.steer;
    local.boost = input.pressed[static_cast<int>(Button::Boost)];

    // Appui de boost à côté du temps : on dit au joueur de quel côté il s'est
    // trompé. Pendant le délai entre deux accords, l'appui ne compte pas.
    missTime = std::max(0.0f, missTime - dt);
    {
        const sim::Vehicle& me = world.vehicles[localIndex];
        const bool pressed = local.boost && !boostWas;
        boostWas = local.boost;
        if (pressed && !autopilot && world.countdown <= 0.0f && !me.finished && me.stunTime <= 0.0f &&
            me.accordCooldown <= 0.0f && !track.beats.empty()) {
            const float now = sim::songTime(world, track, me);
            auto next = std::lower_bound(track.beats.begin(), track.beats.end(), now);
            const float toNext = next != track.beats.end() ? *next - now : 1e9f;
            const float sincePrev = next != track.beats.begin() ? now - *(next - 1) : 1e9f;
            if (std::min(toNext, sincePrev) > sim::kBeatTolerance) {
                missKind = toNext < sincePrev ? 1 : 2;
                missTime = 0.7f;
            }
        }
    }

    // Une image très longue ne doit pas déclencher une rafale de pas.
    accumulator += std::min(dt, 0.1f);
    while (accumulator >= sim::kFixedDt) {
        float countdownBefore = world.countdown;
        if (clientTick) clientTick(clientUser, *this, local);
        else tick(local);
        accumulator -= sim::kFixedDt;
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

void Race::updatePresentation(float dt) {
    const sim::Vehicle& player = world.vehicles[localIndex];

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
    float shakeTarget = player.scraping ? 1.0f : (player.boosting ? 0.25f : 0.0f);
    camShake += (shakeTarget - camShake) * (1.0f - std::exp(-dt * 10.0f));

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
        if (i == localIndex) state.shipPos = pos + f.up * 0.6f;

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

    state.energy = player.energy;
    state.position = sim::rankOf(world, localIndex);
    state.racers = world.count;
    state.remaining = std::max(0.0f, track.duration - sim::songTime(world, track, player));
    state.showGap = !world.ownClock && world.countdown <= 0.0f && !player.finished;
    state.gap = sim::gapOf(world, track, player);
    state.accord = player.accord / sim::kAccordTime;
    state.countdown = world.countdown;
    state.raceTime = player.finished ? player.finishTime : world.raceTime;
    state.finished = player.finished;
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
    state.combo = player.combo;
    // Anneau de rythme : part de l'intervalle qui reste avant le prochain temps.
    state.beatPhase = -1.0f;
    if (world.countdown <= 0.0f && !player.finished && !track.beats.empty()) {
        const float now = sim::songTime(world, track, player) + accumulator;
        auto next = std::lower_bound(track.beats.begin(), track.beats.end(), now);
        if (next != track.beats.end() && next != track.beats.begin())
            state.beatPhase = std::clamp((*next - now) / std::max(0.05f, *next - *(next - 1)), 0.0f, 1.0f);
    }
    state.cooldown = std::clamp(player.accordCooldown / sim::kAccordCooldown, 0.0f, 1.0f);
    // Lumière de rythme sur le dos du vaisseau. Elle monte à l'approche du
    // temps et culmine dessus : il faut appuyer quand elle est au plus fort.
    // Jaune quand un accord est possible, bleu pâle pendant le délai, vert sur
    // un accord réussi, rouge sur un appui raté, rose quand une décharge arrive.
    if (state.beatPhase >= 0.0f && localIndex < out.shipCount) {
        const float near = 1.0f - state.beatPhase;
        const float charge = near * near * near;
        glm::vec3 tint{1.0f, 0.78f, 0.22f};
        float level = 0.06f + 1.5f * charge;
        if (player.accordCooldown > 0.0f) {
            tint = {0.35f, 0.75f, 1.0f};
            level = 0.06f + 0.35f * charge;
        }
        if (player.accord > 0.0f) {
            tint = {0.35f, 1.0f, 0.45f};
            level = 0.9f + 1.8f * player.accord / sim::kAccordTime;
        } else if (missTime > 0.0f) {
            tint = {1.0f, 0.12f, 0.08f};
            level = 2.0f * missTime / 0.7f;
        }
        out.ships[localIndex].glow = glm::vec4(tint, level);
    }
    state.miss = missTime > 0.0f ? missKind : 0;
    state.missFade = missTime / 0.7f;
    state.comboSize = sim::kComboSize;
    state.armored = player.energy >= sim::kArmorCost;
    // Une décharge arrive : elle est derrière, proche, et personne ne la sépare du joueur.
    state.incoming = false;
    for (const sim::Missile& m : world.missiles) {
        if (!m.active || m.owner == localIndex) continue;
        const float behind = player.dist - m.dist;
        if (behind <= 0.0f || behind > 450.0f) continue;
        bool shielded = false;
        for (int i = 0; i < world.count; i++)
            shielded = shielded || (i != localIndex && i != m.owner && world.vehicles[i].dist > m.dist &&
                                    world.vehicles[i].dist < player.dist);
        if (!shielded) state.incoming = true;
    }
    state.gridStart = config.gridStart;

    hud.begin(fonts, screen.x, screen.y);
    buildHud(hud, state, gfx::viewProjection(out, screen.x / screen.y));
    out.hud = &hud;
}

} // namespace game

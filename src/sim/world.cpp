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
constexpr float kScrub = 1.6f;        // vitesse perdue par mètre de glissade
constexpr float kBoostDrain = 1.0f / 5.0f;   // énergie par seconde
// La jauge se recharge surtout par le rythme : plaques et accords. La
// recharge passive n'est qu'un filet.
constexpr float kEnergyRegen = 0.4f / 24.0f;

struct Clock {
    float raceTime;
    bool own;
};

// Effet des éléments de piste sur un vaisseau qui vient d'avancer de `before`
// à v.dist.
void applyFeatures(Vehicle& v, const ShipStats& stats, const Track& track, float before) {

    float moved = v.dist - before;
    for (const Feature& f : track.features) {
        if (f.type == FeatureType::Barrier) {
            // À cette vitesse un pas dépasse l'épaisseur de la barrière : on
            // teste le franchissement, pas la présence.
            if (track.ahead(before, f.dist) >= moved) continue;
            if (std::abs(v.lateral - f.lateral) > f.halfWidth + kHitRadius) continue;
            if (v.energy >= kArmorCost) {
                // Armure : la jauge encaisse le choc à la place du vaisseau.
                v.energy -= kArmorCost;
                v.shieldTime = kArmorFlash;
                continue;
            }
            v.combo = 0;
            v.speed *= 1.0f - kBarrierLoss / stats.sturdy;
            v.stunTime = std::max(v.stunTime, kBarrierStun / stats.sturdy);
            v.energy = std::max(0.0f, v.energy - kBarrierEnergy);
            continue;
        }
        if (track.ahead(f.dist, v.dist) > f.length) continue;
        if (std::abs(v.lateral - f.lateral) > f.halfWidth) continue;
        if (f.type == FeatureType::EnergyPad) {
            // Une plaque recharge la jauge, une fois, à l'entrée.
            if (before < f.dist) v.energy = std::min(1.0f, v.energy + kPadEnergy);
        }
    }
}

void stepVehicleDt(Vehicle& v, const Input& in, const Track& track, const Clock& clock, float dt) {
    const ShipStats& stats = shipStats(v.ship);
    TrackFrame f = track.at(v.dist);
    float steer = std::clamp(in.steer, -1.0f, 1.0f);

    v.shieldTime = std::max(0.0f, v.shieldTime - dt);
    bool stunned = v.stunTime > 0.0f;
    v.stunTime = std::max(0.0f, v.stunTime - dt);
    if (stunned) steer *= 0.3f;

    // Horloge commune : un élastique retient qui prend de l'avance sur la
    // musique et aspire qui prend du retard. Tout le peloton reste dans la
    // même phrase.
    float band = 1.0f;
    if (!clock.own) {
        const float gap = f.time - clock.raceTime;
        band = gap > 0.0f ? std::max(0.75f, 1.0f - kLeadDrag * gap / kWindow)
                          : std::min(1.25f, 1.0f - kDraft * gap / kWindow);
    }

    // Frappe du boost. On juge la régularité du joueur, pas son alignement
    // sur le morceau : une frappe est juste quand elle tombe une période après
    // la précédente. Elle donne alors un court boost gratuit, qui tient
    // jusqu'à la frappe suivante sans garder le bouton enfoncé. Les deux
    // premières frappes posent la cadence. Une frappe irrégulière casse la
    // série et pose une nouvelle cadence.
    const bool pressed = in.boost && !v.boostHeld;
    v.boostHeld = in.boost;
    v.accord = std::max(0.0f, v.accord - dt);
    v.accordCooldown = std::max(0.0f, v.accordCooldown - dt);
    v.sinceTap = std::min(10.0f, v.sinceTap + dt);
    // Cadence abandonnée : il faudra la reposer.
    if (v.tapPeriod > 0.0f && v.sinceTap > 2.6f * v.tapPeriod) {
        v.tapPeriod = 0.0f;
        v.combo = 0;
    }
    if (pressed && !stunned && v.accordCooldown <= 0.0f) {
        const float gap = v.sinceTap;
        v.sinceTap = 0.0f;
        v.accordCooldown = kAccordCooldown;
        v.tapCount++;
        const float period = v.tapPeriod;
        // Une frappe sautée reste dans le rythme : deux périodes font l'affaire.
        const bool regular = period > 0.0f && (std::abs(gap - period) <= kTapTolerance * period ||
                                               std::abs(gap - 2.0f * period) <= kTapTolerance * period);
        // La cadence doit tenir à un temps, un demi-temps ou deux temps du morceau.
        bool onTempo = !track.tempoKnown;
        if (period > 0.0f && !onTempo) {
            const float beat = track.beatPeriod(clock.own ? f.time : clock.raceTime);
            for (float ratio : {0.5f, 1.0f, 2.0f})
                onTempo = onTempo || std::abs(period - ratio * beat) <= kTempoTolerance * ratio * beat;
        }
        if (regular && onTempo) {
            v.tapVerdict = kTapGood;
            v.accord = std::min(1.2f, period * 1.3f);
            v.combo++;
            v.energy = std::min(1.0f, v.energy + kAccordEnergy * period);
            // La cadence suit doucement le joueur, sans se laisser entraîner.
            if (std::abs(gap - period) <= kTapTolerance * period) v.tapPeriod = 0.75f * period + 0.25f * gap;
        } else if (regular) {
            v.tapVerdict = kTapOffTempo;
            v.accord = 0.0f;
            v.combo = 0;
        } else {
            v.tapVerdict = period <= 0.0f ? kTapStart : gap < period ? kTapEarly : kTapLate;
            v.accord = 0.0f;
            v.combo = 0;
            // Nouvelle référence, si l'écart ressemble à une cadence.
            v.tapPeriod = gap >= kMinTapPeriod && gap <= kMaxTapPeriod ? gap : 0.0f;
        }
    }
    if (stunned) {
        v.combo = 0;
        v.accord = 0.0f;
    }
    const bool accord = v.accord > 0.0f;

    v.boosting = (accord || (in.boost && v.energy > 0.0f)) && !stunned;
    // Les vitesses suivent celle du point de synchronisation : un vaisseau
    // en croisière avance avec la musique.
    const float base = f.pace / kSyncEdge * band;
    const float pace = base / kCruiseSpeed;
    float cruise = base * stats.topSpeed;
    float target = v.boosting ? kBoostSpeed * pace * stats.topSpeed * (accord ? 1.05f : 1.0f) : cruise;
    if (stunned) target = cruise * 0.55f;
    float delta = target - v.speed;
    float gain = (v.boosting ? kBoostAccel * (accord ? 1.6f : 1.0f) : kAccel) * stats.accel;
    v.speed += std::clamp(delta, -(stunned ? 4.0f : 1.0f) * kDecel * dt, gain * dt);

    v.energy += (v.boosting ? (accord ? 0.0f : -kBoostDrain) : kEnergyRegen * stats.regen) * dt;

    // Braquage, plus la dérive vers l'extérieur du virage.
    float steerSpeed = kSteerSpeed * stats.handling;
    v.lateralVel += (steer * steerSpeed - v.lateralVel) * std::min(1.0f, kSteerResponse * dt);
    float drift = -f.curvature * v.speed * v.speed * kDrift / stats.grip;
    v.lateral += (v.lateralVel + drift) * dt;
    // Glisser coûte de la vitesse : un vaisseau qui accroche sort plus vite
    // des virages qu'un vaisseau rapide en ligne droite.
    v.speed -= std::abs(drift) * kScrub * dt;

    // Murets : rebond mou, perte de vitesse et d'énergie.
    float limit = track.halfWidth - kShipHalfWidth;
    v.scraping = false;
    if (std::abs(v.lateral) > limit) {
        float side = v.lateral > 0.0f ? 1.0f : -1.0f;
        v.lateral = side * limit;
        if (v.lateralVel * side > 0.0f) v.lateralVel *= -0.35f;
        v.speed -= v.speed * (0.55f / stats.sturdy) * dt;
        v.energy -= 0.12f * dt;
        v.scraping = true;
    }
    v.energy = std::clamp(v.energy, 0.0f, 1.0f);

    // À l'intérieur d'un virage, la même vitesse couvre plus de piste.
    float pathScale = 1.0f / std::max(0.5f, 1.0f - f.curvature * v.lateral);
    float before = v.dist;
    v.dist += v.speed * pathScale * dt;
    applyFeatures(v, stats, track, before);

    float rollTarget = -steer * 0.38f - f.curvature * v.speed * 0.25f;
    v.roll += (rollTarget - v.roll) * std::min(1.0f, 7.0f * dt);
}

// Série d'accords complète : la décharge part seule sur le vaisseau de devant.
// En tête de course il n'y a personne à viser, la série rend de l'énergie.
void useItems(World& world, int index) {
    Vehicle& v = world.vehicles[index];
    if (v.combo < kComboSize || v.finished) return;
    v.combo = 0;
    bool someoneAhead = false;
    for (int i = 0; i < world.count; i++)
        someoneAhead = someoneAhead || (i != index && world.vehicles[i].dist > v.dist + 10.0f &&
                                        world.vehicles[i].dist < v.dist + 900.0f);
    if (!someoneAhead) {
        v.energy = std::min(1.0f, v.energy + kLeaderGain);
        return;
    }
    for (Missile& m : world.missiles) {
        if (m.active) continue;
        m = Missile{};
        m.active = true;
        m.owner = index;
        m.dist = v.dist + 7.0f;
        m.lateral = v.lateral;
        m.speed = v.speed + 170.0f;
        m.life = 4.5f;
        break;
    }
}

void stepMissiles(World& world, const Track& track, float dt) {
    const float limit = track.halfWidth - 1.0f;
    for (Missile& m : world.missiles) {
        if (!m.active) continue;

        // Cible : le vaisseau le plus proche devant, autre que le tireur.
        const Vehicle* target = nullptr;
        float best = 900.0f;
        for (int i = 0; i < world.count; i++) {
            if (i == m.owner) continue;
            float ahead = world.vehicles[i].dist - m.dist;
            if (ahead > -4.0f && ahead < best) {
                best = ahead;
                target = &world.vehicles[i];
            }
        }
        if (target) {
            float delta = target->lateral - m.lateral;
            m.lateral += std::clamp(delta, -34.0f * dt, 34.0f * dt);
        }
        m.lateral = std::clamp(m.lateral, -limit, limit);
        float before = m.dist;
        m.dist += m.speed * dt;
        m.life -= dt;
        // Une barrière arrête un missile : on peut s'abriter derrière.
        for (const Feature& f : track.features) {
            if (f.type != FeatureType::Barrier) continue;
            if (track.ahead(before, f.dist) < m.dist - before &&
                std::abs(m.lateral - f.lateral) < f.halfWidth)
                m.life = 0.0f;
        }
        if (m.life <= 0.0f) {
            m.active = false;
            continue;
        }

        for (int i = 0; i < world.count; i++) {
            if (i == m.owner) continue;
            Vehicle& v = world.vehicles[i];
            if (std::abs(v.dist - m.dist) > 6.5f || std::abs(v.lateral - m.lateral) > 3.8f)
                continue;
            m.active = false;
            // Parade : la cible est en plein accord. Elle absorbe la décharge
            // et en récupère l'énergie.
            if (v.accord > 0.0f) {
                v.energy = std::min(1.0f, v.energy + kAbsorbGain);
                v.shieldTime = kArmorFlash;
            } else {
                const ShipStats& stats = shipStats(v.ship);
                v.stunTime = kStunDuration / stats.sturdy;
                v.speed *= 1.0f - 0.4f / stats.sturdy;
                v.energy = std::max(0.0f, v.energy - 0.10f);
                Vehicle& shooter = world.vehicles[m.owner];
                shooter.energy = std::min(1.0f, shooter.energy + kHitGain);
            }
            break;
        }
    }
}

} // namespace

void stepVehicle(Vehicle& vehicle, const Input& input, const Track& track, const World& world) {
    stepVehicleDt(vehicle, input, track, {world.raceTime, world.ownClock}, kFixedDt);
}

float cruiseSpeed(const Vehicle& vehicle, const Track& track) {
    return track.at(vehicle.dist).pace / kSyncEdge * shipStats(vehicle.ship).topSpeed;
}

void step(World& world, const Track& track, const Input* inputs) {
    world.tick++;
    world.time += kFixedDt;
    if (world.countdown > 0.0f) {
        world.countdown -= kFixedDt;
        return;
    }
    world.raceTime += kFixedDt;

    const float finishDist = track.length;
    const Clock clock{world.raceTime, world.ownClock};
    for (int i = 0; i < world.count; i++) {
        Vehicle& v = world.vehicles[i];
        float before = v.dist;
        useItems(world, i);
        stepVehicleDt(v, inputs[i], track, clock, kFixedDt);
        if (!v.finished && v.dist >= finishDist) {
            v.finished = true;
            // Instant exact du passage, interpolé dans le pas.
            float moved = v.dist - before;
            float fraction = moved > 0.0f ? (finishDist - before) / moved : 1.0f;
            v.finishTime = world.raceTime - kFixedDt * (1.0f - fraction);
        }
    }

    stepMissiles(world, track, kFixedDt);

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
    // Un contact ne doit pousser personne à travers un muret.
    const float limit = track.halfWidth - kShipHalfWidth;
    for (int i = 0; i < world.count; i++)
        world.vehicles[i].lateral = std::clamp(world.vehicles[i].lateral, -limit, limit);
}

namespace {

// Tirage pseudo-aléatoire reproductible : dépend du pilote et de l'instant,
// jamais d'un état global, donc deux exécutions donnent la même course.
float chance(int index, uint32_t slot, uint32_t salt) {
    uint32_t h = static_cast<uint32_t>(index) * 0x9E3779B1u ^ slot * 0x85EBCA77u ^ salt * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return static_cast<float>(h & 0xFFFFFFu) / 16777216.0f;
}

} // namespace

Input autopilot(const World& world, int index, const Track& track, const AiProfile& profile,
                float laneOffset) {
    const Vehicle& v = world.vehicles[index];
    const ShipStats& stats = shipStats(v.ship);
    Input in;

    // Regarde devant pour prendre le virage à temps.
    TrackFrame here = track.at(v.dist);
    TrackFrame ahead = track.at(v.dist + v.speed * 0.6f);
    float limit = track.halfWidth - kShipHalfWidth - 1.5f;
    float line = ahead.curvature * profile.lineGain + laneOffset;
    if (profile.weave > 0.0f)
        line += profile.weave * std::sin(static_cast<float>(world.time) * 0.9f +
                                         static_cast<float>(index) * 2.1f);

    // Réactions aux autres vaisseaux.
    for (int i = 0; i < world.count; i++) {
        if (i == index) continue;
        const Vehicle& other = world.vehicles[i];
        float along = other.dist - v.dist;
        float across = other.lateral - v.lateral;
        if (profile.ram && std::abs(along) < 14.0f && std::abs(across) < 11.0f) {
            line = other.lateral; // se rabat dessus
        } else if (profile.block && along < -8.0f && along > -45.0f) {
            line = other.lateral; // ferme la porte
        } else if (!profile.ram && along > 0.0f && along < 30.0f && std::abs(across) < 6.5f) {
            line += across > 0.0f ? -7.5f : 7.5f; // double par le côté libre
        }
    }
    line = std::clamp(line, -limit, limit);

    float steerSpeed = kSteerSpeed * stats.handling;
    // Éléments de piste : viser la plaque d'énergie atteignable la plus proche,
    // puis s'écarter des barrières, qui ont le dernier mot.
    const float look = v.speed * profile.foresight;
    float padAhead = look;
    for (const Feature& f : track.features) {
        if (f.type != FeatureType::EnergyPad) continue;
        float ahead = track.ahead(v.dist, f.dist);
        if (ahead >= padAhead) continue;
        float reach = steerSpeed * 0.8f * ahead / std::max(v.speed, 1.0f) + f.halfWidth * 0.5f;
        if (std::abs(f.lateral - v.lateral) > reach) continue;
        padAhead = ahead;
        line = f.lateral;
    }
    // Un obstacle à la fois : seul le plus proche compte, avec ses voisins
    // immédiats. Sinon le suivant le ferait revenir dans le premier.
    auto hazardAhead = [&](const Feature& f) {
        if (track.ahead(f.dist, v.dist) <= f.length) return 0.0f; // déjà dedans
        return track.ahead(v.dist, f.dist);
    };
    float nearest = look;
    for (const Feature& f : track.features)
        if (f.type != FeatureType::EnergyPad) nearest = std::min(nearest, hazardAhead(f));
    bool dodging = false;
    for (int pass = 0; pass < 2; pass++) {
        for (const Feature& f : track.features) {
            if (f.type == FeatureType::EnergyPad) continue;
            float ahead = hazardAhead(f);
            if (ahead >= look || ahead > nearest + 40.0f) continue;
            float margin = f.halfWidth +
                           (f.type == FeatureType::Barrier ? kShipHalfWidth + 1.6f : 0.8f);
            // Gênant s'il est sur la ligne visée ou entre elle et le vaisseau.
            float lo = std::min(line, v.lateral), hi = std::max(line, v.lateral);
            if (hi <= f.lateral - margin || lo >= f.lateral + margin) continue;
            dodging = true;
            float left = f.lateral - margin, right = f.lateral + margin;
            bool leftOk = left >= -limit - 1.5f, rightOk = right <= limit + 1.5f;
            if (leftOk && rightOk)
                line = std::abs(left - v.lateral) < std::abs(right - v.lateral) ? left : right;
            else
                line = leftOk ? left : right;
        }
    }
    line = std::clamp(line, -limit - 1.5f, limit + 1.5f);
    float drift = -here.curvature * v.speed * v.speed * kDrift / stats.grip;
    // Devant un obstacle il braque franchement, sans arrondir sa trajectoire.
    float wanted = (line - v.lateral) * (dodging ? 5.0f : 1.6f) - drift;
    in.steer = std::clamp(wanted / steerSpeed, -1.0f, 1.0f);

    // Décisions prises par tranches d'une demi-seconde pour les tirages.
    uint32_t slot = world.tick / 30u;
    bool whim = profile.erratic > 0.0f && chance(index, slot, 1u) < profile.erratic;

    bool straight = std::abs(here.curvature) < 0.0006f && std::abs(ahead.curvature) < 0.0008f;
    in.boost = straight && (v.boosting ? v.energy > profile.boostStop : v.energy > profile.boostStart);
    if (whim && v.energy > 0.3f) in.boost = chance(index, slot, 2u) < 0.5f;

    // Sens du rythme : selon son talent, il attend le temps pour lancer un
    // boost, et place des accords même sans vouloir accélérer longtemps.
    const float beatTime = songTime(world, track, v);
    const bool onBeat = track.onBeat(beatTime, kBeatTolerance * 0.6f);
    // Il bat la mesure par plages de six secondes : poser une cadence prend
    // deux frappes, des plages trop courtes ne rapporteraient rien.
    const bool inRhythm = chance(index, static_cast<uint32_t>(beatTime / 6.0f) + 7919u, 3u) < profile.rhythm;
    if (inRhythm && v.stunTime <= 0.0f) {
        // Il bat la mesure : une frappe par temps de la grille, rien entre.
        // L'accord tient le boost, inutile de payer en énergie.
        in.boost = onBeat;
    }

    // Parade : une décharge arrive par-derrière. S'il a du rythme, il place
    // un accord sur le temps pour l'absorber.
    if (onBeat && v.stunTime <= 0.0f && chance(index, static_cast<uint32_t>(beatTime * 2.0f) + 31u, 5u) < profile.rhythm) {
        for (const Missile& m : world.missiles) {
            if (!m.active || m.owner == index) continue;
            const float behind = v.dist - m.dist;
            if (behind > 0.0f && behind < 140.0f) in.boost = true;
        }
    }
    return in;
}

int rankOf(const World& world, int index) {
    const Vehicle& me = world.vehicles[index];
    int rank = 1;
    for (int i = 0; i < world.count; i++) {
        if (i == index) continue;
        const Vehicle& other = world.vehicles[i];
        bool ahead;
        // Une fois la ligne franchie, c'est le temps d'arrivée qui classe.
        if (me.finished && other.finished) ahead = other.finishTime < me.finishTime;
        else if (me.finished != other.finished) ahead = other.finished;
        else ahead = other.dist > me.dist;
        if (ahead) rank++;
    }
    return rank;
}

float songTime(const World& world, const Track& track, const Vehicle& vehicle) {
    return world.ownClock ? track.at(vehicle.dist).time : world.raceTime;
}

float gapOf(const World& world, const Track& track, const Vehicle& vehicle) {
    return track.at(vehicle.dist).time - world.raceTime;
}

} // namespace sim

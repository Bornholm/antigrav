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
// à v.dist. Un bouclier levé traverse les champs et encaisse une barrière.
void applyFeatures(Vehicle& v, const ShipStats& stats, const Track& track, float before,
                   float pace, float dt) {

    float moved = v.dist - before;
    bool shielded = v.shieldTime > 0.0f;
    for (const Feature& f : track.features) {
        if (f.type == FeatureType::Barrier) {
            // À cette vitesse un pas dépasse l'épaisseur de la barrière : on
            // teste le franchissement, pas la présence.
            if (track.ahead(before, f.dist) >= moved) continue;
            if (std::abs(v.lateral - f.lateral) > f.halfWidth + kShipHalfWidth * 0.8f) continue;
            if (shielded) {
                v.shieldTime = 0.0f; // le bouclier se brise à sa place
                shielded = false;
                continue;
            }
            v.speed *= 1.0f - kBarrierLoss / stats.sturdy;
            v.stunTime = std::max(v.stunTime, kBarrierStun / stats.sturdy);
            v.energy = std::max(0.0f, v.energy - kBarrierEnergy);
            continue;
        }
        if (track.ahead(f.dist, v.dist) > f.length) continue;
        if (std::abs(v.lateral - f.lateral) > f.halfWidth) continue;
        if (f.type == FeatureType::BoostPad) {
            // À l'entrée sur la plaque, un peu d'énergie.
            if (before < f.dist) v.energy = std::min(1.0f, v.energy + kPadEnergy);
            float top = kPadSpeed * pace * stats.topSpeed;
            if (v.speed < top) v.speed = std::min(top, v.speed + kPadAccel * dt);
        } else if (!shielded) {
            v.speed -= v.speed * kSlowDrag * dt;
            v.energy = std::max(0.0f, v.energy - kSlowDrain * dt);
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

    // Boost lancé sur un temps : un accord. Il ne coûte rien pendant un
    // instant, pousse plus fort et rend un peu d'énergie.
    const bool pressed = in.boost && !v.boostHeld;
    v.boostHeld = in.boost;
    v.accord = std::max(0.0f, v.accord - dt);
    v.accordCooldown = std::max(0.0f, v.accordCooldown - dt);
    if (pressed && !stunned && v.accordCooldown <= 0.0f &&
        track.onBeat(clock.own ? f.time : clock.raceTime, kBeatTolerance)) {
        v.accord = kAccordTime;
        v.accordCooldown = kAccordCooldown;
        v.energy = std::min(1.0f, v.energy + kAccordEnergy);
    }
    if (stunned || !in.boost) v.accord = 0.0f;
    const bool accord = v.accord > 0.0f;

    v.boosting = in.boost && (v.energy > 0.0f || accord) && !stunned;
    // Les vitesses suivent celle du point de synchronisation : un vaisseau
    // en croisière avance avec la musique.
    const float pace = f.pace / kCruiseSpeed * band;
    float cruise = f.pace * band * stats.topSpeed;
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
        // Le bouclier protège aussi des murets.
        if (v.shieldTime <= 0.0f) v.energy -= 0.12f * dt;
        v.scraping = true;
    }
    v.energy = std::clamp(v.energy, 0.0f, 1.0f);

    // À l'intérieur d'un virage, la même vitesse couvre plus de piste.
    float pathScale = 1.0f / std::max(0.5f, 1.0f - f.curvature * v.lateral);
    float before = v.dist;
    v.dist += v.speed * pathScale * dt;
    applyFeatures(v, stats, track, before, pace, dt);

    float rollTarget = -steer * 0.38f - f.curvature * v.speed * 0.25f;
    v.roll += (rollTarget - v.roll) * std::min(1.0f, 7.0f * dt);
}

// Appuis sur missile et bouclier. Seul le front montant compte : rester appuyé
// ne tire pas en rafale.
void useItems(World& world, int index, const Input& in) {
    Vehicle& v = world.vehicles[index];
    bool canAct = !v.finished && v.stunTime <= 0.0f;

    if (in.fire && !v.fireHeld && canAct && v.energy >= kMissileCost) {
        for (Missile& m : world.missiles) {
            if (m.active) continue;
            m = Missile{};
            m.active = true;
            m.owner = index;
            m.dist = v.dist + 7.0f;
            m.lateral = v.lateral;
            m.speed = v.speed + 170.0f;
            m.life = 3.5f;
            v.energy -= kMissileCost;
            break;
        }
    }
    if (in.shield && !v.shieldHeld && canAct && v.shieldTime <= 0.0f &&
        v.energy >= kShieldCost) {
        v.shieldTime = kShieldDuration;
        v.energy -= kShieldCost;
    }
    v.fireHeld = in.fire;
    v.shieldHeld = in.shield;
}

void stepMissiles(World& world, const Track& track, float dt) {
    const float limit = track.halfWidth - 1.0f;
    for (Missile& m : world.missiles) {
        if (!m.active) continue;

        // Cible : le vaisseau le plus proche devant, autre que le tireur.
        const Vehicle* target = nullptr;
        float best = 600.0f;
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
            // Un bouclier levé absorbe le missile et en récupère l'énergie : se
            // défendre au bon moment rapporte plus que tirer à l'aveugle.
            if (v.shieldTime > 0.0f) v.energy = std::min(1.0f, v.energy + kAbsorbGain);
            if (v.shieldTime <= 0.0f) {
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
    return track.at(vehicle.dist).pace * shipStats(vehicle.ship).topSpeed;
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
        useItems(world, i, inputs[i]);
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
    bool targetInSight = false;
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
        if (along > 25.0f && along < profile.fireRange && std::abs(across) < 5.0f &&
            other.stunTime <= 0.0f && other.shieldTime <= 0.0f)
            targetInSight = true;
    }
    line = std::clamp(line, -limit, limit);

    float steerSpeed = kSteerSpeed * stats.handling;
    // Éléments de piste : viser la plaque de boost atteignable la plus proche,
    // puis s'écarter des barrières et des champs, qui ont le dernier mot.
    const float look = v.speed * profile.foresight;
    float padAhead = look;
    for (const Feature& f : track.features) {
        if (f.type != FeatureType::BoostPad) continue;
        float ahead = track.ahead(v.dist, f.dist);
        if (ahead >= padAhead) continue;
        float reach = steerSpeed * 0.8f * ahead / std::max(v.speed, 1.0f) + f.halfWidth * 0.5f;
        if (std::abs(f.lateral - v.lateral) > reach) continue;
        // Pas de plaque si la vitesse qu'elle donne envoie dans le muret : la
        // dérive croît avec le carré de la vitesse.
        float padDrift = std::abs(track.at(f.dist + 150.0f).curvature) * kPadSpeed * kPadSpeed *
                         kDrift / stats.grip;
        if (padDrift > steerSpeed * profile.nerve) continue;
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
        if (f.type != FeatureType::BoostPad) nearest = std::min(nearest, hazardAhead(f));
    bool dodging = false;
    for (int pass = 0; pass < 2; pass++) {
        for (const Feature& f : track.features) {
            if (f.type == FeatureType::BoostPad) continue;
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
    const bool inRhythm = chance(index, static_cast<uint32_t>(beatTime * 0.5f) + 7919u, 3u) < profile.rhythm;
    if (v.accord > 0.0f) {
        in.boost = true; // tient l'accord jusqu'au bout
    } else if (inRhythm && v.stunTime <= 0.0f) {
        if (in.boost && !v.boostHeld && !onBeat) in.boost = false; // attend le temps
        if (!in.boost && onBeat && v.accordCooldown <= 0.0f && straight) in.boost = true;
    }

    // Un appui bref : la simulation ne tire que sur le front montant.
    bool pulse = world.tick % 30u == static_cast<uint32_t>(index) % 30u;
    bool wantsFire = targetInSight && v.energy >= profile.fireEnergy;
    if (whim && v.energy > kMissileCost + 0.05f && chance(index, slot, 3u) < 0.25f) wantsFire = true;
    in.fire = wantsFire && pulse;

    if (profile.shieldRange > 0.0f) {
        for (const Missile& m : world.missiles) {
            if (!m.active || m.owner == index) continue;
            float behind = v.dist - m.dist;
            if (behind > 0.0f && behind < profile.shieldRange && std::abs(m.lateral - v.lateral) < 7.0f)
                in.shield = true;
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

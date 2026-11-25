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
struct Clock {
    float raceTime;
    bool own;
};

// Choc subi par un vaisseau : tir, mine, onde ou barrière. Un bouclier
// l'encaisse et s'éteint. Renvoie vrai si le vaisseau a vraiment été touché.
bool hit(Vehicle& v, float loss, float stun) {
    if (v.shieldTime > 0.0f) {
        v.shieldTime = 0.0f;
        return false;
    }
    const ShipStats& stats = shipStats(v.ship);
    v.speed *= 1.0f - loss / stats.sturdy;
    v.stunTime = std::max(v.stunTime, stun / stats.sturdy);
    v.turboTime = 0.0f;
    return true;
}

// Effet des éléments de piste sur un vaisseau qui vient d'avancer de `before`
// à v.dist.
void applyFeatures(Vehicle& v, const Track& track, float before) {
    float moved = v.dist - before;
    const bool flying = v.airTime > 0.0f;
    for (const Feature& f : track.features) {
        if (f.type == FeatureType::Jump) {
            if (before < f.dist && v.dist >= f.dist) v.airTime = kAirTime;
            continue;
        }
        if (flying) continue; // en l'air, on survole tout
        if (f.type == FeatureType::Barrier) {
            // À cette vitesse un pas dépasse l'épaisseur de la barrière : on
            // teste le franchissement, pas la présence.
            if (track.ahead(before, f.dist) >= moved) continue;
            if (std::abs(v.lateral - f.lateral) > f.halfWidth + kHitRadius) continue;
            hit(v, kBarrierLoss, kBarrierStun);
            continue;
        }
        // Boîte à bonus : il suffit de passer dessus les mains vides.
        if (before >= f.dist || track.ahead(f.dist, v.dist) > f.length) continue;
        if (std::abs(v.lateral - f.lateral) > f.halfWidth + kShipHalfWidth * 0.5f) continue;
        if (v.item == Item::None) v.boxHit = true;
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
    v.airTime = std::max(0.0f, v.airTime - dt);
    if (v.airTime > 0.0f) steer *= 0.45f; // en l'air, on corrige à peine

    // Horloge commune : un élastique retient qui prend de l'avance sur la
    // musique et aspire qui prend du retard. Tout le peloton reste dans la
    // même phrase.
    float band = 1.0f;
    if (!clock.own) {
        const float gap = f.time - clock.raceTime;
        band = gap > 0.0f ? std::max(0.75f, 1.0f - kLeadDrag * gap / kWindow)
                          : std::min(1.25f, 1.0f - kDraft * gap / kWindow);
    }

    // Les vitesses suivent celle du point de synchronisation : un vaisseau
    // en croisière avance avec la musique.
    v.turboTime = std::max(0.0f, v.turboTime - dt);
    v.boosting = v.turboTime > 0.0f && !stunned;
    const float base = f.pace / kSyncEdge * band;
    float cruise = base * stats.topSpeed;
    float target = v.boosting ? cruise * kTurboSpeed : cruise;
    if (stunned) target = cruise * 0.55f;
    float delta = target - v.speed;
    float gain = (v.boosting ? kBoostAccel : kAccel) * stats.accel;
    v.speed += std::clamp(delta, -(stunned ? 4.0f : 1.0f) * kDecel * dt, gain * dt);

    // Braquage, plus la dérive vers l'extérieur du virage.
    float steerSpeed = kSteerSpeed * stats.handling;
    v.lateralVel += (steer * steerSpeed - v.lateralVel) * std::min(1.0f, kSteerResponse * dt);
    float drift = -f.curvature * v.speed * v.speed * kDrift / stats.grip;
    v.lateral += (v.lateralVel + drift) * dt;
    // Glisser coûte de la vitesse : un vaisseau qui accroche sort plus vite
    // des virages qu'un vaisseau rapide en ligne droite.
    v.speed -= std::abs(drift) * kScrub * dt;

    // Murets : rebond mou et perte de vitesse.
    float limit = track.halfWidth - kShipHalfWidth;
    v.scraping = false;
    if (std::abs(v.lateral) > limit) {
        float side = v.lateral > 0.0f ? 1.0f : -1.0f;
        v.lateral = side * limit;
        if (v.lateralVel * side > 0.0f) v.lateralVel *= -0.35f;
        v.speed -= v.speed * (0.55f / stats.sturdy) * dt;
        v.scraping = true;
    }

    // À l'intérieur d'un virage, la même vitesse couvre plus de piste.
    float pathScale = 1.0f / std::max(0.5f, 1.0f - f.curvature * v.lateral);
    float before = v.dist;
    v.dist += v.speed * pathScale * dt;
    applyFeatures(v, track, before);

    float rollTarget = -steer * 0.38f - f.curvature * v.speed * 0.25f;
    v.roll += (rollTarget - v.roll) * std::min(1.0f, 7.0f * dt);
}

// Tirage pseudo-aléatoire reproductible : dépend du pilote et de l'instant,
// jamais d'un état global, donc deux exécutions donnent la même course.
float chance(int index, uint32_t slot, uint32_t salt) {
    uint32_t h = static_cast<uint32_t>(index) * 0x9E3779B1u ^ slot * 0x85EBCA77u ^ salt * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return static_cast<float>(h & 0xFFFFFFu) / 16777216.0f;
}

// Bonus donné par une boîte. Le dernier reçoit de quoi remonter, tirs et
// turbos ; le premier, de quoi se défendre, mines et boucliers.
Item rollItem(const World& world, int index) {
    const float back = world.count > 1 ? static_cast<float>(rankOf(world, index) - 1) / static_cast<float>(world.count - 1) : 0.5f;
    const float weights[static_cast<int>(Item::Count)] = {
        0.0f,
        1.0f + 3.0f * back, // tir
        3.0f - 2.0f * back, // mine
        2.0f,               // onde
        1.0f + 3.0f * back, // turbo
        3.0f - 2.0f * back, // bouclier
        0.0f,               // comète : tirée à part
    };
    // Le dernier a une chance de recevoir la comète, qui rebat les cartes.
    if (world.count >= 3 && rankOf(world, index) == world.count && chance(index, world.tick, 78u) < kCometChance)
        return Item::Comet;
    float total = 0.0f;
    for (float w : weights) total += w;
    float pick = chance(index, world.tick, 77u) * total;
    for (int i = 1; i < static_cast<int>(Item::Comet); i++) {
        pick -= weights[i];
        if (pick <= 0.0f) return static_cast<Item>(i);
    }
    return Item::Shield;
}

// Appui sur le bouton : le bonus porté part. Seul le front montant compte.
void useItems(World& world, int index, const Input& in) {
    Vehicle& v = world.vehicles[index];
    // Boîte franchie au pas précédent : le bonus arrive ici, où l'on connaît
    // le classement.
    if (v.boxHit) {
        v.boxHit = false;
        if (v.item == Item::None) v.item = rollItem(world, index);
    }
    const bool pressed = in.use && !v.useHeld;
    v.useHeld = in.use;
    if (!pressed || v.item == Item::None || v.finished || v.stunTime > 0.0f) return;
    const Item item = v.item;
    v.item = Item::None;

    auto spawn = [&](MissileKind kind) {
        for (Missile& m : world.missiles) {
            if (m.active) continue;
            m = Missile{};
            m.active = true;
            m.kind = kind;
            m.owner = index;
            const bool mine = kind == MissileKind::Mine;
            m.dist = mine ? v.dist - 9.0f : v.dist + 7.0f;
            m.lateral = v.lateral;
            m.speed = mine ? 0.0f : v.speed + (kind == MissileKind::Comet ? kCometSpeed : 170.0f);
            m.life = mine ? kMineLife : kind == MissileKind::Comet ? kCometLife : 4.5f;
            return;
        }
    };
    switch (item) {
    case Item::Shot: spawn(MissileKind::Shot); break;
    case Item::Mine: spawn(MissileKind::Mine); break;
    case Item::Comet: spawn(MissileKind::Comet); break;
    case Item::Pulse:
        for (int i = 0; i < world.count; i++) {
            Vehicle& other = world.vehicles[i];
            if (i == index || other.finished) continue;
            if (std::abs(other.dist - v.dist) > kPulseRange || std::abs(other.lateral - v.lateral) > kPulseWidth) continue;
            hit(other, kHitLoss, kStunDuration);
        }
        break;
    case Item::Turbo: v.turboTime = kTurboTime; break;
    case Item::Shield: v.shieldTime = kShieldTime; break;
    default: break;
    }
}

void stepMissiles(World& world, const Track& track, float dt) {
    const float limit = track.halfWidth - 1.0f;
    for (Missile& m : world.missiles) {
        if (!m.active) continue;
        m.life -= dt;
        const float before = m.dist;
        if (m.kind != MissileKind::Mine) {
            // Cible : le vaisseau le plus proche devant, autre que le tireur.
            const Vehicle* target = nullptr;
            float best = m.kind == MissileKind::Comet ? 1e9f : 900.0f;
            for (int i = 0; i < world.count; i++) {
                if (i == m.owner) continue;
                float ahead = world.vehicles[i].dist - m.dist;
                if (ahead > -4.0f && ahead < best) {
                    best = ahead;
                    target = &world.vehicles[i];
                }
            }
            // La comète vire deux fois plus sec qu'un tir : on ne l'esquive guère.
            const float turn = m.kind == MissileKind::Comet ? 70.0f : 34.0f;
            if (target) {
                float delta = target->lateral - m.lateral;
                m.lateral += std::clamp(delta, -turn * dt, turn * dt);
            } else if (m.kind == MissileKind::Comet) {
                m.life = 0.0f; // plus personne devant : elle a fini sa remontée
            }
            m.lateral = std::clamp(m.lateral, -limit, limit);
            m.dist += m.speed * dt;
            // Une barrière arrête un tir : on peut s'abriter derrière. Pas la comète.
            if (m.kind == MissileKind::Shot)
                for (const Feature& f : track.features) {
                    if (f.type != FeatureType::Barrier) continue;
                    if (track.ahead(before, f.dist) < m.dist - before &&
                        std::abs(m.lateral - f.lateral) < f.halfWidth)
                        m.life = 0.0f;
                }
        }
        if (m.life <= 0.0f) {
            m.active = false;
            continue;
        }

        for (int i = 0; i < world.count; i++) {
            if (i == m.owner) continue; // celui qui l'a lancé ne craint rien
            Vehicle& v = world.vehicles[i];
            // En plein saut on passe au-dessus d'une mine.
            if (m.kind == MissileKind::Mine && v.airTime > 0.0f) continue;
            if (m.kind == MissileKind::Comet) {
                // Elle va plus vite qu'un pas ne couvre : on teste le dépassement.
                if (v.dist <= before - 6.5f || v.dist > m.dist + 6.5f) continue;
                if (std::abs(v.lateral - m.lateral) > 5.5f) continue;
                // Chacun n'est frappé qu'une fois, bouclier ou non.
                if (m.struck & (1u << i)) continue;
                m.struck = static_cast<uint8_t>(m.struck | (1u << i));
                hit(v, kHitLoss, kStunDuration);
                continue;
            }
            if (std::abs(v.dist - m.dist) > 6.5f || std::abs(v.lateral - m.lateral) > 3.8f)
                continue;
            m.active = false;
            hit(v, kHitLoss, kStunDuration);
            break;
        }
    }
}

} // namespace

void stepVehicle(Vehicle& vehicle, const Input& input, const Track& track, const World& world) {
    stepVehicleDt(vehicle, input, track, {world.raceTime, world.ownClock}, kFixedDt);
}

float airHeight(const Vehicle& vehicle) {
    if (vehicle.airTime <= 0.0f) return 0.0f;
    const float u = 1.0f - vehicle.airTime / kAirTime;
    return kJumpHeight * 4.0f * u * (1.0f - u);
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
    // Éléments de piste : les mains vides, viser la boîte à bonus atteignable
    // la plus proche. Puis s'écarter des barrières, qui ont le dernier mot.
    const float look = v.speed * profile.foresight;
    float padAhead = look;
    for (const Feature& f : track.features) {
        if (f.type != FeatureType::ItemBox || v.item != Item::None) continue;
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
        if (f.type == FeatureType::Barrier) nearest = std::min(nearest, hazardAhead(f));
    bool dodging = false;
    for (int pass = 0; pass < 2; pass++) {
        for (const Feature& f : track.features) {
            if (f.type != FeatureType::Barrier) continue;
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

    // Bonus : chacun a son moment. Les décisions sont prises par tranches,
    // et la personnalité règle l'empressement.
    if (v.item != Item::None && v.stunTime <= 0.0f) {
        bool wanted = false;
        const bool straight = std::abs(here.curvature) < 0.0006f && std::abs(ahead.curvature) < 0.0008f;
        for (int i = 0; i < world.count && !wanted; i++) {
            if (i == index) continue;
            const Vehicle& other = world.vehicles[i];
            const float along = other.dist - v.dist, across = std::abs(other.lateral - v.lateral);
            if (v.item == Item::Shot) wanted = along > 20.0f && along < 500.0f;
            if (v.item == Item::Comet) wanted = along > 20.0f;
            if (v.item == Item::Mine) wanted = along < -10.0f && along > -160.0f && across < 9.0f;
            if (v.item == Item::Pulse) wanted = std::abs(along) < kPulseRange * 0.7f && across < kPulseWidth * 0.8f;
        }
        if (v.item == Item::Turbo) wanted = straight;
        if (v.item == Item::Shield) {
            // Levé quand un tir arrive, ou devant une barrière qu'il ne pourra pas éviter.
            for (const Missile& m : world.missiles)
                wanted = wanted || (m.active && m.kind != MissileKind::Mine && m.owner != index && v.dist - m.dist > 0.0f && v.dist - m.dist < 220.0f);
            wanted = wanted || (dodging && nearest < v.speed * 0.25f);
        }
        if (whim && chance(index, slot, 6u) < 0.3f) wanted = true;
        // Un appui d'un seul pas : la simulation ne lit que le front montant.
        const bool moment = world.tick % 20u == static_cast<uint32_t>(index) % 20u;
        in.use = wanted && moment && chance(index, slot, 4u) < profile.trigger;
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

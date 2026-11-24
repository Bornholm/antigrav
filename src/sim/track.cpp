#include "sim/track.h"

#include "sim/content.h"
#include "sim/generate.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace sim {

namespace {

// Tirage reproductible, entre 0 et 1.
float featureRoll(uint32_t seed, int slot, uint32_t salt) {
    uint32_t h = (seed + 1u) * 0x9E3779B1u ^
                 static_cast<uint32_t>(slot) * 0x85EBCA77u ^ salt * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return static_cast<float>(h & 0xFFFFFFu) / 16777216.0f;
}


constexpr float kLead = 400.0f;  // piste avant la ligne de départ
constexpr float kTail = 1600.0f; // piste après l'arrivée, pour finir sur sa lancée
constexpr float kStep = 4.0f;
constexpr float kPi = 3.14159265f;

// Énergie moyenne du morceau autour d'un instant.
float smoothEnergy(const music::Partition& p, float time, float half) {
    float sum = 0.0f;
    int count = 0;
    for (float t = time - half; t <= time + half; t += 0.25f, count++) sum += p.energyAt(t);
    return count ? sum / static_cast<float>(count) : 0.0f;
}

// Hargne moyenne du morceau autour d'un instant.
float smoothDrive(const music::Partition& p, float time, float half) {
    float sum = 0.0f;
    int count = 0;
    for (float t = time - half; t <= time + half; t += 0.25f, count++) sum += p.driveAt(t);
    return count ? sum / static_cast<float>(count) : 0.0f;
}

// Vitesse du point de synchronisation : elle suit l'énergie du morceau, et un
// morceau hargneux va plus vite qu'un morceau doux de même énergie.
float paceFor(float energy, float drive) { return 270.0f + 120.0f * energy + 30.0f * drive; }

struct Phrase {
    float start, end;
    float amplitude; // courbure maximale, signée
};

// Un virage par phrase de huit temps, ou de quatre quand le morceau est
// hargneux : la piste se met alors à secouer. Elle serpente autour d'un axe,
// le cap ne dépasse jamais un quart de tour, donc elle ne se recoupe pas.
std::vector<Phrase> makePhrases(const music::Partition& p, const std::vector<music::Beat>& beats,
                                float duration, uint32_t seed) {
    std::vector<float> cuts{0.0f};
    for (size_t i = 0; i < beats.size(); i++) {
        if (beats[i].bar != 0) continue;
        for (size_t k = i; k < beats.size(); k += 4) {
            const bool whole = (k - i) % 8 == 0;
            if (!whole && smoothDrive(p, beats[k].time, 1.5f) < 0.7f) continue;
            if (beats[k].time > cuts.back() + 1.0f && beats[k].time < duration - 1.0f)
                cuts.push_back(beats[k].time);
        }
        break;
    }
    cuts.push_back(duration);

    std::vector<Phrase> phrases;
    float heading = 0.0f, lastSign = 1.0f;
    for (size_t i = 0; i + 1 < cuts.size(); i++) {
        const float start = cuts[i], end = cuts[i + 1];
        const float energy = smoothEnergy(p, 0.5f * (start + end), 0.5f * (end - start));
        const float drive = smoothDrive(p, 0.5f * (start + end), 0.5f * (end - start));
        const float length = paceFor(energy, drive) * (end - start);
        const int slot = static_cast<int>(i);
        // Un passage intense tourne serré, un passage calme file presque droit.
        float amplitude = (0.0008f + 0.0024f * energy) * (1.0f + 0.5f * drive) * (0.6f + 0.4f * featureRoll(seed, slot, 11u));
        if (energy < 0.25f && featureRoll(seed, slot, 12u) < 0.5f) amplitude *= 0.3f;
        // Le virage occupe les six dixièmes du milieu de la phrase et tourne de
        // amplitude * longueur * 0,3 : au plus 0,9 radian.
        amplitude = std::min(amplitude, 3.0f / std::max(1.0f, length));
        const float turn = 0.3f * amplitude * length;
        float sign = -lastSign;
        if (std::abs(heading) > 0.5f) sign = heading > 0.0f ? -1.0f : 1.0f;
        else if (featureRoll(seed, slot, 13u) < 0.25f && std::abs(heading + lastSign * turn) < 1.2f) sign = lastSign;
        heading += sign * turn;
        lastSign = sign;
        phrases.push_back({start, end, sign * amplitude});
    }
    return phrases;
}

// Repères, courbure et dévers à partir des positions.
void finishFrames(Track& track) {
    const int count = static_cast<int>(track.frames.size());
    auto clampIndex = [&](int i) { return std::clamp(i, 0, count - 1); };
    for (int i = 0; i < count; i++) {
        const glm::vec3& prev = track.frames[clampIndex(i - 1)].pos;
        const glm::vec3& next = track.frames[clampIndex(i + 1)].pos;
        track.frames[i].tangent = glm::normalize(next - prev);
    }

    // Dévers proportionnel à la courbure, lissé pour éviter les à-coups.
    std::vector<float> bank(count);
    for (int i = 0; i < count; i++) {
        const glm::vec3& t0 = track.frames[clampIndex(i - 1)].tangent;
        const glm::vec3& t1 = track.frames[clampIndex(i + 1)].tangent;
        glm::vec3 flatRight =
            glm::normalize(glm::cross(track.frames[i].tangent, glm::vec3(0.0f, 1.0f, 0.0f)));
        float curvature = glm::dot(t1 - t0, flatRight) / (2.0f * track.step);
        track.frames[i].curvature = curvature;
        bank[i] = std::clamp(curvature * 170.0f, -0.42f, 0.42f);
    }
    std::vector<float> smoothed(count);
    for (int pass = 0; pass < 12; pass++) {
        for (int i = 0; i < count; i++)
            smoothed[i] = (bank[clampIndex(i - 1)] + 2.0f * bank[i] + bank[clampIndex(i + 1)]) * 0.25f;
        bank.swap(smoothed);
    }
    // La courbure brute est bruitée par l'échantillonnage : même lissage.
    for (int pass = 0; pass < 6; pass++) {
        for (int i = 0; i < count; i++)
            smoothed[i] = (track.frames[clampIndex(i - 1)].curvature + 2.0f * track.frames[i].curvature +
                           track.frames[clampIndex(i + 1)].curvature) * 0.25f;
        for (int i = 0; i < count; i++) track.frames[i].curvature = smoothed[i];
    }

    for (int i = 0; i < count; i++) {
        TrackFrame& f = track.frames[i];
        glm::vec3 flatRight = glm::normalize(glm::cross(f.tangent, glm::vec3(0.0f, 1.0f, 0.0f)));
        glm::vec3 flatUp = glm::cross(flatRight, f.tangent);
        // Dans un virage à droite, la normale penche vers l'intérieur.
        f.up = glm::normalize(flatUp * std::cos(bank[i]) + flatRight * std::sin(bank[i]));
        f.right = glm::normalize(glm::cross(f.tangent, f.up));
    }
}

// Pose les éléments sur la musique. Une plaque d'énergie tombe sur le premier
// temps de la mesure. Un obstacle tombe sur le deuxième ou le quatrième
// temps, celui que le morceau accentue le plus. Un passage reste
// toujours libre, et la plaque de la mesure est dans ce passage.
void placeFeatures(Track& track, const music::Partition& p, const std::vector<music::Beat>& beats,
                   uint32_t seed) {
    const float hw = track.halfWidth;
    const float kPadLength = 36.0f, kPadHalf = 3.6f;
    const float kBarrierLength = 3.0f;
    const float kGapHalf = 6.5f; // demi-largeur du passage entre deux barrières

    auto add = [&](FeatureType type, float dist, float length, float lateral, float half) {
        if (dist < 260.0f || dist + length > track.length - 120.0f) return;
        track.features.push_back({type, dist, length, lateral, half});
    };
    // Force de l'accent le plus marqué autour d'un instant, hors graves.
    auto accent = [&](float time) {
        float best = 0.0f;
        for (const music::Onset& o : p.onsets)
            if (o.band != music::kLow && std::abs(o.time - time) < 0.07f) best = std::max(best, o.strength);
        return best;
    };

    // Barrière ou groupe de barrières à cette distance. Renvoie le couloir
    // resté libre le plus sûr pour une plaque.
    auto hazard = [&](float dist, int slot, uint32_t salt, float padLane) {
        const float lanes[3] = {-0.55f * hw, 0.0f, 0.55f * hw};
        const float curvature = track.at(dist + 20.0f).curvature;
        const float inside = curvature > 0.0f ? 1.0f : -1.0f;
        const float kind = featureRoll(seed, slot, salt);
        if (std::abs(curvature) > 0.0012f) {
            // En virage, une barrière ferme la corde : il faut élargir.
            const float half = hw * 0.36f;
            add(FeatureType::Barrier, dist, kBarrierLength, inside * (hw - half), half);
            return -inside * 0.5f * hw;
        }
        if (kind < 0.34f) {
            // Chicane : deux barrières, un passage au milieu.
            const float half = (hw - kGapHalf) * 0.5f;
            add(FeatureType::Barrier, dist, kBarrierLength, -(hw - half), half);
            add(FeatureType::Barrier, dist, kBarrierLength, hw - half, half);
            return 0.0f;
        }
        if (kind < 0.67f) {
            // Plot central, on passe d'un côté ou de l'autre.
            add(FeatureType::Barrier, dist, kBarrierLength, 0.0f, hw * 0.28f);
            return padLane == 0.0f ? (featureRoll(seed, slot, salt + 1u) < 0.5f ? lanes[0] : lanes[2]) : padLane;
        }
        // Demi-piste fermée.
        const float side = featureRoll(seed, slot, salt + 1u) < 0.5f ? -1.0f : 1.0f;
        const float half = hw * 0.42f;
        add(FeatureType::Barrier, dist, kBarrierLength, side * (hw - half), half);
        return -side * 0.5f * hw;
    };

    const float kHazardGap = 2.0f; // secondes
    float lastHazard = -100.0f;
    int bar = 0;
    for (size_t i = 0; i + 3 < beats.size(); i++) {
        if (beats[i].bar != 0) continue;
        const int slot = bar++;
        const float energy = smoothEnergy(p, beats[i].time, 1.0f);
        const float drive = smoothDrive(p, beats[i].time, 1.0f);
        const float lanes[3] = {-0.55f * hw, 0.0f, 0.55f * hw};
        float padLane = lanes[std::min(2, static_cast<int>(featureRoll(seed, slot, 2u) * 3.0f))];
        const float turn = track.at(track.distAt(beats[i].time) + 20.0f).curvature;
        if (std::abs(turn) > 0.0012f) padLane = turn > 0.0f ? -0.5f * hw : 0.5f * hw; // la plaque récompense l'extérieur

        const float accent2 = accent(beats[i + 1].time), accent4 = accent(beats[i + 3].time);
        // Les passages calmes n'ont presque pas d'obstacles, les passages
        // hargneux en ont un peu plus.
        const float chance = (0.15f + 0.35f * energy + 0.20f * std::max(accent2, accent4) + 0.15f * drive) *
                             (energy < 0.25f ? 0.3f : 1.0f);
        // Sur le deuxième temps l'obstacle suit la plaque d'un seul temps :
        // réservé aux passages intenses.
        const bool early = accent2 > accent4 && energy >= 0.6f;
        // Jamais deux obstacles à moins de deux secondes l'un de l'autre :
        // sur un tempo rapide les mesures défilent vite, et la piste
        // deviendrait un mur.
        const float hazardTime = beats[i + (early ? 1 : 3)].time;
        if (hazardTime - lastHazard >= kHazardGap && featureRoll(seed, slot, 1u) < chance) {
            padLane = hazard(track.distAt(hazardTime), slot, 3u, padLane);
            lastHazard = hazardTime;
        }
        // Passages calmes : une plaque une mesure sur deux. Passages hargneux :
        // une seconde plaque sur le troisième temps.
        if (energy >= 0.35f || slot % 2 == 0)
            add(FeatureType::EnergyPad, track.distAt(beats[i].time) - 0.5f * kPadLength, kPadLength, padLane, kPadHalf);
        if (drive > 0.7f)
            add(FeatureType::EnergyPad, track.distAt(beats[i + 2].time) - 0.5f * kPadLength, kPadLength,
                -padLane, kPadHalf);
    }

    // Règle générale : une barrière ne suit jamais une plaque. Qui vient de
    // prendre une plaque doit pouvoir continuer tout droit sans rien heurter
    // pendant une seconde et demie. Les barrières en faute sont retirées.
    std::vector<Feature> kept;
    for (const Feature& f : track.features) {
        bool blocked = false;
        if (f.type == FeatureType::Barrier) {
            for (const Feature& pad : track.features) {
                if (pad.type != FeatureType::EnergyPad) continue;
                const float after = f.dist - (pad.dist + pad.length);
                if (after < 0.0f || after > track.at(pad.dist).pace * kClearAfterPad) continue;
                if (std::abs(f.lateral - pad.lateral) < f.halfWidth + kClearHalfWidth) blocked = true;
            }
        }
        if (!blocked) kept.push_back(f);
    }
    track.features.swap(kept);
}

} // namespace

Track generateTrack(const music::Partition& p, float halfWidth, uint32_t seed) {
    const float duration = std::max(5.0f, p.duration);
    // Sans tempo fiable, une grille régulière à 120 battements par minute
    // tient lieu de temps.
    std::vector<music::Beat> beats = p.beats;
    if (beats.size() < 16 || p.confidence < 0.1f) {
        beats.clear();
        int index = 0;
        for (float t = 0.5f; t < duration; t += 0.5f, index++) beats.push_back({t, 0.5f, index % 4});
    }
    const std::vector<Phrase> phrases = makePhrases(p, beats, duration, seed);

    float meanEnergy = 0.0f;
    for (float e : p.energy) meanEnergy += e;
    if (!p.energy.empty()) meanEnergy /= static_cast<float>(p.energy.size());

    // Tracé dense, un point tous les centièmes de seconde du morceau, précédé
    // d'une ligne droite et suivi d'une autre.
    struct Sample {
        glm::vec3 pos;
        float time, pace, dist;
    };
    std::vector<Sample> dense;
    auto paceAt = [&](float time) { return paceFor(smoothEnergy(p, time, 1.0f), smoothDrive(p, time, 1.0f)); };
    const float startPace = paceAt(0.0f);
    // La piste part vers -z : la ligne droite de départ est donc en z positif.
    for (float d = -kLead; d < 0.0f; d += kStep)
        dense.push_back({{0.0f, 0.0f, -d}, d / startPace, startPace, d});

    const float dt = 0.01f;
    glm::vec3 pos{0.0f};
    float heading = 0.0f, height = 0.0f, slope = 0.0f, dist = 0.0f, pace = startPace;
    size_t phrase = 0;
    float slowEnergy = 0.0f, fastPace = startPace;
    for (int i = 0; static_cast<float>(i) * dt < duration; i++) {
        const float time = static_cast<float>(i) * dt;
        // L'énergie ne bouge qu'au dixième de seconde : inutile de la relire plus.
        if (i % 10 == 0) {
            fastPace = paceAt(time);
            slowEnergy = smoothEnergy(p, time, 2.5f);
        }
        pace += (fastPace - pace) * 0.02f;
        dense.push_back({pos, time, pace, dist});

        while (phrase + 1 < phrases.size() && time >= phrases[phrase].end) phrase++;
        const Phrase& ph = phrases[phrase];
        const float u = std::clamp((time - ph.start) / std::max(0.01f, ph.end - ph.start), 0.0f, 1.0f);
        const float s = std::sin(kPi * std::clamp((u - 0.2f) / 0.6f, 0.0f, 1.0f));
        const float moved = pace * dt;
        heading += ph.amplitude * s * s * moved;
        // La piste plonge dans les passages intenses et remonte dans les
        // passages calmes.
        const float target = -560.0f * (slowEnergy - meanEnergy);
        const float wanted = std::clamp((target - height) / 320.0f, -0.22f, 0.22f);
        slope += (wanted - slope) * std::min(1.0f, moved / 150.0f);
        const glm::vec3 dir = glm::normalize(glm::vec3(std::sin(heading), slope, -std::cos(heading)));
        pos += dir * moved;
        height = pos.y;
        dist += moved;
    }
    const float length = dist;
    const float endTime = duration;
    for (float d = 0.0f; d <= kTail; d += kStep) {
        dense.push_back({pos, endTime + d / pace, pace, length + d});
        slope *= 0.9f;
        pos += glm::normalize(glm::vec3(std::sin(heading), slope, -std::cos(heading))) * kStep;
    }

    Track track;
    track.halfWidth = halfWidth;
    track.lead = kLead;
    track.length = length;
    track.duration = duration;
    const float total = kLead + length + kTail;
    const int count = static_cast<int>(total / kStep);
    track.step = kStep;
    track.frames.resize(count);
    size_t seg = 0;
    for (int i = 0; i < count; i++) {
        const float d = track.distOf(i);
        while (seg + 2 < dense.size() && dense[seg + 1].dist < d) seg++;
        const Sample& a = dense[seg];
        const Sample& b = dense[seg + 1];
        const float span = b.dist - a.dist;
        const float t = span > 0.0f ? std::clamp((d - a.dist) / span, 0.0f, 1.0f) : 0.0f;
        track.frames[i].pos = glm::mix(a.pos, b.pos, t);
        track.frames[i].time = glm::mix(a.time, b.time, t);
        track.frames[i].pace = glm::mix(a.pace, b.pace, t);
    }
    finishFrames(track);
    placeFeatures(track, p, beats, seed);
    for (const music::Beat& b : beats) {
        track.beats.push_back(b.time);
        track.downbeats.push_back(b.bar == 0);
    }
    return track;
}

music::Partition metronome(float bpm, float seconds) {
    music::Partition p;
    p.duration = seconds;
    p.tempo = bpm;
    p.confidence = 1.0f;
    const float beat = 60.0f / bpm;
    // Quatre parties de huit mesures, qui reviennent : calme, forte, pont, forte.
    static const float levels[4] = {0.3f, 0.8f, 0.15f, 0.95f};
    const float part = beat * 32.0f;
    int index = 0;
    for (float t = 0.25f; t < seconds; t += beat, index++) {
        const int bar = index % 4;
        p.beats.push_back({t, bar == 0 ? 1.0f : 0.6f, bar});
        p.onsets.push_back({t, bar == 0 ? 1.0f : 0.7f, music::kLow});
        if (bar == 1 || bar == 3) p.onsets.push_back({t, bar == 3 ? 0.9f : 0.7f, music::kHigh});
    }
    for (float t = 0.0f; t <= seconds; t += 1.0f / music::Partition::kEnergyRate)
        p.energy.push_back(levels[static_cast<int>(t / part) % 4]);
    for (float t = 0.0f; t < seconds; t += part) {
        const float level = levels[static_cast<int>(t / part) % 4];
        p.sections.push_back({t, std::min(seconds, t + part), level, 1.5f, 0.5f});
    }
    return p;
}

Track Track::make(int id) {
    const music::Partition& p = song(id);
    uint32_t seed = static_cast<uint32_t>(id);
    // Le morceau du joueur tire sa graine de sa partition : deux morceaux
    // différents ne donnent pas les mêmes tirages.
    if (id == kCustomTrack)
        seed = 1000u + static_cast<uint32_t>(p.duration * 100.0f) * 31u + static_cast<uint32_t>(p.beats.size()) * 7919u +
               static_cast<uint32_t>(p.tempo * 10.0f);
    return generateTrack(p, trackInfo(id).halfWidth, seed);
}

TrackFrame Track::at(float distance) const {
    const int count = static_cast<int>(frames.size());
    const float fi = (distance + lead) / step;
    if (fi <= 0.0f || fi >= static_cast<float>(count - 1)) {
        // Hors de la piste construite : prolongement en ligne droite.
        TrackFrame out = fi <= 0.0f ? frames.front() : frames.back();
        const float beyond = fi <= 0.0f ? distance + lead : distance - distOf(count - 1);
        out.pos += out.tangent * beyond;
        out.time += beyond / out.pace;
        out.curvature = 0.0f;
        return out;
    }
    const int i0 = static_cast<int>(fi);
    const float t = fi - static_cast<float>(i0);
    const TrackFrame& a = frames[i0];
    const TrackFrame& b = frames[i0 + 1];
    TrackFrame out;
    out.pos = glm::mix(a.pos, b.pos, t);
    out.tangent = glm::normalize(glm::mix(a.tangent, b.tangent, t));
    out.up = glm::normalize(glm::mix(a.up, b.up, t));
    out.curvature = glm::mix(a.curvature, b.curvature, t);
    out.pace = glm::mix(a.pace, b.pace, t);
    out.time = glm::mix(a.time, b.time, t);
    out.right = glm::normalize(glm::cross(out.tangent, out.up));
    out.up = glm::cross(out.right, out.tangent);
    return out;
}

glm::vec3 Track::point(float distance, float lateral, float height) const {
    TrackFrame f = at(distance);
    return f.pos + f.right * lateral + f.up * height;
}

float Track::ahead(float from, float to) const { return to >= from ? to - from : 1e9f; }

bool Track::onBeat(float time, float tolerance) const {
    auto next = std::lower_bound(beats.begin(), beats.end(), time);
    if (next != beats.end() && *next - time <= tolerance) return true;
    return next != beats.begin() && time - *(next - 1) <= tolerance;
}

float Track::distAt(float time) const {
    const int count = static_cast<int>(frames.size());
    if (time <= frames.front().time) return distOf(0) + (time - frames.front().time) * frames.front().pace;
    if (time >= frames.back().time)
        return distOf(count - 1) + (time - frames.back().time) * frames.back().pace;
    int lo = 0, hi = count - 1;
    while (hi - lo > 1) {
        const int mid = (lo + hi) / 2;
        (frames[mid].time <= time ? lo : hi) = mid;
    }
    const float span = frames[hi].time - frames[lo].time;
    const float t = span > 0.0f ? (time - frames[lo].time) / span : 0.0f;
    return distOf(lo) + t * step;
}

} // namespace sim

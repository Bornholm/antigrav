#include "sim/track.h"

#include "sim/content.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace sim {

namespace {

glm::vec3 catmullRom(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
                     const glm::vec3& p3, float t) {
    float t2 = t * t, t3 = t2 * t;
    return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                   (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

Track build(const std::vector<glm::vec3>& control, float halfWidth, float targetStep) {
    const int n = static_cast<int>(control.size());

    // Polyligne dense, puis rééchantillonnage à pas constant.
    std::vector<glm::vec3> dense;
    const int perSpan = 96;
    for (int i = 0; i < n; i++) {
        const glm::vec3& p0 = control[(i + n - 1) % n];
        const glm::vec3& p1 = control[i];
        const glm::vec3& p2 = control[(i + 1) % n];
        const glm::vec3& p3 = control[(i + 2) % n];
        for (int k = 0; k < perSpan; k++)
            dense.push_back(catmullRom(p0, p1, p2, p3, static_cast<float>(k) / perSpan));
    }
    const int dn = static_cast<int>(dense.size());
    std::vector<float> cumulative(dn + 1, 0.0f);
    for (int i = 0; i < dn; i++)
        cumulative[i + 1] = cumulative[i] + glm::length(dense[(i + 1) % dn] - dense[i]);
    const float total = cumulative[dn];

    Track track;
    track.halfWidth = halfWidth;
    const int count = std::max(8, static_cast<int>(std::round(total / targetStep)));
    track.step = total / static_cast<float>(count);
    track.length = total;
    track.frames.resize(count);

    int seg = 0;
    for (int i = 0; i < count; i++) {
        float d = track.step * static_cast<float>(i);
        while (seg < dn - 1 && cumulative[seg + 1] < d) seg++;
        float span = cumulative[seg + 1] - cumulative[seg];
        float t = span > 0.0f ? (d - cumulative[seg]) / span : 0.0f;
        track.frames[i].pos = glm::mix(dense[seg], dense[(seg + 1) % dn], t);
    }

    for (int i = 0; i < count; i++) {
        const glm::vec3& prev = track.frames[(i + count - 1) % count].pos;
        const glm::vec3& next = track.frames[(i + 1) % count].pos;
        track.frames[i].tangent = glm::normalize(next - prev);
    }

    // Dévers proportionnel à la courbure, lissé pour éviter les à-coups.
    std::vector<float> bank(count);
    for (int i = 0; i < count; i++) {
        const glm::vec3& t0 = track.frames[(i + count - 1) % count].tangent;
        const glm::vec3& t1 = track.frames[(i + 1) % count].tangent;
        glm::vec3 flatRight =
            glm::normalize(glm::cross(track.frames[i].tangent, glm::vec3(0.0f, 1.0f, 0.0f)));
        float curvature = glm::dot(t1 - t0, flatRight) / (2.0f * track.step);
        track.frames[i].curvature = curvature;
        bank[i] = std::clamp(curvature * 170.0f, -0.42f, 0.42f);
    }
    for (int pass = 0; pass < 12; pass++) {
        std::vector<float> smoothed(count);
        for (int i = 0; i < count; i++)
            smoothed[i] = (bank[(i + count - 1) % count] + 2.0f * bank[i] +
                           bank[(i + 1) % count]) * 0.25f;
        bank.swap(smoothed);
    }
    // La courbure brute est bruitée par l'échantillonnage : même lissage.
    for (int pass = 0; pass < 6; pass++) {
        std::vector<float> smoothed(count);
        for (int i = 0; i < count; i++)
            smoothed[i] = (track.frames[(i + count - 1) % count].curvature +
                           2.0f * track.frames[i].curvature +
                           track.frames[(i + 1) % count].curvature) * 0.25f;
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
    return track;
}

// Tirage reproductible pour le placement des éléments de piste.
float featureRoll(int track, int slot, uint32_t salt) {
    uint32_t h = static_cast<uint32_t>(track + 1) * 0x9E3779B1u ^
                 static_cast<uint32_t>(slot) * 0x85EBCA77u ^ salt * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return static_cast<float>(h & 0xFFFFFFu) / 16777216.0f;
}

// Sème plaques de boost, champs de freinage et barrières le long du circuit.
// Chaque circuit a son dosage : la ville est rapide, le canyon encombré, la
// station piégée. Un passage reste toujours libre.
void placeFeatures(Track& track, int id) {
    struct Recipe {
        float spacing;
        float pad, slow; // parts en ligne droite, le reste en barrières
    };
    static const Recipe recipes[kTrackCount] = {
        {340.0f, 0.50f, 0.22f},
        {300.0f, 0.22f, 0.28f},
        {290.0f, 0.36f, 0.42f},
    };
    const Recipe& recipe = recipes[id < 0 || id >= kTrackCount ? 0 : id];
    const float hw = track.halfWidth;
    const float kPadLength = 36.0f, kPadHalf = 3.6f;
    const float kSlowLength = 50.0f;
    const float kBarrierLength = 3.0f;
    const float kGapHalf = 6.5f; // demi-largeur du passage entre deux barrières

    auto add = [&](FeatureType type, float dist, float length, float lateral, float half) {
        track.features.push_back({type, dist, length, lateral, half});
    };

    const int slots = static_cast<int>((track.length - 450.0f) / recipe.spacing);
    for (int s = 0; s < slots; s++) {
        float dist = 300.0f + recipe.spacing * static_cast<float>(s);
        float curvature = track.at(dist + 20.0f).curvature;
        float type = featureRoll(id, s, 1u);
        float where = featureRoll(id, s, 2u);
        float lanes[3] = {-0.55f * hw, 0.0f, 0.55f * hw};
        float lane = lanes[std::min(2, static_cast<int>(where * 3.0f))];

        if (std::abs(curvature) > 0.0012f) {
            // En virage : un champ de freinage punit la corde, ou une plaque
            // récompense l'extérieur.
            float inside = curvature > 0.0f ? 1.0f : -1.0f;
            if (type < 0.5f) {
                float half = hw * 0.30f;
                add(FeatureType::SlowField, dist, kSlowLength, inside * (hw - half), half);
            } else {
                add(FeatureType::BoostPad, dist, kPadLength, -inside * 0.5f * hw, kPadHalf);
            }
        } else if (type < recipe.pad) {
            add(FeatureType::BoostPad, dist, kPadLength, lane, kPadHalf);
        } else if (type < recipe.pad + recipe.slow) {
            add(FeatureType::SlowField, dist, kSlowLength, lane, hw * 0.34f);
        } else if (where < 0.34f) {
            // Chicane : deux barrières, un passage au milieu.
            float half = (hw - kGapHalf) * 0.5f;
            add(FeatureType::Barrier, dist, kBarrierLength, -(hw - half), half);
            add(FeatureType::Barrier, dist, kBarrierLength, hw - half, half);
        } else if (where < 0.67f) {
            // Plot central, on passe d'un côté ou de l'autre.
            add(FeatureType::Barrier, dist, kBarrierLength, 0.0f, hw * 0.28f);
        } else {
            // Demi-piste fermée, avec une plaque dans le couloir libre.
            float side = featureRoll(id, s, 3u) < 0.5f ? -1.0f : 1.0f;
            float half = hw * 0.42f;
            add(FeatureType::Barrier, dist, kBarrierLength, side * (hw - half), half);
            add(FeatureType::BoostPad, dist + 10.0f, kPadLength, -side * 0.5f * hw, kPadHalf);
        }
    }
}

} // namespace

Track Track::make(int id) {
    const float halfWidth = trackInfo(id).halfWidth;
    std::vector<glm::vec3> control;
    switch (id) {
    case 1:
        // Canyon Solaire : tracé sinueux et vallonné, virages serrés.
        control = {
            {0.0f, 0.0f, 0.0f},       {0.0f, 15.0f, -900.0f},   {350.0f, 70.0f, -1500.0f},
            {1000.0f, 130.0f, -1500.0f}, {1300.0f, 95.0f, -900.0f}, {900.0f, 40.0f, -450.0f},
            {1200.0f, 0.0f, 150.0f},  {1900.0f, -35.0f, 300.0f}, {2300.0f, 25.0f, -300.0f},
            {2900.0f, 95.0f, -200.0f}, {3000.0f, 60.0f, 700.0f}, {2200.0f, 10.0f, 1100.0f},
            {1200.0f, -25.0f, 850.0f}, {500.0f, 30.0f, 1300.0f}, {-150.0f, 20.0f, 800.0f},
        };
        break;
    case 2: {
        // Station Orbitale : un huit, dont une branche enjambe l'autre.
        const int points = 14;
        for (int i = 0; i < points; i++) {
            float t = 0.45f + 6.2831853f * static_cast<float>(i) / points;
            control.push_back({1700.0f * std::sin(t), 45.0f * std::cos(t),
                               -1150.0f * std::sin(2.0f * t)});
        }
        break;
    }
    default:
        // Néon City : grandes courbes rapides, environ 10,7 km.
        control = {
            {0.0f, 0.0f, 0.0f},        {0.0f, 0.0f, -1500.0f},    {450.0f, 25.0f, -2700.0f},
            {1500.0f, 60.0f, -3000.0f}, {2400.0f, 25.0f, -2400.0f}, {2250.0f, 0.0f, -1350.0f},
            {1350.0f, -12.0f, -900.0f}, {1500.0f, 0.0f, 150.0f},   {900.0f, 35.0f, 900.0f},
            {0.0f, 25.0f, 750.0f},
        };
        break;
    }
    Track track = build(control, halfWidth, 4.0f);
    placeFeatures(track, id);
    return track;
}

TrackFrame Track::at(float distance) const {
    const int count = static_cast<int>(frames.size());
    float wrapped = std::fmod(distance, length);
    if (wrapped < 0.0f) wrapped += length;
    float fi = wrapped / step;
    int i0 = static_cast<int>(fi) % count;
    int i1 = (i0 + 1) % count;
    float t = fi - std::floor(fi);

    const TrackFrame& a = frames[i0];
    const TrackFrame& b = frames[i1];
    TrackFrame out;
    out.pos = glm::mix(a.pos, b.pos, t);
    out.tangent = glm::normalize(glm::mix(a.tangent, b.tangent, t));
    out.up = glm::normalize(glm::mix(a.up, b.up, t));
    out.curvature = glm::mix(a.curvature, b.curvature, t);
    out.right = glm::normalize(glm::cross(out.tangent, out.up));
    out.up = glm::cross(out.right, out.tangent);
    return out;
}

glm::vec3 Track::point(float distance, float lateral, float height) const {
    TrackFrame f = at(distance);
    return f.pos + f.right * lateral + f.up * height;
}

} // namespace sim

namespace sim {

float Track::ahead(float from, float to) const {
    float d = std::fmod(to - from, length);
    return d < 0.0f ? d + length : d;
}

} // namespace sim

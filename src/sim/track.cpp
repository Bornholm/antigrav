#include "sim/track.h"

#include <algorithm>
#include <cmath>

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
        bank[i] = std::clamp(curvature * 55.0f, -0.42f, 0.42f);
    }
    for (int pass = 0; pass < 12; pass++) {
        std::vector<float> smoothed(count);
        for (int i = 0; i < count; i++)
            smoothed[i] = (bank[(i + count - 1) % count] + 2.0f * bank[i] +
                           bank[(i + 1) % count]) * 0.25f;
        bank.swap(smoothed);
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

} // namespace

Track Track::makeDemo() {
    std::vector<glm::vec3> control = {
        {0.0f, 0.0f, 0.0f},      {0.0f, 0.0f, -500.0f},   {150.0f, 10.0f, -900.0f},
        {500.0f, 25.0f, -1000.0f}, {800.0f, 10.0f, -800.0f}, {750.0f, 0.0f, -450.0f},
        {450.0f, -5.0f, -300.0f}, {500.0f, 0.0f, 50.0f},   {300.0f, 15.0f, 300.0f},
        {0.0f, 10.0f, 250.0f},
    };
    return build(control, 15.0f, 4.0f);
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
    out.right = glm::normalize(glm::cross(out.tangent, out.up));
    out.up = glm::cross(out.right, out.tangent);
    return out;
}

glm::vec3 Track::point(float distance, float lateral, float height) const {
    TrackFrame f = at(distance);
    return f.pos + f.right * lateral + f.up * height;
}

} // namespace sim

#pragma once

#include "sim/world.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace game {

// Fantôme : la trajectoire du meilleur temps du joueur sur un morceau,
// relevée vingt fois par seconde et rejouée pendant les courses suivantes.
// On garde des positions et non des commandes : le fantôme ne dépend ni des
// IA ni des bonus de la course où il a été enregistré.
struct Ghost {
    static constexpr float kStep = 1.0f / 20.0f; // secondes entre deux relevés
    static constexpr uint32_t kMagic = 0x31475753; // « SWG1 »

    struct Sample {
        float dist = 0.0f, lateral = 0.0f, roll = 0.0f, height = 0.0f;
    };

    int ship = 0;
    float finishTime = 0.0f; // temps de course à l'arrivée, 0 s'il n'y a pas de fantôme
    float trackLength = 0.0f; // longueur de la piste, pour écarter un fantôme d'une autre piste
    std::vector<Sample> samples; // le premier au départ, un tous les kStep

    bool valid() const { return finishTime > 0.0f && samples.size() >= 2; }
    void clear() { *this = Ghost{}; }

    // Relève l'état d'un vaisseau. À appeler à chaque pas de course :
    // il ne garde qu'un relevé tous les kStep.
    void record(const sim::Vehicle& v, float raceTime) {
        while (static_cast<float>(samples.size()) * kStep <= raceTime + 1e-4f)
            samples.push_back({v.dist, v.lateral, v.roll, sim::airHeight(v)});
    }

    // Position à un instant de la course, interpolée entre deux relevés.
    Sample at(float raceTime) const {
        if (samples.empty()) return {};
        const float f = std::max(0.0f, raceTime / kStep);
        const size_t i = std::min(static_cast<size_t>(f), samples.size() - 1);
        if (i + 1 >= samples.size()) return samples.back();
        const float t = f - static_cast<float>(i);
        const Sample& a = samples[i];
        const Sample& b = samples[i + 1];
        return {a.dist + (b.dist - a.dist) * t, a.lateral + (b.lateral - a.lateral) * t,
                a.roll + (b.roll - a.roll) * t, a.height + (b.height - a.height) * t};
    }

    // Instant où le fantôme passait à cette distance, ou un nombre négatif
    // s'il n'y est jamais allé. Les distances relevées ne reculent pas.
    float timeAt(float dist) const {
        if (samples.size() < 2 || dist < samples.front().dist || dist > samples.back().dist) return -1.0f;
        auto it = std::lower_bound(samples.begin(), samples.end(), dist,
                                   [](const Sample& s, float d) { return s.dist < d; });
        if (it == samples.begin()) return 0.0f;
        const Sample& b = *it;
        const Sample& a = *(it - 1);
        const float span = std::max(1e-3f, b.dist - a.dist);
        const float i = static_cast<float>(it - samples.begin() - 1);
        return (i + std::clamp((dist - a.dist) / span, 0.0f, 1.0f)) * kStep;
    }

    // Format du fichier : en-tête, puis les relevés à plat.
    std::vector<uint8_t> encode() const {
        const uint32_t count = static_cast<uint32_t>(samples.size());
        const int32_t shipId = ship;
        std::vector<uint8_t> out(20 + count * sizeof(Sample));
        std::memcpy(out.data(), &kMagic, 4);
        std::memcpy(out.data() + 4, &shipId, 4);
        std::memcpy(out.data() + 8, &finishTime, 4);
        std::memcpy(out.data() + 12, &trackLength, 4);
        std::memcpy(out.data() + 16, &count, 4);
        if (count) std::memcpy(out.data() + 20, samples.data(), count * sizeof(Sample));
        return out;
    }

    bool decode(const uint8_t* data, size_t size) {
        clear();
        if (size < 20) return false;
        uint32_t magic = 0, count = 0;
        int32_t shipId = 0;
        std::memcpy(&magic, data, 4);
        std::memcpy(&shipId, data + 4, 4);
        std::memcpy(&finishTime, data + 8, 4);
        std::memcpy(&trackLength, data + 12, 4);
        std::memcpy(&count, data + 16, 4);
        // Une heure de course au plus : au-delà, le fichier est abîmé.
        if (magic != kMagic || count > 72000 || size != 20 + count * sizeof(Sample) ||
            !std::isfinite(finishTime) || finishTime <= 0.0f) {
            clear();
            return false;
        }
        ship = std::clamp(static_cast<int>(shipId), 0, sim::kShipCount - 1);
        samples.resize(count);
        if (count) std::memcpy(samples.data(), data + 20, count * sizeof(Sample));
        return valid() || (clear(), false);
    }

    // Fichier du fantôme d'un morceau, dans le dossier du jeu.
    bool load(const std::string& key);
    bool save(const std::string& key) const;
};

} // namespace game

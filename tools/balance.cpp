// Outil d'équilibrage. Joue des centaines de courses entre pilotes automatiques
// et affiche le taux de victoire et le rang moyen de chaque modèle de vaisseau
// et de chaque personnalité, circuit par circuit.
//
// Sort avec un code non nul si un modèle ou une personnalité domine, ce qui en
// fait aussi un test : `ctest` échoue quand un réglage casse l'équilibre.
#include "sim/content.h"
#include "sim/track.h"
#include "music/decode.h"
#include "sim/generate.h"
#include "sim/world.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {

struct Tally {
    int races = 0;
    int wins = 0;
    int rankSum = 0;
    double winRate() const { return races ? 100.0 * wins / races : 0.0; }
    double meanRank() const { return races ? static_cast<double>(rankSum) / races : 0.0; }
};

constexpr int kRacers = 4;

// Joue une course et renvoie le rang final de chaque emplacement de grille.
// Horloge musicale mesurée : commune par défaut, par joueur avec l'option --own.
bool ownClock = false;

void runRace(const sim::Track& track, const int* ships, const int* profiles, int /*laps*/,
             int* ranks) {
    sim::World world;
    world.count = kRacers;
    world.ownClock = ownClock;
    world.countdown = 3.0f;
    const float side = track.halfWidth * 0.42f;
    for (int i = 0; i < kRacers; i++) {
        sim::Vehicle& v = world.vehicles[i];
        v.ship = ships[i];
        v.lateral = i % 2 == 0 ? -side : side;
        v.dist = -40.0f - 24.0f * static_cast<float>(i / 2) - (i % 2 ? 8.0f : 0.0f);
    }
    const int maxSteps = 60 * 400;
    for (int s = 0; s < maxSteps; s++) {
        sim::Input inputs[sim::kMaxVehicles];
        for (int i = 0; i < kRacers; i++) {
            float lane = (static_cast<float>(i) / (kRacers - 1) - 0.5f) * track.halfWidth * 0.75f;
            inputs[i] = sim::autopilot(world, i, track, sim::aiProfile(profiles[i]), lane);
        }
        sim::step(world, track, inputs);
        bool done = true;
        for (int i = 0; i < kRacers; i++) done = done && world.vehicles[i].finished;
        if (done) break;
    }
    for (int i = 0; i < kRacers; i++) ranks[i] = sim::rankOf(world, i);
}

void printTable(const char* title, const char* const* names, int count,
                const std::vector<std::vector<Tally>>& perTrack) {
    std::printf("\n%s\n%-14s", title, "");
    for (int t = 0; t < sim::kTrackCount; t++) std::printf("| %-22s", sim::trackInfo(t).name);
    std::printf("| ensemble\n");
    for (int e = 0; e < count; e++) {
        std::printf("%-14s", names[e]);
        Tally total;
        for (int t = 0; t < sim::kTrackCount; t++) {
            const Tally& tally = perTrack[t][e];
            std::printf("| %5.1f %%  rang %.2f     ", tally.winRate(), tally.meanRank());
            total.races += tally.races;
            total.wins += tally.wins;
            total.rankSum += tally.rankSum;
        }
        std::printf("| %5.1f %%  rang %.2f\n", total.winRate(), total.meanRank());
    }
}

// Renvoie le nombre de critères d'équilibre violés.
int judge(const char* what, const char* const* names, int count,
          const std::vector<std::vector<Tally>>& perTrack) {
    int violations = 0;
    for (int e = 0; e < count; e++) {
        Tally total;
        for (int t = 0; t < sim::kTrackCount; t++) {
            total.races += perTrack[t][e].races;
            total.wins += perTrack[t][e].wins;
            total.rankSum += perTrack[t][e].rankSum;
        }
        // À quatre concurrents équivalents, chacun gagnerait 25 % des courses.
        if (total.winRate() > 40.0) {
            std::printf("DESEQUILIBRE: %s %s gagne %.1f %% des courses\n", what, names[e],
                        total.winRate());
            violations++;
        }
        if (total.winRate() < 10.0) {
            std::printf("DESEQUILIBRE: %s %s ne gagne que %.1f %% des courses\n", what, names[e],
                        total.winRate());
            violations++;
        }
    }
    return violations;
}

} // namespace

int main(int argc, char** argv) {
    ownClock = argc > 1 && std::string(argv[1]) == "--own";
    std::printf("Horloge %s\n", ownClock ? "par joueur" : "commune");
    // Les pistes mesurées sont celles des morceaux livrés avec le jeu.
    static const char* const files[sim::kTrackCount] = {"neon.ogg", "canyon.ogg", "station.ogg"};
    for (int t = 0; t < sim::kTrackCount; t++) {
        music::Audio audio;
        std::string error;
        const std::string path = std::string(SOUNDWAVE_MUSIC_DIR) + "/" + files[t];
        if (!music::decodeFile(path, audio, error)) {
            std::printf("%s : %s\n", path.c_str(), error.c_str());
            return 1;
        }
        const std::vector<float> mono = audio.mono();
        sim::setSong(t, music::analyze(mono.data(), mono.size(), audio.sampleRate));
    }
    std::vector<sim::Track> tracks;
    for (int t = 0; t < sim::kTrackCount; t++) tracks.push_back(sim::Track::make(t));

    std::vector<std::vector<Tally>> shipTally(sim::kTrackCount,
                                              std::vector<Tally>(sim::kShipCount));
    std::vector<std::vector<Tally>> profileTally(sim::kTrackCount,
                                                 std::vector<Tally>(sim::kProfileCount));
    std::vector<std::vector<Tally>> mixedShip(sim::kTrackCount,
                                              std::vector<Tally>(sim::kShipCount));
    std::vector<std::vector<Tally>> mixedProfile(sim::kTrackCount,
                                                 std::vector<Tally>(sim::kProfileCount));
    int races = 0;

    for (int t = 0; t < sim::kTrackCount; t++) {
        int laps = 1;
        int order[kRacers] = {0, 1, 2, 3};
        do {
            int ranks[kRacers];
            // Vaisseaux : les quatre modèles s'affrontent, tous avec la même
            // personnalité, sur toutes les grilles de départ possibles.
            for (int p = 0; p < sim::kProfileCount; p++) {
                int profiles[kRacers] = {p, p, p, p};
                runRace(tracks[t], order, profiles, laps, ranks);
                races++;
                for (int i = 0; i < kRacers; i++) {
                    Tally& tally = shipTally[t][order[i]];
                    tally.races++;
                    tally.rankSum += ranks[i];
                    tally.wins += ranks[i] == 1;
                }
            }
            // Personnalités : les quatre s'affrontent, toutes sur le même modèle.
            for (int s = 0; s < sim::kShipCount; s++) {
                int ships[kRacers] = {s, s, s, s};
                runRace(tracks[t], ships, order, laps, ranks);
                races++;
                for (int i = 0; i < kRacers; i++) {
                    Tally& tally = profileTally[t][order[i]];
                    tally.races++;
                    tally.rankSum += ranks[i];
                    tally.wins += ranks[i] == 1;
                }
            }
            // Mélange : modèle et personnalité varient ensemble.
            for (int shift = 0; shift < kRacers; shift++) {
                int ships[kRacers];
                for (int i = 0; i < kRacers; i++) ships[i] = (order[i] + shift) % sim::kShipCount;
                runRace(tracks[t], ships, order, laps, ranks);
                races++;
                for (int i = 0; i < kRacers; i++) {
                    Tally& a = mixedShip[t][ships[i]];
                    Tally& b = mixedProfile[t][order[i]];
                    a.races++;
                    a.rankSum += ranks[i];
                    a.wins += ranks[i] == 1;
                    b.races++;
                    b.rankSum += ranks[i];
                    b.wins += ranks[i] == 1;
                }
            }
        } while (std::next_permutation(order, order + kRacers));
    }

    const char* shipNames[sim::kShipCount];
    for (int s = 0; s < sim::kShipCount; s++) shipNames[s] = sim::shipStats(s).name;
    const char* profileNames[sim::kProfileCount];
    for (int p = 0; p < sim::kProfileCount; p++) profileNames[p] = sim::aiProfile(p).name;

    std::printf("%d courses jouées\n", races);
    printTable("Vaisseaux, à personnalité égale", shipNames, sim::kShipCount, shipTally);
    printTable("Personnalités, à vaisseau égal", profileNames, sim::kProfileCount, profileTally);
    printTable("Vaisseaux, personnalités mélangées", shipNames, sim::kShipCount, mixedShip);
    printTable("Personnalités, vaisseaux mélangés", profileNames, sim::kProfileCount,
               mixedProfile);

    std::printf("\n");
    int violations = judge("le vaisseau", shipNames, sim::kShipCount, shipTally) +
                     judge("la personnalité", profileNames, sim::kProfileCount, profileTally) +
                     judge("le vaisseau (mélange)", shipNames, sim::kShipCount, mixedShip) +
                     judge("la personnalité (mélange)", profileNames, sim::kProfileCount,
                           mixedProfile);
    if (violations == 0) std::printf("Équilibre tenu : aucun modèle ni personnalité ne domine.\n");
    return violations == 0 ? 0 : 1;
}

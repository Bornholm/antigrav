#pragma once

#include "music/analysis.h"
#include "sim/track.h"

#include <cstdint>

namespace sim {

// Génère la piste d'un morceau. La même partition et la même graine donnent
// toujours la même piste.
Track generateTrack(const music::Partition& partition, float halfWidth, uint32_t seed);

// Partition d'un morceau fictif, régulier comme un métronome. Sert de
// morceau par défaut et de base aux tests.
music::Partition metronome(float bpm, float seconds);

// Partition du morceau d'indice donné. Tant que le jeu n'a pas fourni celle
// du vrai morceau par setSong, c'est un métronome.
const music::Partition& song(int id);
void setSong(int id, const music::Partition& partition);

} // namespace sim

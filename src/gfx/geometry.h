#pragma once

#include "gfx/mesh.h"
#include "gfx/theme.h"
#include "sim/track.h"

#include <cstdint>
#include <vector>

namespace gfx {

// Géométrie procédurale. Tout est généré au chargement, rien n'est lu sur disque.
MeshData buildRoad(const sim::Track& track);
MeshData buildRails(const sim::Track& track, const Theme& theme);
MeshData buildGates(const sim::Track& track, const Theme& theme);
// Plaques d'énergie et barrières de la piste.
MeshData buildFeatures(const sim::Track& track);
MeshData buildScenery(const sim::Track& track, const Theme& theme);

// Longueur d'un tronçon de piste, en mètres. Le rendu ne dessine que les
// tronçons proches de la caméra.
constexpr float kChunkLength = 400.0f;
// Trie les triangles par tronçon, d'après le point de la piste le plus proche
// de leur centre. Renvoie le premier indice de chaque tronçon, suivi du
// nombre total d'indices.
std::vector<uint32_t> sortByChunk(MeshData& mesh, const sim::Track& track);

// variant : modèle de vaisseau, de 0 à sim::kShipCount - 1.
MeshData buildShip(int variant);
// Flammes de réacteur, placées sur les tuyères du même modèle.
MeshData buildTrail(int variant);
MeshData buildShield();
// Ce qui vole dans le décor. 0 : véhicule aérien de la ville. 1 : créature
// ailée du canyon. 2 : drone de la station.
MeshData buildFlyer(int kind);
// Sphère de rayon 1, pour les effets lumineux : explosions, ondes, halos.
MeshData buildOrb();
MeshData buildMissile();

} // namespace gfx

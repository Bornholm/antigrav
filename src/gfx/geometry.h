#pragma once

#include "gfx/mesh.h"
#include "sim/track.h"

namespace gfx {

// Géométrie procédurale du prototype. Tout est généré au chargement, rien
// n'est lu sur disque.
MeshData buildRoad(const sim::Track& track);
MeshData buildRails(const sim::Track& track);
MeshData buildGates(const sim::Track& track);
MeshData buildCity(const sim::Track& track);
MeshData buildShip();
MeshData buildTrail();
MeshData buildShield();
MeshData buildMissile();

} // namespace gfx

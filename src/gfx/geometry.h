#pragma once

#include "gfx/mesh.h"
#include "gfx/theme.h"
#include "sim/track.h"

namespace gfx {

// Géométrie procédurale. Tout est généré au chargement, rien n'est lu sur disque.
MeshData buildRoad(const sim::Track& track);
MeshData buildRails(const sim::Track& track, const Theme& theme);
MeshData buildGates(const sim::Track& track, const Theme& theme);
MeshData buildScenery(const sim::Track& track, const Theme& theme);

// variant : modèle de vaisseau, de 0 à sim::kShipCount - 1.
MeshData buildShip(int variant);
// Flammes de réacteur, placées sur les tuyères du même modèle.
MeshData buildTrail(int variant);
MeshData buildShield();
MeshData buildMissile();

} // namespace gfx

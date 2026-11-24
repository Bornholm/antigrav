#include "sim/content.h"

namespace sim {

// Réglé avec tools/balance : aucun modèle ni aucune personnalité ne doit
// l'emporter nettement sur l'ensemble des circuits.
const ShipStats& shipStats(int ship) {
    static const ShipStats stats[kShipCount] = {
        //  nom         accroche                          vit.   acc.   mani.  adh.   rech.  solid.
        {"Flèche", "Équilibrée, sans point faible", 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f},
        {"Manta", "Agile, tient les virages", 0.995f, 1.000f, 1.250f, 1.800f, 1.050f, 0.900f},
        {"Aiguille", "Pointe de vitesse, dérive fort", 1.012f, 0.900f, 0.850f, 0.700f, 0.950f, 0.850f},
        {"Jumeau", "Lourd, repart vite, encaisse", 0.990f, 1.200f, 0.920f, 1.000f, 0.950f, 1.500f},
    };
    return stats[ship < 0 || ship >= kShipCount ? 0 : ship];
}

const TrackInfo& trackInfo(int track) {
    static const TrackInfo infos[kTrackCount] = {
        {"Néon City", "Grandes courbes dans la ville", 15.0f, 3},
        {"Canyon Solaire", "Technique, étroit, vallonné", 13.0f, 3},
        {"Station Orbitale", "Huit à pleine vitesse, avec pont", 17.0f, 3},
    };
    return infos[track < 0 || track >= kTrackCount ? 0 : track];
}

const AiProfile& aiProfile(int profile) {
    static const AiProfile profiles[kProfileCount] = {
        //  nom          ligne   boost: dép.  arrêt  tir: én.  portée  boucl.  rab.   bloque zigzag hasard
        {"Agressif", 3800.0f, 0.75f, 0.30f, 0.45f, 260.0f, 70.0f, true, false, 0.0f, 0.00f},
        {"Défensif", 3900.0f, 0.68f, 0.36f, 0.92f, 150.0f, 170.0f, false, true, 0.0f, 0.00f},
        {"Pilote", 4300.0f, 0.65f, 0.06f, 2.00f, 0.0f, 90.0f, false, false, 0.0f, 0.00f},
        {"Imprévisible", 3400.0f, 0.70f, 0.20f, 0.35f, 300.0f, 110.0f, false, false, 5.0f, 0.25f},
    };
    return profiles[profile < 0 || profile >= kProfileCount ? 0 : profile];
}

} // namespace sim

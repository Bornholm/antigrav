#include "sim/content.h"

#include "sim/generate.h"

#include <cstdio>

namespace sim {

// Réglé avec tools/balance : aucun modèle ni aucune personnalité ne doit
// l'emporter nettement sur l'ensemble des circuits.
const ShipStats& shipStats(int ship) {
    static const ShipStats stats[kShipCount] = {
        //  nom         accroche                          vit.   acc.   mani.  adh.   rech.  solid.
        {"Flèche", "Équilibrée, sans point faible", 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f},
        {"Manta", "Agile, tient les virages", 0.992f, 1.000f, 1.250f, 1.800f, 1.050f, 0.900f},
        {"Aiguille", "Pointe de vitesse, dérive fort", 1.008f, 0.950f, 0.900f, 0.750f, 0.950f, 0.850f},
        {"Jumeau", "Lourd, repart vite, encaisse", 0.997f, 1.150f, 0.920f, 1.000f, 0.950f, 1.500f},
    };
    return stats[ship < 0 || ship >= kShipCount ? 0 : ship];
}

namespace {
char customName[64] = "Morceau du joueur";
char customTagline[64] = "";
} // namespace

const TrackInfo& trackInfo(int track) {
    static const TrackInfo infos[kSongSlots] = {
        {"Néon City", "Outrun synthwave", 15.0f},
        {"Canyon Solaire", "Big beat, basse acide", 13.0f},
        {"Station Orbitale", "Drum and bass froide", 17.0f},
        {customName, customTagline, 15.0f},
    };
    return infos[track < 0 || track >= kSongSlots ? 0 : track];
}

void setCustomTrackInfo(const char* name, const char* tagline) {
    std::snprintf(customName, sizeof(customName), "%s", name);
    std::snprintf(customTagline, sizeof(customTagline), "%s", tagline);
}

const AiProfile& aiProfile(int profile) {
    static const AiProfile profiles[kProfileCount] = {
        //  nom          ligne   boost: dép.  arrêt  rab.   bloque zigzag hasard anticip. rythme
        {"Agressif", 3800.0f, 0.75f, 0.30f, true, false, 0.0f, 0.00f, 1.0f, 0.66f},
        {"Défensif", 3900.0f, 0.68f, 0.36f, false, true, 0.0f, 0.00f, 1.2f, 0.66f},
        {"Pilote", 4300.0f, 0.65f, 0.06f, false, false, 0.0f, 0.00f, 1.3f, 0.66f},
        {"Imprévisible", 3400.0f, 0.70f, 0.20f, false, false, 5.0f, 0.15f, 0.9f, 0.60f},
    };
    return profiles[profile < 0 || profile >= kProfileCount ? 0 : profile];
}

namespace {

music::Partition& songSlot(int id) {
    static music::Partition songs[kSongSlots] = {
        metronome(128.0f, 100.0f),
        metronome(136.0f, 100.0f),
        metronome(172.0f, 100.0f),
        metronome(120.0f, 100.0f),
    };
    return songs[id < 0 || id >= kSongSlots ? 0 : id];
}

} // namespace

const music::Partition& song(int id) { return songSlot(id); }
void setSong(int id, const music::Partition& partition) { songSlot(id) = partition; }

} // namespace sim

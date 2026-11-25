#pragma once

namespace sim {

constexpr int kCupCount = 3;  // coupes livrées avec le jeu
constexpr int kCupRaces = 3;  // courses par coupe
// Morceaux livrés avec le jeu : ceux de la première coupe, puis de la deuxième, etc.
constexpr int kTrackCount = kCupCount * kCupRaces;
// Emplacement du morceau choisi par le joueur parmi ses fichiers.
constexpr int kCustomTrack = kTrackCount;
constexpr int kSongSlots = kTrackCount + 1;
constexpr int kShipCount = 4;
constexpr int kProfileCount = 4;

// Caractéristiques d'un modèle de vaisseau, en facteurs autour de 1.
struct ShipStats {
    const char* name;
    const char* tagline;
    float topSpeed;  // vitesse de croisière et de boost
    float accel;     // accélération, boost compris
    float handling;  // vitesse de déplacement latéral
    float grip;      // résistance à la dérive en virage, plus grand = dérive moins
    float sturdy;    // résistance aux impacts : réduit l'étourdissement et les pertes
};
const ShipStats& shipStats(int ship);

struct TrackInfo {
    const char* name;
    const char* tagline;
    float halfWidth;
};
const TrackInfo& trackInfo(int track);

// Une coupe : trois courses sur un même style musical, avec son identité visuelle.
struct CupInfo {
    const char* name;
    const char* style; // style musical, affiché sous le nom
    float color[3];    // teinte de la coupe dans les menus
};
const CupInfo& cupInfo(int cup);
// Morceau de la course `race` d'une coupe.
inline int cupTrack(int cup, int race) { return cup * kCupRaces + race; }
// Points de coupe pour une place à l'arrivée, 1 pour le premier.
int cupPoints(int rank);
// Nom et sous-titre du morceau du joueur. Les chaînes sont copiées.
void setCustomTrackInfo(const char* name, const char* tagline);

// Personnalité d'un pilote automatique.
struct AiProfile {
    const char* name;
    float lineGain;    // à quel point il coupe l'intérieur des virages
    bool ram;          // se rabat sur les adversaires à sa hauteur
    bool block;        // ferme la porte à celui qui le suit
    float weave;       // amplitude de ses changements de couloir, en mètres
    float erratic;     // part de décisions prises au hasard
    float foresight;   // secondes d'avance avec lesquelles il lit les éléments de piste
    float trigger;     // empressement à déclencher un bonus quand l'occasion se présente, de 0 à 1
};
const AiProfile& aiProfile(int profile);

} // namespace sim

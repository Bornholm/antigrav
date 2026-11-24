#pragma once

namespace sim {

constexpr int kTrackCount = 3; // morceaux livrés avec le jeu
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
    float regen;     // recharge de la jauge d'énergie
    float sturdy;    // résistance aux impacts : réduit l'étourdissement et les pertes
};
const ShipStats& shipStats(int ship);

struct TrackInfo {
    const char* name;
    const char* tagline;
    float halfWidth;
};
const TrackInfo& trackInfo(int track);
// Nom et sous-titre du morceau du joueur. Les chaînes sont copiées.
void setCustomTrackInfo(const char* name, const char* tagline);

// Personnalité d'un pilote automatique.
struct AiProfile {
    const char* name;
    float lineGain;    // à quel point il coupe l'intérieur des virages
    float boostStart;  // énergie à partir de laquelle il lance un boost
    float boostStop;   // énergie à laquelle il le coupe : sa réserve
    float fireEnergy;  // énergie minimale pour tirer ; au-delà de 1, ne tire jamais
    float fireRange;   // portée de tir, en mètres
    float shieldRange; // distance du missile qui déclenche le bouclier ; 0, jamais
    bool ram;          // se rabat sur les adversaires à sa hauteur
    bool block;        // ferme la porte à celui qui le suit
    float weave;       // amplitude de ses changements de couloir, en mètres
    float erratic;     // part de décisions prises au hasard
    float foresight;   // secondes d'avance avec lesquelles il lit les éléments de piste
    float rhythm;      // part des occasions où il joue en rythme, de 0 à 1
};
const AiProfile& aiProfile(int profile);

} // namespace sim

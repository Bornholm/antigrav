#pragma once

#include "game/i18n.h"

#include <cstdint>

namespace game {

// Réglages gardés d'un lancement à l'autre, dans le dossier du jeu.
struct Settings {
    char name[20] = ""; // pseudo, vide tant que le joueur n'en a pas choisi
    Lang lang = Lang::Count; // Count : pas encore choisie, on suit le système
    bool swapButtons = false; // direction à droite, bonus à gauche
    int difficulty = 1;       // niveau des IA : 0 facile, 1 normal, 2 difficile
    int ship = 0;             // dernier vaisseau choisi
    bool ghost = true;        // fantôme du meilleur temps en course solo
    bool tutorialSeen = false; // le tutoriel a déjà été ouvert
    bool checkUpdates = true; // Windows et Linux : chercher une nouvelle version au démarrage
    int quality = -1;         // -1 automatique, 0 basse, 1 moyenne, 2 haute
    int autoQuality = -1;     // niveau choisi par le mode automatique, -1 pas encore estimé
    // Niveau de rendu en vigueur : celui du joueur, sinon celui du mode
    // automatique, haute en attendant son estimation.
    int effectiveQuality() const { return quality >= 0 ? quality : autoQuality >= 0 ? autoQuality : 2; }
    int cadence = -1;         // -1 automatique, 0 maximale, 1 plafond à 60 images par seconde
    bool autoCap60 = false;   // plafond choisi par le mode automatique
    bool cap60() const { return cadence == 1 || (cadence < 0 && autoCap60); }
    // Touches du clavier pour tourner à gauche, à droite et utiliser le bonus,
    // en codes de position. 0 : touche par défaut.
    int keys[3] = {0, 0, 0};

    void load();
    void save() const;
};

} // namespace game

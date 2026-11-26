#pragma once

#include "game/i18n.h"

namespace game {

// Réglages gardés d'un lancement à l'autre, dans le dossier du jeu.
struct Settings {
    char name[20] = ""; // pseudo, vide tant que le joueur n'en a pas choisi
    Lang lang = Lang::Count; // Count : pas encore choisie, on suit le système
    bool swapButtons = false; // direction à droite, bonus à gauche
    int difficulty = 1;       // niveau des IA : 0 facile, 1 normal, 2 difficile
    int ship = 0;             // dernier vaisseau choisi
    int quality = -1;         // -1 automatique, 0 basse, 1 moyenne, 2 haute
    int autoQuality = -1;     // niveau choisi par le mode automatique, -1 pas encore estimé
    // Niveau de rendu en vigueur : celui du joueur, sinon celui du mode
    // automatique, haute en attendant son estimation.
    int effectiveQuality() const { return quality >= 0 ? quality : autoQuality >= 0 ? autoQuality : 2; }
    bool cap60 = false;       // plafonne l'affichage à 60 images par seconde
    // Touches du clavier pour tourner à gauche, à droite et utiliser le bonus,
    // en codes de position. 0 : touche par défaut.
    int keys[3] = {0, 0, 0};

    void load();
    void save() const;
};

} // namespace game

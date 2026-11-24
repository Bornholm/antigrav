#pragma once

#include "game/i18n.h"

namespace game {

// Réglages gardés d'un lancement à l'autre, dans le dossier du jeu.
struct Settings {
    char name[20] = ""; // pseudo, vide tant que le joueur n'en a pas choisi
    Lang lang = Lang::Count; // Count : pas encore choisie, on suit le système

    void load();
    void save() const;
};

} // namespace game

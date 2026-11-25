#pragma once

#include <string>
#include <utility>
#include <vector>

namespace game {

// Meilleur score du joueur sur chaque morceau, gardé d'un lancement à l'autre.
struct Records {
    std::vector<std::pair<std::string, int>> best; // clé du morceau, score

    void load();
    void save() const;
    // Meilleur score connu, 0 s'il n'y en a pas.
    int get(const std::string& key) const;
    // Note un score. Renvoie vrai s'il bat le précédent.
    bool submit(const std::string& key, int score);
};

} // namespace game

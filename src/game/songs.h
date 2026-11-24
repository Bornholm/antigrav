#pragma once

#include <string>
#include <vector>

namespace game {

class Music;

// Morceaux du joueur. Un fichier ouvert est copié dans le dossier du jeu, ce
// qui le garde lisible aux lancements suivants, y compris sur Android où le
// droit de lecture accordé par le sélecteur ne dure que la session.
struct SongLibrary {
    struct Entry {
        std::string name; // nom affiché
        std::string path; // copie dans le dossier du jeu
    };
    static constexpr int kMaxEntries = 12;

    std::vector<Entry> entries; // du plus récent au plus ancien
    std::string error;          // dernier message d'erreur, vide si tout va bien

    // Relit le dossier.
    void scan();
    // Copie un fichier dans la bibliothèque. Renvoie son indice, ou -1 avec
    // `error` renseigné.
    int import(const std::string& source);
    // Décode et analyse un morceau, puis le place dans l'emplacement du
    // joueur : partition pour la piste, échantillons pour la musique.
    bool load(int index, Music* music);
    bool loadFile(const std::string& path, const std::string& name, Music* music);
};

// Décor choisi pour le morceau du joueur, et numéro qui change à chaque
// chargement : le rendu s'en sert pour savoir quand reconstruire la piste.
int customTheme();
int customSerial();

} // namespace game

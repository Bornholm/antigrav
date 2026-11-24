#pragma once

#include "music/analysis.h"
#include "sim/track.h"

#include <cstdint>
#include <string>
#include <vector>

namespace game {

class Music;
struct SongPackage;

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

    // Fichier du morceau du joueur actuellement chargé, vide s'il n'y en a pas.
    std::string loadedPath;
    // Hôte : paquet du morceau d'indice de piste donné. Vide avec `error`
    // renseigné si le morceau ne peut pas être envoyé.
    std::vector<unsigned char> makePackage(int track);
    // Client : installe le morceau reçu de l'hôte. Remplit `out` pour que la
    // course reprenne ses éléments de piste.
    bool applyPackage(const std::vector<unsigned char>& bytes, Music* music, SongPackage& out);
};

// Paquet envoyé par l'hôte aux autres joueurs : partition, éléments de piste
// et, pour un morceau du joueur, le fichier audio.
struct SongPackage {
    int track = 0;         // morceau livré, ou sim::kCustomTrack
    char name[64] = "";
    music::Partition partition;
    std::vector<sim::Feature> features;
    std::vector<unsigned char> audio; // vide pour un morceau livré
};
// Identifiant d'un paquet : il change dès que son contenu change.
uint64_t packageId(const std::vector<unsigned char>& bytes);

// Décor choisi pour le morceau du joueur, et numéro qui change à chaque
// chargement : le rendu s'en sert pour savoir quand reconstruire la piste.
int customTheme();
int customSerial();

} // namespace game

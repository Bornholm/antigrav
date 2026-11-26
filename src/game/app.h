#pragma once

#include "game/records.h"
#include "game/music.h"
#include "game/race.h"
#include "game/settings.h"
#include "game/songs.h"
#include "game/ui.h"
#include "net/net.h"

struct SDL_Window;

#include <functional>
#include <cstring>
#include <cstdio>
#include <string>

namespace game {

enum class Screen { Main, Options, Keys, Solo, Songs, Cups, CupTable, Multi, Address, Name, Lobby, Race };

// Une coupe en cours : trois courses, des points selon la place, et le score
// du joueur cumulé pour la note finale. Le classement est tenu par nom, ce qui
// supporte qu'un joueur arrive ou parte entre deux courses en multijoueur.
struct CupRun {
    bool active = false;
    int cup = 0;
    int done = 0; // courses déjà courues
    struct Entry {
        char name[20] = "";
        int points = 0;
        int gained = 0; // points de la dernière course
    };
    Entry entries[sim::kMaxVehicles];
    int count = 0;
    int scoreSum = 0;     // somme des scores du joueur
    int scored = 0;       // courses comptées dans cette somme
    bool counted = false; // la course finie est déjà comptée
    bool newRecord = false;

    bool finished() const { return done >= sim::kCupRaces; }
    // Donne des points à un concurrent, qu'on ajoute au classement s'il est nouveau.
    void award(const char* name, int points) {
        Entry* found = nullptr;
        for (int i = 0; i < count; i++)
            if (!std::strcmp(entries[i].name, name)) found = &entries[i];
        if (!found && count < sim::kMaxVehicles) {
            found = &entries[count++];
            std::snprintf(found->name, sizeof(found->name), "%s", name);
        }
        if (!found) return;
        found->gained = points;
        found->points += points;
    }
    // Place d'une entrée au classement général, 1 pour la première.
    int standing(int entry) const {
        int rank = 1;
        for (int i = 0; i < count; i++)
            if (entries[i].points > entries[entry].points ||
                (entries[i].points == entries[entry].points && i < entry))
                rank++;
        return rank;
    }
    // Place d'un concurrent par son nom, 0 s'il n'est pas classé.
    int standingOf(const char* name) const {
        for (int i = 0; i < count; i++)
            if (!std::strcmp(entries[i].name, name)) return standing(i);
        return 0;
    }
};

// Le jeu au-dessus d'une course : menus, réglages de partie, salon réseau.
struct App {
    Screen screen = Screen::Main;
    Race race;
    net::Net net;
    Ui ui;
    gfx::HudBatch menuHud;
    Synth* synth = nullptr;
    Music* music = nullptr;
    SDL_Window* window = nullptr; // pour le sélecteur de fichiers
    SongLibrary songs;
    Settings settings;
    Records records;
    CupRun cupRun;
    RaceConfig cupConfig; // concurrents d'une coupe en solo
    int lobbyCup = -1;    // hôte : coupe choisie pour le salon, -1 pour une course simple
    int clientCupSeen = 0; // client : nombre de courses de coupe attendu au classement
    int cupChoice = 0;  // coupe choisie dans le menu des coupes
    int keyCapture = -1; // commande qui attend sa touche dans l'écran Clavier, -1 sinon
    int songsCup = 0;   // coupe affichée dans la liste des morceaux
    bool scoreSaved = false; // le score de la course en cours est déjà enregistré
    // Clé d'un morceau dans les records : par son indice dans la liste, ou
    // celui de la course en cours.
    std::string songKey(int index) const;
    std::string raceSongKey() const;
    char nameDraft[20] = ""; // pseudo en cours de saisie
    // Écrans où reviennent le choix du morceau et la saisie du pseudo.
    Screen songReturn = Screen::Solo;
    Screen nameReturn = Screen::Multi;
    bool quit = false;
    const char* statsLine = "";

    // Réglages choisis dans les menus.
    int track = 0;
    // Position du sélecteur de morceau : les morceaux livrés, puis ceux du
    // joueur, puis l'entrée qui ouvre un fichier.
    int songIndex = 0;
    // Multijoueur. Hôte : morceau proposé. Client : morceau de l'hôte déjà
    // installé, et son contenu pour la course à venir.
    uint64_t publishedSong = 0;
    uint64_t appliedSong = 0;
    SongPackage hostSong;
    char songStatus[80] = "";
    int ship = 0;
    int opponents = 3;
    int personality = 0; // 0 : variées, sinon personnalité + 1
    char playerName[20] = "Pilote";
    char address[20] = "";
    const char* notice = ""; // message d'état affiché dans les menus

    double now = 0.0;
    double screenSince = 0.0; // instant du dernier changement d'écran
    double lastDiscover = -10.0;
    uint32_t lastSnapshotTick = 0;
    // Travail en attente derrière l'écran de chargement. Hors fenêtre, pour
    // les captures et les essais, tout s'exécute sur-le-champ.
    std::function<void()> pending;
    int pendingFrames = 0;
    bool deferLoads = false;
    bool confirmQuit = false; // demande de confirmation avant d'abandonner une course
    // Vrai quand le menu de course fige la partie : en solo seulement.
    bool paused() const;
    bool inRace = false; // une vraie course est en cours, pas la démonstration
    // Automatismes pour les essais sans interface : l'hôte lance dès que ce
    // nombre de joueurs est atteint, le client rejoint la première partie vue.
    int autoHostPlayers = 0;
    bool autoJoin = false;
    // Essais sans interface : enchaîne seul les écrans de fin de course et de
    // classement, et quitte à la fin d'une coupe.
    bool autoPlay = false;

    void init(int startTrack);
    // Court sur un fichier audio donné, sans passer par le sélecteur.
    bool useSongFile(const std::string& path);
    // Copie un fichier dans la bibliothèque du joueur et le sélectionne.
    bool importSong(const std::string& path);
    // Hôte : propose le morceau courant aux autres joueurs.
    void publishSong();
    // Lance directement une course, sans passer par les menus.
    void startQuickRace(const RaceConfig& config, bool autopilot);
    void update(double time, float dt, const Controls& controls, const UiInput& input,
                glm::vec2 screenSize, const gfx::FontAtlas& fonts);
    void fill(const gfx::FontAtlas& fonts, glm::vec2 screenSize, gfx::FrameParams& out);

  private:
    void go(Screen next);
    void startAttract();
    void changeAttractShip();
    void selectSong(int index);
    void selectSongNow(int index);
    // Lance un travail long derrière un écran de chargement : il s'exécute à
    // l'image suivante, une fois le message affiché.
    void later(std::function<void()> work);
    // Client : reçoit et installe le morceau de l'hôte.
    void receiveSong();
    void installHostSong();
    void openSongDialog();
    void pollSongDialog();
    const char* songLabel(int index) const;
    int songChoices() const { return sim::kTrackCount + static_cast<int>(songs.entries.size()) + 1; }
    void startSolo();
    void startCup(int cup);
    void startCupRace();
    // Compte la course qui vient de finir dans la coupe : points selon les places.
    void awardCup();
    // Hôte : recopie la coupe dans l'état diffusé aux clients.
    void publishCup();
    void startHosted();
    void leaveNetwork();
    RaceConfig withOpponents(RaceConfig base, int count) const;
    void drawMenus(glm::vec2 screenSize);
    void drawShipCard(glm::vec2 center);
};

} // namespace game

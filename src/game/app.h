#pragma once

#include "game/music.h"
#include "game/race.h"
#include "game/songs.h"
#include "game/ui.h"
#include "net/net.h"

struct SDL_Window;

#include <string>

namespace game {

enum class Screen { Main, Solo, Multi, Address, Lobby, Race };

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
    double lastDiscover = -10.0;
    uint32_t lastSnapshotTick = 0;
    float lastRaceTime = -1.0f;
    bool confirmQuit = false; // demande de confirmation avant d'abandonner une course
    bool inRace = false; // une vraie course est en cours, pas la démonstration
    // Automatismes pour les essais sans interface : l'hôte lance dès que ce
    // nombre de joueurs est atteint, le client rejoint la première partie vue.
    int autoHostPlayers = 0;
    bool autoJoin = false;

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
    void selectSong(int index);
    // Client : reçoit et installe le morceau de l'hôte.
    void receiveSong();
    void openSongDialog();
    void pollSongDialog();
    const char* songLabel(int index) const;
    int songChoices() const { return sim::kTrackCount + static_cast<int>(songs.entries.size()) + 1; }
    void startSolo();
    void startHosted();
    void leaveNetwork();
    RaceConfig withOpponents(RaceConfig base, int count) const;
    void drawMenus(glm::vec2 screenSize);
    void drawShipCard(glm::vec2 center);
};

} // namespace game

#pragma once

#include "game/race.h"
#include "game/ui.h"
#include "net/net.h"

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
    bool quit = false;
    const char* statsLine = "";

    // Réglages choisis dans les menus.
    int track = 0;
    int ship = 0;
    int opponents = 3;
    int personality = 0; // 0 : variées, sinon personnalité + 1
    int laps = 3;
    char playerName[20] = "Pilote";
    char address[20] = "";
    const char* notice = ""; // message d'état affiché dans les menus

    double now = 0.0;
    double lastDiscover = -10.0;
    uint32_t lastSnapshotTick = 0;
    bool inRace = false; // une vraie course est en cours, pas la démonstration
    // Automatismes pour les essais sans interface : l'hôte lance dès que ce
    // nombre de joueurs est atteint, le client rejoint la première partie vue.
    int autoHostPlayers = 0;
    bool autoJoin = false;

    void init(int startTrack);
    // Lance directement une course, sans passer par les menus.
    void startQuickRace(const RaceConfig& config, bool autopilot);
    void update(double time, float dt, const Controls& controls, const UiInput& input,
                glm::vec2 screenSize, const gfx::FontAtlas& fonts);
    void fill(const gfx::FontAtlas& fonts, glm::vec2 screenSize, gfx::FrameParams& out);

  private:
    void go(Screen next);
    void startAttract();
    void startSolo();
    void startHosted();
    void leaveNetwork();
    RaceConfig withOpponents(RaceConfig base, int count) const;
    void drawMenus(glm::vec2 screenSize);
    void drawShipCard(glm::vec2 center);
};

} // namespace game

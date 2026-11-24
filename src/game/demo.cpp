#include "game/demo.h"

#include <cmath>

namespace game {

namespace {

const glm::vec3 kCyan{0.05f, 0.75f, 1.0f};
const glm::vec3 kMagenta{1.0f, 0.08f, 0.75f};
const glm::vec3 kPink{1.0f, 0.25f, 0.55f};

struct ShipSetup {
    float ahead;   // mètres devant le joueur
    float lateral; // décalage latéral moyen
    float sway;    // amplitude du balancement latéral
    float phase;
    glm::vec3 trim;
    glm::vec3 engine;
};

const ShipSetup kShips[] = {
    {0.0f, 0.0f, 0.6f, 0.0f, kMagenta, kCyan},     // joueur
    {34.0f, -8.5f, 1.2f, 1.3f, kMagenta, kPink},   // SynthFox
    {120.0f, -1.0f, 2.0f, 2.9f, kPink, kMagenta},  // KazeVR
    {66.0f, 8.0f, 1.5f, 4.4f, kCyan, kCyan},       // NovaBlade
};

} // namespace

void Demo::init() { track = sim::Track::makeDemo(); }

void Demo::fill(float time, gfx::FrameParams& out) const {
    const float hover = 0.55f;
    float playerDist = 150.0f + speed * time;

    out.time = time;
    out.speed = speed;
    out.shipCount = 0;

    float playerLateral = 0.0f;
    for (const ShipSetup& setup : kShips) {
        float dist = playerDist + setup.ahead + std::sin(time * 0.6f + setup.phase) * 3.0f;
        float lateral = setup.lateral + std::sin(time * 0.9f + setup.phase) * setup.sway;
        float roll = std::cos(time * 0.9f + setup.phase) * setup.sway * 0.06f;
        float bob = std::sin(time * 3.1f + setup.phase) * 0.05f;
        if (setup.ahead == 0.0f) playerLateral = lateral;

        sim::TrackFrame f = track.at(dist);
        glm::vec3 up = glm::normalize(f.up * std::cos(roll) + f.right * std::sin(roll));
        glm::vec3 right = glm::cross(f.tangent, up);
        glm::vec3 pos = f.pos + f.right * lateral + f.up * (hover + bob);

        gfx::ShipDraw& ship = out.ships[out.shipCount++];
        // Le maillage regarde vers -Z, donc son axe +Z pointe vers l'arrière.
        ship.model = glm::mat4(glm::vec4(right, 0.0f), glm::vec4(up, 0.0f),
                               glm::vec4(-f.tangent, 0.0f), glm::vec4(pos, 1.0f));
        ship.planePoint = f.pos;
        ship.planeNormal = f.up;
        ship.trim = glm::vec4(setup.trim, 6.0f);
        ship.engine = glm::vec4(setup.engine, 9.0f);
    }

    // Caméra de poursuite : derrière et au-dessus du joueur, inclinée avec la
    // piste, et qui regarde loin devant.
    sim::TrackFrame camFrame = track.at(playerDist - 9.5f);
    out.camPos = camFrame.pos + camFrame.right * (playerLateral * 0.7f) + camFrame.up * 3.1f;
    out.camTarget = track.point(playerDist + 32.0f, playerLateral * 0.4f, 0.9f);
    out.camUp = camFrame.up;
    out.fovY = 1.12f;
    out.planePoint = camFrame.pos;
    out.planeNormal = camFrame.up;
}

} // namespace game

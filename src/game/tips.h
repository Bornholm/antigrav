#pragma once

#include <cstdint>

namespace game {

// Initiation : des conseils qui s'affichent pendant les premières courses,
// chacun une seule fois, au moment où il sert. Le joueur continue de piloter
// pendant qu'ils s'affichent.
enum class Tip : uint8_t {
    StartTurbo, // compte à rebours : le départ turbo
    Steer,      // juste après le départ : tourner
    Ghost,      // un fantôme est en piste
    Tempo,      // l'écart avec la musique
    Shot,       // premier bonus de chaque sorte ramassé
    Mine,
    Pulse,
    Turbo,
    Shield,
    Comet,
    Barrier,    // une barrière arrive
    Jump,       // un tremplin arrive
    Used,       // pas un conseil : le joueur a déjà déclenché un bonus
    Count
};

struct Tips {
    static constexpr float kShow = 4.5f; // secondes d'affichage d'un conseil
    static constexpr int kQueue = 6;

    uint32_t seen = 0; // un bit par conseil déjà montré, gardé dans les réglages
    bool dirty = false; // seen a changé depuis le dernier enregistrement

    int current = -1; // conseil affiché, -1 pour aucun
    float age = 0.0f;  // depuis combien de temps il est affiché
    struct Waiting {
        Tip tip;
        float age;     // depuis combien de temps il attend
        float patience; // au-delà, il n'est plus d'actualité : il repassera une autre fois
    };
    Waiting queue[kQueue];
    int queued = 0;

    bool has(Tip tip) const { return seen & (1u << static_cast<int>(tip)); }
    void mark(Tip tip) {
        if (has(tip)) return;
        seen |= 1u << static_cast<int>(tip);
        dirty = true;
    }
    // Nouvelle course : rien d'affiché, rien en attente.
    void reset() {
        current = -1;
        queued = 0;
    }
    // Demande un conseil, s'il n'a jamais été montré et n'attend pas déjà.
    void want(Tip tip, float patience) {
        if (has(tip) || current == static_cast<int>(tip)) return;
        for (int i = 0; i < queued; i++)
            if (queue[i].tip == tip) return;
        if (queued < kQueue) queue[queued++] = {tip, 0.0f, patience};
    }
    void update(float dt) {
        for (int i = 0; i < queued; i++) queue[i].age += dt;
        if (current >= 0) {
            age += dt;
            if (age < kShow) return;
            current = -1;
        }
        // Le premier conseil encore d'actualité passe à l'écran, les autres
        // attendent leur tour.
        while (queued > 0) {
            const Waiting next = queue[0];
            for (int i = 1; i < queued; i++) queue[i - 1] = queue[i];
            queued--;
            if (next.age > next.patience || has(next.tip)) continue;
            current = static_cast<int>(next.tip);
            age = 0.0f;
            mark(next.tip);
            break;
        }
    }
    // Opacité du conseil affiché : il apparaît puis s'efface en douceur.
    float level() const {
        if (current < 0) return 0.0f;
        const float in = age / 0.25f, out = (kShow - age) / 0.5f;
        return in < 1.0f ? in : out < 1.0f ? (out > 0.0f ? out : 0.0f) : 1.0f;
    }
};

// Texte d'un conseil dans la langue du jeu : un titre, une phrase et un
// détail facultatif, vide s'il n'y en a pas. Le détail sur la commande du
// bonus ne s'affiche que tant que le joueur n'en a jamais déclenché.
struct TipText {
    const char* title = "";
    char body[128] = "";
    char detail[128] = "";
};
TipText tipText(Tip tip, const Tips& tips);

} // namespace game

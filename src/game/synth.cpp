#include "game/synth.h"

#include <algorithm>
#include <cmath>

namespace game {

namespace {

constexpr float kTau = 6.28318530718f;

} // namespace

void Synth::setEngine(float speed, bool boosting, bool scraping) {
    speed_.store(speed);
    boost_.store(boosting ? 1.0f : 0.0f);
    scrape_.store(scraping ? 1.0f : 0.0f);
}

void Synth::jingle(int kind) {
    jingleKind_.store(kind);
    jingleCount_.fetch_add(1);
}

// Musique de fin : une nappe grave de scies désaccordées derrière un filtre
// qui s'ouvre ou se referme, lancée par un impact sourd. Pas de mélodie.
// Victoire : accord suspendu qui s'ouvre et tient. Défaite : accord mineur
// qui s'affaisse comme une bande qu'on freine, et s'étouffe.
float Synth::jingleSample(float dt) {
    if (jinglePlaying_ < 0) return 0.0f;
    const bool win = jinglePlaying_ == 0;
    const float t = jingleTime_;
    const float length = win ? 4.2f : 3.6f;
    static const float kWin[] = {73.42f, 146.83f, 164.81f, 220.00f, 293.66f};  // ré, mi, la : suspendu
    static const float kLose[] = {73.42f, 146.83f, 174.61f, 220.00f, 110.00f}; // ré mineur
    const float* notes = win ? kWin : kLose;
    // Défaite : la hauteur tombe de trois demi-tons. La phase est l'intégrale de la fréquence.
    const float sag = win ? 0.0f : 0.055f;
    const float warp = sag > 0.0f ? (1.0f - std::exp(-sag * t)) / sag : t;
    float pad = 0.0f;
    for (int i = 0; i < 5; i++) {
        for (int k = 0; k < 2; k++) {
            const float x = notes[i] * (k ? 1.004f : 0.996f) * warp + 0.37f * static_cast<float>(i * 2 + k);
            pad += (2.0f * (x - std::floor(x)) - 1.0f) * (i == 0 ? 0.9f : 0.5f);
        }
    }
    // Filtre passe-bas à un pôle, deux fois : la couleur de la nappe.
    const float cutoff = win ? 260.0f + 2600.0f * std::min(1.0f, t / 0.7f) * std::exp(-std::max(0.0f, t - 0.7f) * 0.55f)
                             : 1500.0f * std::exp(-t * 1.1f) + 120.0f;
    const float a = 1.0f - std::exp(-kTau * cutoff * dt);
    jingleLp_[0] += (pad - jingleLp_[0]) * a;
    jingleLp_[1] += (jingleLp_[0] - jingleLp_[1]) * a;
    const float swell = win ? std::min(1.0f, t / 0.35f) : std::min(1.0f, t / 0.08f);
    const float fade = std::min(1.0f, (length - t) / 1.4f);
    float out = jingleLp_[1] * swell * std::max(0.0f, fade) * 0.085f;
    // Impact d'ouverture : un coup de basse qui descend et un souffle bref.
    const float hit = std::exp(-t * (win ? 5.0f : 3.5f));
    const float drop = (win ? 46.0f : 38.0f) * t + (win ? 55.0f : 40.0f) * (1.0f - std::exp(-t * 9.0f)) / 9.0f;
    out += std::tanh(std::sin(drop * kTau) * 1.8f) * hit * 0.26f;
    jingleLp_[2] += (nextNoise() - jingleLp_[2]) * 0.06f;
    out += jingleLp_[2] * std::exp(-t * 7.0f) * 0.5f;
    jingleTime_ += dt;
    if (jingleTime_ > length + 1.0f) jinglePlaying_ = -1; // l'écho a fini de s'éteindre
    return out;
}

void Synth::beep(int kind) {
    beepKind_.store(kind);
    beepCount_.fetch_add(1);
}

void Synth::play(Sfx sfx, float volume) {
    const uint32_t slot = sfxWrite_.load();
    const uint16_t level = static_cast<uint16_t>(std::clamp(volume, 0.0f, 1.0f) * 255.0f);
    sfxQueue_[slot % kQueue].store(static_cast<uint16_t>(static_cast<uint16_t>(sfx) << 8 | level));
    sfxWrite_.store(slot + 1);
}

// Un échantillon d'une voix d'effet. Renvoie 0 et libère la voix à la fin.
float Synth::voiceSample(Voice& v, float dt) {
    static const float kLength[static_cast<int>(Sfx::Count)] = {0.16f, 1.00f, 0.20f, 1.30f, 0.85f, 0.60f, 0.35f, 1.40f, 0.55f, 0.45f};
    const float length = kLength[v.kind], t = v.time, u = t / length;
    if (u >= 1.0f) {
        v.kind = -1;
        return 0.0f;
    }
    auto tone = [&](float freq) {
        v.phase += freq * dt;
        if (v.phase >= 1.0f) v.phase -= std::floor(v.phase);
        return std::sin(v.phase * kTau);
    };
    const float n = nextNoise();
    // Registre commun : des graves saturés et du souffle filtré, jamais de
    // note aiguë ni de petite mélodie.
    auto drive = [](float x) { return std::tanh(x * 2.2f); };
    float out = 0.0f;
    switch (static_cast<Sfx>(v.kind)) {
    case Sfx::Pickup: // verrouillage : un déclic et un coup mat
        v.low += (n - v.low) * 0.35f;
        out = (n - v.low) * std::exp(-t * 90.0f) * 0.5f + drive(tone(170.0f * std::exp(-t * 10.0f) + 70.0f)) * std::exp(-t * 22.0f) * 0.55f;
        break;
    case Sfx::Shot: { // roquette : coup de départ, puis le souffle du moteur qui s'éloigne
        // Souffle : du bruit dont le filtre se referme à mesure que la roquette part.
        v.low += (n - v.low) * (0.15f * std::exp(-t * 3.0f) + 0.018f);
        v.band += (v.low - v.band) * 0.007f;
        const float roar = v.low - v.band;
        // Le grondement fait crépiter le souffle, comme une flamme.
        const float crackle = 0.65f + std::min(0.35f, std::abs(v.band) * 9.0f);
        const float envelope = std::min(1.0f, t * 140.0f) * std::exp(-t * 3.4f) * std::min(1.0f, (1.0f - u) * 6.0f);
        out = roar * crackle * envelope * 3.8f + drive(tone(70.0f * std::exp(-t * 12.0f) + 32.0f)) * std::exp(-t * 12.0f) * 0.7f;
        break;
    }
    case Sfx::Mine: // pose : coup sourd et court
        out = drive(tone(95.0f * std::exp(-t * 12.0f) + 40.0f)) * std::exp(-t * 16.0f) * 0.8f;
        break;
    case Sfx::Pulse: { // onde : une vague de pression, sourde, qui enfle puis reflue
        // Bruit très filtré : il ne reste que le roulement grave.
        v.low += (n - v.low) * 0.012f;
        v.band += (v.low - v.band) * 0.012f;
        // Monte en un dixième de seconde, retombe lentement.
        const float swell = std::min(1.0f, t / 0.11f) * std::exp(-std::max(0.0f, t - 0.11f) * 3.2f) *
                            std::min(1.0f, (1.0f - u) * 5.0f);
        out = (std::sin(tone(46.0f * std::exp(-t * 0.9f) + 22.0f) * 1.2f) * 0.95f + v.band * 9.0f) * swell;
        break;
    }
    case Sfx::Turbo: // poussée : souffle dont le filtre s'ouvre, sur un grondement
        v.low += (n - v.low) * (0.02f + 0.20f * u);
        out = v.low * std::sin(3.14159265f * u) * 1.1f + drive(tone(48.0f + 30.0f * u)) * std::sin(3.14159265f * u) * 0.35f;
        break;
    case Sfx::ShieldUp: // champ : bourdon de quinte qui enfle, avec un battement lent
        v.low += (n - v.low) * 0.04f;
        out = (tone(98.0f) + std::sin(v.phase * kTau * 1.5f) * 0.6f) * (0.75f + 0.25f * std::sin(t * kTau * 7.0f)) *
                  std::sin(3.14159265f * std::sqrt(u)) * 0.42f +
              v.low * std::sin(3.14159265f * u) * 0.8f;
        break;
    case Sfx::ShieldBreak: // rupture : craquement et chute grave
        v.low += (n - v.low) * 0.3f;
        out = v.low * std::exp(-t * 16.0f) * 1.0f + drive(tone(240.0f * std::exp(-t * 9.0f) + 50.0f)) * std::exp(-t * 11.0f) * 0.5f;
        break;
    case Sfx::Comet: // grondement qui enfle
        v.low += (n - v.low) * 0.05f;
        out = (drive(tone(55.0f + 60.0f * u)) * (0.7f + 0.3f * std::sin(t * kTau * 9.0f)) * 0.6f + v.low * 1.5f) * std::sin(3.14159265f * u);
        break;
    case Sfx::Hit: // impact : fracas étouffé et coup de basse
        v.low += (n - v.low) * 0.22f;
        out = v.low * std::exp(-t * 8.0f) * 1.3f + drive(tone(130.0f * std::exp(-t * 6.0f) + 35.0f)) * std::exp(-t * 6.0f) * 0.7f;
        break;
    case Sfx::Jump: // envol : souffle d'air, sans note
        v.low += (n - v.low) * (0.03f + 0.12f * u);
        out = v.low * std::sin(3.14159265f * u) * 0.9f + drive(tone(60.0f)) * std::exp(-t * 12.0f) * 0.4f;
        break;
    default: break;
    }
    v.time += dt;
    return out * v.volume;
}

float Synth::nextNoise() {
    noise_ = noise_ * 1664525u + 1013904223u;
    return static_cast<float>(noise_ >> 8) / 8388608.0f - 1.0f;
}

void Synth::render(float* out, int frames) {
    const float dt = 1.0f / static_cast<float>(kSampleRate);
    const float speedTarget = speed_.load();
    const float boostTarget = boost_.load();
    const float scrapeTarget = scrape_.load();
    const float volume = volume_.load();
    // Nouveaux effets : chacun prend une voix libre, ou la plus ancienne.
    for (const uint32_t write = sfxWrite_.load(); sfxRead_ != write; sfxRead_++) {
        const uint16_t packed = sfxQueue_[sfxRead_ % kQueue].load();
        Voice* voice = &voices_[0];
        for (Voice& v : voices_) {
            if (v.kind < 0) {
                voice = &v;
                break;
            }
            if (v.time > voice->time) voice = &v;
        }
        *voice = Voice{};
        voice->kind = std::min<int>(packed >> 8, static_cast<int>(Sfx::Count) - 1);
        voice->volume = static_cast<float>(packed & 0xFF) / 255.0f;
    }

    if (const uint32_t jingles = jingleCount_.load(); jingles != jingleSeen_) {
        jingleSeen_ = jingles;
        jinglePlaying_ = jingleKind_.load() == 0 ? 0 : 1;
        jingleTime_ = 0.0f;
        jingleLp_[0] = jingleLp_[1] = jingleLp_[2] = 0.0f;
    }

    uint32_t beeps = beepCount_.load();
    if (beeps != beepSeen_) {
        beepSeen_ = beeps;
        bool go = beepKind_.load() == 1;
        beepFreq_ = go ? 660.0f : 440.0f;
        beepLength_ = go ? 0.45f : 0.13f;
        beepTime_ = 0.0f;
        beepPhase_ = 0.0f;
    }

    for (int i = 0; i < frames; i++) {
        // Paramètres lissés : un saut de valeur s'entendrait comme un clic.
        speedSmooth_ += (speedTarget - speedSmooth_) * 0.0004f;
        boostSmooth_ += (boostTarget - boostSmooth_) * 0.0003f;
        scrapeSmooth_ += (scrapeTarget - scrapeSmooth_) * 0.002f;

        float ratio = std::clamp(speedSmooth_ / 400.0f, 0.0f, 1.2f);
        // Fondamentale très basse, qui monte peu : c'est un bourdonnement, pas
        // un régime moteur. Une hauteur qui suit la vitesse sonne comme une hélice.
        float f0 = (40.0f + 26.0f * ratio) * (1.0f + 0.07f * boostSmooth_);

        // phase_ : 0 fondamentale, 1 octave désaccordée, 2 quinte, 3 modulation
        // de largeur d'impulsion, 4 pulsation.
        const float freqs[5] = {f0, f0 * 2.006f, f0 * 3.0f, 0.31f, 5.0f + 9.0f * ratio};
        for (int k = 0; k < 5; k++) {
            phase_[k] += freqs[k] * dt;
            if (phase_[k] >= 1.0f) phase_[k] -= 1.0f;
        }
        // Ondes carrées à largeur variable : le timbre creux d'un synthé numérique.
        float duty = 0.5f + 0.28f * std::sin(phase_[3] * kTau);
        float sub = std::sin(phase_[0] * kTau);
        float pulse = phase_[0] < duty ? 1.0f : -1.0f;
        float octave = phase_[1] < 0.5f ? 1.0f : -1.0f;
        float fifth = std::sin(phase_[2] * kTau);
        float engineL = sub * 0.28f + pulse * 0.46f + octave * 0.20f + fifth * 0.16f;
        float engineR = sub * 0.28f + pulse * 0.46f - octave * 0.20f + fifth * 0.16f;

        // Filtre passe-bas sombre, qui s'entrouvre avec la vitesse.
        float cutoff = 260.0f + 620.0f * ratio + 300.0f * boostSmooth_;
        float a = 1.0f - std::exp(-kTau * cutoff * dt);
        float engine[2] = {engineL, engineR};
        float bright[2];
        for (int c = 0; c < 2; c++) {
            lowpass_[c][0] += (engine[c] - lowpass_[c][0]) * a;
            lowpass_[c][1] += (lowpass_[c][0] - lowpass_[c][1]) * a;
            bright[c] = lowpass_[c][0];
            engine[c] = lowpass_[c][1];
        }

        // Grain numérique : une part du signal, prise avant le second filtre,
        // est rééchantillonnée bas et réduite à quelques niveaux, comme un vieux
        // convertisseur. C'est elle qui reste audible sur un haut-parleur de
        // téléphone, qui ne restitue presque rien sous 150 Hz.
        crushPhase_ += (2200.0f + 2600.0f * ratio) * dt;
        if (crushPhase_ >= 1.0f) {
            crushPhase_ -= 1.0f;
            for (int c = 0; c < 2; c++) crushHold_[c] = std::round(bright[c] * 10.0f) / 10.0f;
        }
        float throb = 1.0f + 0.14f * std::sin(phase_[4] * kTau);
        // Le moteur reste sous la musique, qui porte le rythme.
        float engineGain = (0.09f + 0.12f * ratio + 0.06f * boostSmooth_) * throb;

        float beep = 0.0f;
        if (beepTime_ < beepLength_) {
            beepPhase_ += beepFreq_ * dt;
            if (beepPhase_ >= 1.0f) beepPhase_ -= 1.0f;
            float envelope = std::min(1.0f, beepTime_ * 200.0f) *
                             std::min(1.0f, (beepLength_ - beepTime_) * 40.0f);
            // Carré adouci, dans le même registre numérique.
            float wave = 0.6f * std::sin(beepPhase_ * kTau) + (beepPhase_ < 0.5f ? 0.2f : -0.2f);
            beep = wave * envelope * 0.20f;
            beepTime_ += dt;
        }

        // Musique de fin, avec un écho alterné entre les deux oreilles.
        const float dry = jingleSample(dt);
        float jingleOut[2];
        {
            const int other = (jingleEchoAt_ + 6000) % 12000;
            jingleOut[0] = dry + 0.35f * jingleEcho_[0][jingleEchoAt_];
            jingleOut[1] = dry + 0.35f * jingleEcho_[1][other];
            jingleEcho_[0][jingleEchoAt_] = dry + 0.3f * jingleEcho_[0][jingleEchoAt_];
            jingleEcho_[1][other] = dry + 0.3f * jingleEcho_[1][other];
            jingleEchoAt_ = (jingleEchoAt_ + 1) % 12000;
        }

        float effects = 0.0f;
        for (Voice& v : voices_)
            if (v.kind >= 0) effects += voiceSample(v, dt);

        for (int c = 0; c < 2; c++) {
            float n = nextNoise();

            // Vent : souffle grave et discret.
            float windCut = 1.0f - std::exp(-kTau * (220.0f + 520.0f * ratio) * dt);
            windLow_[c] += (n - windLow_[c]) * windCut;
            windBand_[c] += (windLow_[c] - windBand_[c]) * windCut * 0.25f;
            float wind = (windLow_[c] - windBand_[c]) * 0.16f * ratio * ratio;

            // Boost : souffle un peu plus clair, sans sifflement.
            float rushCut = 1.0f - std::exp(-kTau * 1400.0f * dt);
            boostHigh_[c] += (n - boostHigh_[c]) * rushCut;
            float rush = boostHigh_[c] * 0.07f * boostSmooth_;

            // Frottement contre le muret : bruit résonant.
            float scrapeCut = 1.0f - std::exp(-kTau * 1500.0f * dt);
            scrapeLow_[c] += (n - scrapeLow_[c]) * scrapeCut;
            scrapeBand_[c] += (scrapeLow_[c] - scrapeBand_[c]) * scrapeCut * 0.5f;
            float scrape = (scrapeLow_[c] - scrapeBand_[c]) * 0.40f * scrapeSmooth_;

            float body = engine[c] * 0.62f + crushHold_[c] * 0.38f;
            float sample = (body * engineGain + wind + rush) * 0.5f + scrape + beep + jingleOut[c] + effects * 0.6f;
            // Écrêtage doux : la somme ne sature jamais brutalement.
            out[i * 2 + c] = std::tanh(sample * 1.4f) * volume;
        }
    }
}

} // namespace game

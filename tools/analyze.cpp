// Analyse un morceau et écrit de quoi contrôler le résultat : un résumé, le
// morceau avec un clic sur chaque temps, et une image annotée.
#include "music/analysis.h"
#include "music/decode.h"

#include <stb_image_write.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

bool writeWav(const std::string& path, const std::vector<float>& samples, int channels, int rate) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const uint32_t bytes = uint32_t(samples.size() * 2);
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 4, 1, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 8, 1, f);
    u32(16); u16(1); u16(uint16_t(channels)); u32(uint32_t(rate));
    u32(uint32_t(rate * channels * 2)); u16(uint16_t(channels * 2)); u16(16);
    std::fwrite("data", 4, 1, f); u32(bytes);
    for (float s : samples) {
        const int16_t v = int16_t(std::lround(std::clamp(s, -1.0f, 1.0f) * 32767.0f));
        std::fwrite(&v, 2, 1, f);
    }
    std::fclose(f);
    return true;
}

struct Image {
    int w, h;
    std::vector<unsigned char> px;
    Image(int w_, int h_) : w(w_), h(h_), px(size_t(w_) * size_t(h_) * 3, 0) {}
    void set(int x, int y, unsigned char r, unsigned char g, unsigned char b, float a = 1.0f) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        unsigned char* p = &px[(size_t(y) * size_t(w) + size_t(x)) * 3];
        p[0] = (unsigned char)(p[0] + (r - p[0]) * a);
        p[1] = (unsigned char)(p[1] + (g - p[1]) * a);
        p[2] = (unsigned char)(p[2] + (b - p[2]) * a);
    }
    void vline(int x, int y0, int y1, unsigned char r, unsigned char g, unsigned char b, float a = 1.0f) {
        for (int y = y0; y < y1; ++y) set(x, y, r, g, b, a);
    }
};

// De haut en bas : sections et temps, spectrogramme, attaques par bande
// (aigus, médiums, graves), courbe d'énergie.
bool writeImage(const std::string& path, const music::Partition& p, const music::AnalysisTrace& trace, float from, float to) {
    const int bands = music::AnalysisTrace::kBands;
    const int width = 2000, rowTop = 28, rowSpec = bands * 5, rowOnset = 26, rowEnergy = 90;
    const int ySpec = rowTop, yOnset = ySpec + rowSpec + 4, yEnergy = yOnset + 3 * rowOnset + 4;
    Image img(width, yEnergy + rowEnergy);
    const float span = std::max(0.001f, to - from);
    auto xOf = [&](float t) { return int((t - from) / span * float(width)); };

    float top = 1e-6f;
    for (float v : trace.spectrum) top = std::max(top, v);
    for (int x = 0; x < width; ++x) {
        const int a = std::clamp(int((from - trace.frameOffset + span * float(x) / float(width)) * trace.frameRate), 0, std::max(0, trace.frames - 1));
        const int b = std::clamp(int((from - trace.frameOffset + span * float(x + 1) / float(width)) * trace.frameRate), a + 1, std::max(a + 1, trace.frames));
        for (int k = 0; k < bands; ++k) {
            float v = 0.0f;
            for (int t = a; t < b && t < trace.frames; ++t) v = std::max(v, trace.spectrum[size_t(t) * size_t(bands) + size_t(k)]);
            v = std::pow(std::clamp(v / top, 0.0f, 1.0f), 1.6f);
            const unsigned char r = (unsigned char)(255 * std::clamp(v * 2.2f - 0.9f, 0.0f, 1.0f));
            const unsigned char g = (unsigned char)(255 * std::clamp(v * 1.6f - 0.2f, 0.0f, 1.0f));
            const unsigned char bl = (unsigned char)(255 * std::clamp(v * 2.5f, 0.0f, 1.0f) * (1.0f - 0.5f * v));
            for (int y = 0; y < 5; ++y) img.set(x, ySpec + (bands - 1 - k) * 5 + y, r, g, bl);
        }
        const float e = p.energyAt(from + span * (float(x) + 0.5f) / float(width));
        const int level = int(e * float(rowEnergy - 2));
        img.vline(x, yEnergy + rowEnergy - 1 - level, yEnergy + rowEnergy, 60, 200, 255);
    }
    static const unsigned char color[3][3] = {{255, 90, 60}, {90, 230, 120}, {120, 170, 255}};
    for (const music::Onset& o : p.onsets) {
        if (o.time < from || o.time > to) continue;
        const int row = 2 - o.band;
        const int height = 4 + int(o.strength * float(rowOnset - 6));
        const int y1 = yOnset + (row + 1) * rowOnset - 1;
        img.vline(xOf(o.time), y1 - height, y1, color[o.band][0], color[o.band][1], color[o.band][2]);
    }
    for (const music::Beat& b : p.beats) {
        if (b.time < from || b.time > to) continue;
        const int x = xOf(b.time);
        if (b.bar == 0) {
            img.vline(x, 8, rowTop, 255, 255, 255);
            img.vline(x, ySpec, ySpec + rowSpec, 255, 255, 255, 0.35f);
        } else {
            img.vline(x, 18, rowTop, 150, 150, 150);
        }
    }
    for (const music::Section& s : p.sections) {
        const int x = xOf(s.start);
        for (int dx = 0; dx < 2; ++dx) img.vline(x + dx, 0, img.h, 255, 60, 200);
        const int x1 = std::min(width, xOf(s.end)) - 2;
        const unsigned char v = (unsigned char)(60 + 195 * s.energy);
        for (int xx = std::max(0, x + 3); xx < x1; ++xx) img.vline(xx, 0, 6, v, (unsigned char)(v / 3), v);
    }
    return stbi_write_png(path.c_str(), img.w, img.h, 3, img.px.data(), img.w * 3) != 0;
}

} // namespace

int main(int argc, char** argv) {
    std::string input, clicks, image;
    float from = 0.0f, to = -1.0f;
    bool listBeats = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--clicks" && i + 1 < argc) clicks = argv[++i];
        else if (a == "--image" && i + 1 < argc) image = argv[++i];
        else if (a == "--from" && i + 1 < argc) from = float(std::atof(argv[++i]));
        else if (a == "--to" && i + 1 < argc) to = float(std::atof(argv[++i]));
        else if (a == "--beats") listBeats = true;
        else input = a;
    }
    if (input.empty()) {
        std::fprintf(stderr, "usage : analyze morceau.ogg|wav [--clicks sortie.wav] [--image sortie.png] [--from s] [--to s] [--beats]\n");
        return 2;
    }
    music::Audio audio;
    std::string error;
    if (!music::decodeFile(input, audio, error)) {
        std::fprintf(stderr, "%s : %s\n", input.c_str(), error.c_str());
        return 1;
    }
    const std::vector<float> mono = audio.mono();
    music::AnalysisTrace trace;
    const auto start = std::chrono::steady_clock::now();
    const music::Partition p = music::analyze(mono.data(), mono.size(), audio.sampleRate, &trace);
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    int perBand[3] = {0, 0, 0};
    for (const music::Onset& o : p.onsets) ++perBand[o.band];
    std::printf("%s\n", input.c_str());
    std::printf("  durée %.1f s, analyse en %.2f s\n", p.duration, elapsed);
    std::printf("  tempo %.1f BPM, confiance %.2f, %zu temps\n", p.tempo, p.confidence, p.beats.size());
    std::printf("  attaques : %d graves, %d médiums, %d aigus\n", perBand[0], perBand[1], perBand[2]);
    std::printf("  %zu sections\n", p.sections.size());
    for (const music::Section& s : p.sections)
        std::printf("    %6.1f à %6.1f s  énergie %.2f  densité %.2f  brillance %.2f\n", s.start, s.end, s.energy, s.density, s.brightness);
    if (listBeats)
        for (const music::Beat& b : p.beats) std::printf("    temps %.3f  force %.2f  position %d\n", b.time, b.strength, b.bar);

    if (!clicks.empty()) {
        std::vector<float> out(audio.samples);
        for (float& s : out) s *= 0.6f;
        const int length = audio.sampleRate / 25;
        for (const music::Beat& b : p.beats) {
            const size_t at = size_t(double(b.time) * audio.sampleRate);
            const float pitch = b.bar == 0 ? 1760.0f : 880.0f, gain = b.bar == 0 ? 0.55f : 0.35f;
            for (int i = 0; i < length && at + size_t(i) < audio.frames(); ++i) {
                const float t = float(i) / float(audio.sampleRate);
                const float v = gain * std::sin(6.2831853f * pitch * t) * std::exp(-t * 90.0f);
                for (int c = 0; c < audio.channels; ++c) out[(at + size_t(i)) * size_t(audio.channels) + size_t(c)] += v;
            }
        }
        if (!writeWav(clicks, out, audio.channels, audio.sampleRate)) {
            std::fprintf(stderr, "écriture impossible : %s\n", clicks.c_str());
            return 1;
        }
    }
    if (!image.empty() && !writeImage(image, p, trace, from, to > from ? to : p.duration)) {
        std::fprintf(stderr, "écriture impossible : %s\n", image.c_str());
        return 1;
    }
    return 0;
}

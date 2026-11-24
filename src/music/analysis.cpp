#include "music/analysis.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace music {

namespace {

constexpr int kFft = 1024;
constexpr int kBands = AnalysisTrace::kBands;
constexpr float kPi = 3.14159265358979f;

// Transformée de Fourier rapide en place, taille puissance de deux.
void fft(std::vector<std::complex<float>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const float angle = -2.0f * kPi / float(len);
        const std::complex<float> step(std::cos(angle), std::sin(angle));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (size_t k = 0; k < len / 2; ++k) {
                const std::complex<float> u = a[i + k];
                const std::complex<float> v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= step;
            }
        }
    }
}

float percentile(std::vector<float> values, float p) {
    if (values.empty()) return 0.0f;
    const size_t k = std::min(values.size() - 1, size_t(p * float(values.size() - 1) + 0.5f));
    std::nth_element(values.begin(), values.begin() + long(k), values.end());
    return values[k];
}

// Moyenne glissante centrée, de demi-largeur donnée.
std::vector<float> movingMean(const std::vector<float>& in, int half) {
    const int n = int(in.size());
    std::vector<double> sum(size_t(n) + 1, 0.0);
    for (int i = 0; i < n; ++i) sum[size_t(i) + 1] = sum[size_t(i)] + in[size_t(i)];
    std::vector<float> out(size_t(n), 0.0f);
    for (int i = 0; i < n; ++i) {
        const int a = std::max(0, i - half), b = std::min(n, i + half + 1);
        out[size_t(i)] = float((sum[size_t(b)] - sum[size_t(a)]) / double(b - a));
    }
    return out;
}

float peakAround(const std::vector<float>& v, int center, int half) {
    float best = 0.0f;
    for (int i = std::max(0, center - half); i <= std::min(int(v.size()) - 1, center + half); ++i)
        best = std::max(best, v[size_t(i)]);
    return best;
}

// Centre et réduit une série : moyenne nulle, écart type unité.
void standardize(std::vector<float>& v) {
    if (v.empty()) return;
    double mean = 0.0, var = 0.0;
    for (float x : v) mean += x;
    mean /= double(v.size());
    for (float x : v) var += (x - mean) * (x - mean);
    const float sd = float(std::sqrt(var / double(v.size())));
    for (float& x : v) x = sd > 1e-9f ? float((x - mean) / sd) : 0.0f;
}

struct Frames {
    float rate = 0.0f;
    // Une attaque marque le flux quand elle a parcouru les trois quarts de la
    // fenêtre, valeur mesurée sur des signaux synthétiques. L'instant d'une
    // trame est donc son début plus ce décalage.
    float offset = 0.0f;
    int count = 0;
    float time(int frame) const { return float(frame) / rate + offset; }
    std::vector<float> bands;   // count * kBands, log de l'amplitude
    std::vector<float> flux[kBandCount];
    std::vector<float> total;   // somme des trois flux
    std::vector<float> power;   // carré moyen du signal
    std::vector<float> bass;    // amplitude sous 120 Hz
    std::vector<float> centroid; // 0 à 1 sur l'échelle des bandes
};

Frames computeFrames(const float* mono, size_t count, int sampleRate) {
    // Réduction par un facteur entier vers 16 à 32 kHz : les temps se lisent
    // sous 10 kHz, et l'analyse coûte deux à quatre fois moins.
    int factor = 1;
    while (sampleRate / (factor + 1) >= 22050) ++factor;
    const int rate = sampleRate / factor;
    std::vector<float> signal(count / size_t(factor));
    for (size_t i = 0; i < signal.size(); ++i) {
        float sum = 0.0f;
        for (int k = 0; k < factor; ++k) sum += mono[i * size_t(factor) + size_t(k)];
        signal[i] = sum / float(factor);
    }

    Frames f;
    const int hop = std::max(1, int(std::lround(rate / 100.0)));
    f.rate = float(rate) / float(hop);
    f.offset = 0.75f * float(kFft) / float(rate);
    if (signal.size() < size_t(kFft)) return f;
    f.count = int((signal.size() - kFft) / size_t(hop)) + 1;

    std::vector<float> window(kFft);
    float windowSum = 0.0f;
    for (int i = 0; i < kFft; ++i) {
        window[size_t(i)] = 0.5f - 0.5f * std::cos(2.0f * kPi * float(i) / float(kFft));
        windowSum += window[size_t(i)];
    }

    // Bandes espacées en logarithme de la fréquence, de 40 Hz à la moitié de
    // la fréquence d'échantillonnage.
    int edge[kBands + 1];
    const float lowHz = 40.0f, highHz = float(rate) * 0.5f;
    for (int b = 0; b <= kBands; ++b) {
        const float hz = lowHz * std::pow(highHz / lowHz, float(b) / float(kBands));
        edge[b] = std::clamp(int(std::lround(hz * kFft / float(rate))), 1, kFft / 2);
        if (b > 0 && edge[b] <= edge[b - 1]) edge[b] = std::min(kFft / 2, edge[b - 1] + 1);
    }
    int group[kBands];
    for (int b = 0; b < kBands; ++b) {
        const float hz = float(edge[b]) * float(rate) / kFft;
        group[b] = hz < 200.0f ? kLow : hz < 2500.0f ? kMid : kHigh;
    }

    f.bands.assign(size_t(f.count) * kBands, 0.0f);
    for (auto& v : f.flux) v.assign(size_t(f.count), 0.0f);
    f.total.assign(size_t(f.count), 0.0f);
    f.power.assign(size_t(f.count), 0.0f);
    f.centroid.assign(size_t(f.count), 0.0f);
    f.bass.assign(size_t(f.count), 0.0f);

    std::vector<std::complex<float>> buffer(kFft);
    for (int t = 0; t < f.count; ++t) {
        const float* in = signal.data() + size_t(t) * size_t(hop);
        double power = 0.0;
        for (int i = 0; i < kFft; ++i) {
            buffer[size_t(i)] = std::complex<float>(in[i] * window[size_t(i)], 0.0f);
            power += double(in[i]) * in[i];
        }
        f.power[size_t(t)] = float(power / kFft);
        fft(buffer);

        float* row = f.bands.data() + size_t(t) * kBands;
        const float* prev = t > 0 ? row - kBands : nullptr;
        float weight = 0.0f, moment = 0.0f;
        for (int b = 0; b < kBands; ++b) {
            float amp = 0.0f;
            for (int k = edge[b]; k < std::max(edge[b] + 1, edge[b + 1]); ++k)
                amp += std::abs(buffer[size_t(std::min(k, kFft / 2))]);
            amp *= 2.0f / windowSum;
            row[b] = std::log1p(1000.0f * amp);
            weight += amp;
            if (float(edge[b + 1]) * float(rate) / kFft <= 120.0f) f.bass[size_t(t)] += amp;
            moment += amp * float(b);
            if (prev) f.flux[group[b]][size_t(t)] += std::max(0.0f, row[b] - prev[b]);
        }
        f.centroid[size_t(t)] = weight > 1e-9f ? moment / weight / float(kBands - 1) : 0.0f;
        for (int g = 0; g < kBandCount; ++g) f.total[size_t(t)] += f.flux[g][size_t(t)];
    }
    return f;
}

// Courbe d'attaques pour la recherche du tempo : flux total moins sa moyenne
// locale, partie positive, réduit.
std::vector<float> onsetEnvelope(const Frames& f) {
    std::vector<float> mean = movingMean(f.total, int(f.rate * 0.5f));
    std::vector<float> env(f.total.size());
    for (size_t i = 0; i < env.size(); ++i) env[i] = std::max(0.0f, f.total[i] - mean[i]);
    double var = 0.0;
    for (float x : env) var += double(x) * x;
    const float sd = env.empty() ? 0.0f : float(std::sqrt(var / double(env.size())));
    for (float& x : env) x = sd > 1e-9f ? x / sd : 0.0f;
    return env;
}

constexpr float kMinBpm = 70.0f, kMaxBpm = 190.0f, kBpmStep = 0.5f;

// Score de chaque tempo candidat sur une portion de la courbe d'attaques.
std::vector<float> tempoScores(const float* env, int n, float rate) {
    const int candidates = int((kMaxBpm - kMinBpm) / kBpmStep) + 1;
    std::vector<float> scores(size_t(candidates), 0.0f);
    const int maxLag = std::min(n - 1, int(rate * 60.0f / kMinBpm * 4.0f) + 2);
    if (maxLag < 8) return scores;
    std::vector<float> ac(size_t(maxLag) + 1, 0.0f);
    for (int lag = 1; lag <= maxLag; ++lag) {
        double sum = 0.0;
        for (int t = 0; t + lag < n; ++t) sum += double(env[t]) * env[t + lag];
        ac[size_t(lag)] = float(sum / double(n - lag));
    }
    auto at = [&](float lag) {
        const int i = int(lag);
        if (i + 1 > maxLag) return 0.0f;
        const float frac = lag - float(i);
        return ac[size_t(i)] * (1.0f - frac) + ac[size_t(i) + 1] * frac;
    };
    for (int c = 0; c < candidates; ++c) {
        const float bpm = kMinBpm + kBpmStep * float(c);
        const float period = rate * 60.0f / bpm;
        // Un vrai temps se répète aussi à deux, trois et quatre périodes. Les
        // deux tiers de la période servent de contre-épreuve : s'ils
        // répondent aussi, le vrai temps est une fois et demie plus rapide.
        const float comb = at(period) + 0.5f * at(2 * period) + 0.33f * at(3 * period) + 0.25f * at(4 * period)
                         - 0.5f * at(period * 2.0f / 3.0f);
        // Entre un tempo et son double, on préfère le plus proche de 130.
        const float octaves = std::log2(bpm / 130.0f);
        scores[size_t(c)] = std::max(0.0f, comb) * std::exp(-0.5f * octaves * octaves / (0.8f * 0.8f));
    }
    return scores;
}

// Période d'un temps à chaque trame. Le tempo est cherché par fenêtres de
// douze secondes, puis on retient la suite de tempos la plus cohérente : un
// morceau peut changer d'allure, mais pas à chaque fenêtre. Vide si la courbe
// est plate.
std::vector<float> findPeriods(const std::vector<float>& env, float rate) {
    const int n = int(env.size());
    const int window = int(rate * 12.0f), hop = int(rate * 4.0f);
    const int candidates = int((kMaxBpm - kMinBpm) / kBpmStep) + 1;
    std::vector<std::vector<float>> scores;
    std::vector<int> centers;
    float best = 0.0f;
    for (int start = 0; start == 0 || start + window / 2 < n; start += hop) {
        const int length = std::min(window, n - start);
        if (length < int(rate * 4.0f)) break;
        scores.push_back(tempoScores(env.data() + start, length, rate));
        centers.push_back(start + length / 2);
        for (float v : scores.back()) best = std::max(best, v);
    }
    if (scores.empty() || best < 1e-4f) return {};
    const int windows = int(scores.size());
    // Chaque fenêtre est ramenée à son meilleur score, sauf les fenêtres
    // presque muettes, qui laissent décider leurs voisines.
    for (auto& row : scores) {
        float top = 0.0f;
        for (float v : row) top = std::max(top, v);
        const float scale = top > best * 0.02f ? 1.0f / top : 0.0f;
        for (float& v : row) v *= scale;
    }
    // Chemin le plus cohérent : changer de tempo coûte 1,5 par octave.
    std::vector<std::vector<float>> total(size_t(windows), std::vector<float>(size_t(candidates), 0.0f));
    std::vector<std::vector<int>> from(size_t(windows), std::vector<int>(size_t(candidates), 0));
    total[0] = scores[0];
    for (int w = 1; w < windows; ++w) {
        for (int c = 0; c < candidates; ++c) {
            const float bpm = kMinBpm + kBpmStep * float(c);
            float bestPrev = -1e9f;
            int bestIndex = c;
            for (int k = 0; k < candidates; ++k) {
                const float other = kMinBpm + kBpmStep * float(k);
                const float cost = 1.5f * std::fabs(std::log2(bpm / other));
                const float v = total[size_t(w - 1)][size_t(k)] - cost;
                if (v > bestPrev) {
                    bestPrev = v;
                    bestIndex = k;
                }
            }
            total[size_t(w)][size_t(c)] = scores[size_t(w)][size_t(c)] + bestPrev;
            from[size_t(w)][size_t(c)] = bestIndex;
        }
    }
    std::vector<float> periodOf(size_t(windows), 0.0f);
    int c = int(std::max_element(total.back().begin(), total.back().end()) - total.back().begin());
    for (int w = windows - 1; w >= 0; --w) {
        periodOf[size_t(w)] = rate * 60.0f / (kMinBpm + kBpmStep * float(c));
        c = from[size_t(w)][size_t(c)];
    }
    // Période de chaque trame, interpolée entre les centres des fenêtres.
    std::vector<float> periods(size_t(n), periodOf[0]);
    int w = 0;
    for (int t = 0; t < n; ++t) {
        while (w + 1 < windows && t >= centers[size_t(w + 1)]) ++w;
        if (t <= centers[0] || w + 1 >= windows) {
            periods[size_t(t)] = periodOf[size_t(t <= centers[0] ? 0 : windows - 1)];
        } else {
            const float u = float(t - centers[size_t(w)]) / float(centers[size_t(w + 1)] - centers[size_t(w)]);
            periods[size_t(t)] = periodOf[size_t(w)] + (periodOf[size_t(w + 1)] - periodOf[size_t(w)]) * u;
        }
    }
    return periods;
}

// Place les temps par programmation dynamique : chaque temps cherche le
// meilleur prédécesseur à une période environ, et paie tout écart.
std::vector<int> trackBeats(const std::vector<float>& env, const std::vector<float>& periods) {
    const int n = int(env.size());
    const float tightness = 100.0f;
    std::vector<float> score(size_t(n), 0.0f);
    std::vector<int> back(size_t(n), -1);
    for (int t = 0; t < n; ++t) {
        const float period = periods[size_t(t)];
        const int from = int(std::lround(period * 2.0f)), to = std::max(1, int(std::lround(period * 0.5f)));
        float best = 0.0f;
        int bestIndex = -1;
        for (int d = to; d <= from && t - d >= 0; ++d) {
            const float l = std::log(float(d) / period);
            const float s = score[size_t(t - d)] - tightness * l * l;
            if (bestIndex < 0 || s > best) {
                best = s;
                bestIndex = t - d;
            }
        }
        score[size_t(t)] = env[size_t(t)] + (bestIndex >= 0 ? best : 0.0f);
        back[size_t(t)] = bestIndex >= 0 && best > 0.0f ? bestIndex : -1;
    }
    int last = n - 1;
    for (int t = std::max(0, n - int(periods.back()) - 1); t < n; ++t)
        if (score[size_t(t)] > score[size_t(last)]) last = t;
    std::vector<int> beats;
    for (int t = last; t >= 0; t = back[size_t(t)]) beats.push_back(t);
    std::reverse(beats.begin(), beats.end());
    // La grille se prolonge avant le premier temps trouvé, jusqu'au début.
    if (!beats.empty()) {
        const float period = periods[size_t(beats.front())];
        std::vector<int> head;
        for (float t = float(beats.front()) - period; t >= 0.0f; t -= period) head.push_back(int(std::lround(t)));
        std::reverse(head.begin(), head.end());
        beats.insert(beats.begin(), head.begin(), head.end());
    }
    return beats;
}

// Moyenne des bandes entre deux trames.
void meanBands(const Frames& f, int a, int b, float* out) {
    a = std::clamp(a, 0, f.count - 1);
    b = std::clamp(b, a + 1, f.count);
    std::fill(out, out + kBands, 0.0f);
    for (int t = a; t < b; ++t)
        for (int k = 0; k < kBands; ++k) out[k] += f.bands[size_t(t) * kBands + size_t(k)];
    for (int k = 0; k < kBands; ++k) out[k] /= float(b - a);
}

// Premier temps de la mesure : celui où l'extrême grave est le plus fort et où
// le timbre change le plus. Sous 120 Hz on entend la grosse caisse et la
// basse, pas la caisse claire des temps 2 et 4.
int findDownbeat(const Frames& f, const std::vector<int>& beats) {
    const int n = int(beats.size());
    if (n < 8) return 0;
    std::vector<float> low(size_t(n), 0.0f), change(size_t(n), 0.0f);
    float before[kBands], after[kBands];
    for (int i = 0; i < n; ++i) {
        low[size_t(i)] = peakAround(f.bass, beats[size_t(i)] + 3, 5);
        if (i > 0 && i + 1 < n) {
            meanBands(f, beats[size_t(i - 1)], beats[size_t(i)], before);
            meanBands(f, beats[size_t(i)], beats[size_t(i + 1)], after);
            float d = 0.0f;
            for (int k = 0; k < kBands; ++k) d += (after[k] - before[k]) * (after[k] - before[k]);
            change[size_t(i)] = std::sqrt(d);
        }
    }
    standardize(low);
    standardize(change);
    int best = 0;
    float bestScore = -1e9f;
    for (int phase = 0; phase < Partition::kBeatsPerBar; ++phase) {
        float sum = 0.0f;
        int count = 0;
        for (int i = phase; i < n; i += Partition::kBeatsPerBar, ++count) sum += low[size_t(i)] + change[size_t(i)];
        const float score = count ? sum / float(count) : -1e9f;
        if (score > bestScore) {
            bestScore = score;
            best = phase;
        }
    }
    return best;
}

std::vector<Onset> pickOnsets(const Frames& f) {
    std::vector<Onset> onsets;
    for (int band = 0; band < kBandCount; ++band) {
        const std::vector<float>& flux = f.flux[band];
        const std::vector<float> mean = movingMean(flux, int(f.rate * 0.5f));
        const float top = percentile(flux, 0.98f);
        if (top < 1e-4f) continue;
        int lastFrame = -100;
        std::vector<float> picked;
        for (int t = 1; t + 1 < f.count; ++t) {
            const float v = flux[size_t(t)];
            if (v < mean[size_t(t)] * 1.6f + top * 0.12f) continue;
            if (v < peakAround(flux, t, 3)) continue;
            if (t - lastFrame < 6) continue;
            lastFrame = t;
            onsets.push_back({f.time(t), v, band});
            picked.push_back(v);
        }
        // Force relative aux autres attaques de la bande : 1 pour les 10 %
        // les plus marquées.
        const float strong = std::max(1e-6f, percentile(picked, 0.90f));
        for (size_t i = onsets.size() - picked.size(); i < onsets.size(); ++i)
            onsets[i].strength = std::min(1.0f, onsets[i].strength / strong);
    }
    std::sort(onsets.begin(), onsets.end(), [](const Onset& a, const Onset& b) { return a.time < b.time; });
    return onsets;
}

std::vector<float> energyCurve(const Frames& f, float duration) {
    const std::vector<float> smooth = movingMean(f.power, int(f.rate * 1.0f));
    std::vector<float> db(smooth.size());
    for (size_t i = 0; i < db.size(); ++i) db[i] = 10.0f * std::log10(smooth[i] + 1e-10f);
    const float high = percentile(db, 0.95f);
    const float low = std::max(percentile(db, 0.10f), high - 30.0f);
    const int points = std::max(1, int(duration * Partition::kEnergyRate) + 1);
    std::vector<float> out(size_t(points), 0.0f);
    if (db.empty() || high - low < 1.0f || high < -70.0f) {
        // Niveau constant ou silence : pas de relief à en tirer.
        std::fill(out.begin(), out.end(), high < -70.0f ? 0.0f : 0.5f);
        return out;
    }
    for (int i = 0; i < points; ++i) {
        const int t = std::min(f.count - 1, int(float(i) / Partition::kEnergyRate * f.rate));
        out[size_t(i)] = std::clamp((db[size_t(t)] - low) / (high - low), 0.0f, 1.0f);
    }
    return out;
}

// Frontières de sections, en indices de mesure. Une frontière est un pic de
// nouveauté : les mesures d'avant se ressemblent, celles d'après aussi, et
// les deux groupes diffèrent.
std::vector<int> findBoundaries(const Frames& f, const std::vector<int>& barFrames) {
    const int bars = int(barFrames.size()) - 1;
    std::vector<int> bounds;
    if (bars < 8) return bounds;
    std::vector<float> feat(size_t(bars) * kBands);
    std::vector<float> mean(kBands, 0.0f);
    for (int b = 0; b < bars; ++b) {
        meanBands(f, barFrames[size_t(b)], barFrames[size_t(b + 1)], feat.data() + size_t(b) * kBands);
        for (int k = 0; k < kBands; ++k) mean[size_t(k)] += feat[size_t(b) * kBands + size_t(k)] / float(bars);
    }
    for (int b = 0; b < bars; ++b) {
        float norm = 0.0f;
        float* row = feat.data() + size_t(b) * kBands;
        for (int k = 0; k < kBands; ++k) {
            row[k] -= mean[size_t(k)];
            norm += row[k] * row[k];
        }
        norm = std::sqrt(norm) + 1e-6f;
        for (int k = 0; k < kBands; ++k) row[k] /= norm;
    }
    auto similarity = [&](int a, int b) {
        float s = 0.0f;
        for (int k = 0; k < kBands; ++k) s += feat[size_t(a) * kBands + size_t(k)] * feat[size_t(b) * kBands + size_t(k)];
        return s;
    };
    const int half = 4;
    std::vector<float> novelty(size_t(bars), 0.0f);
    for (int b = half; b + half <= bars; ++b) {
        float sum = 0.0f;
        for (int i = -half; i < half; ++i)
            for (int j = -half; j < half; ++j) {
                const float sign = (i < 0) == (j < 0) ? 1.0f : -1.0f;
                const float di = float(i) + 0.5f, dj = float(j) + 0.5f;
                sum += sign * similarity(b + i, b + j) * std::exp(-(di * di + dj * dj) / 18.0f);
            }
        novelty[size_t(b)] = sum;
    }
    double avg = 0.0, var = 0.0;
    for (float v : novelty) avg += v;
    avg /= double(bars);
    for (float v : novelty) var += (v - avg) * (v - avg);
    const float threshold = float(avg + 0.8 * std::sqrt(var / double(bars)));
    int last = -100;
    for (int b = half; b + half <= bars; ++b) {
        const float v = novelty[size_t(b)];
        if (v < threshold || v < peakAround(novelty, b, 3)) continue;
        if (b - last < 4) continue;
        last = b;
        bounds.push_back(b);
    }
    return bounds;
}

// Hargne du morceau, de 0 à 1 : niveau sonore absolu, part d'aigus et densité
// d'attaques. Un metal saturé y monte à 1, une ballade reste près de 0. À la
// différence de l'énergie, elle n'est pas rapportée au morceau lui-même : elle
// compare les morceaux entre eux.
std::vector<float> driveCurve(const Frames& f, const std::vector<Onset>& onsets, float duration) {
    const int points = std::max(1, int(duration * Partition::kEnergyRate) + 1);
    std::vector<float> out(size_t(points), 0.0f);
    if (f.count < 1) return out;
    const std::vector<float> power = movingMean(f.power, int(f.rate * 2.0f));
    // Part d'aigus : amplitude au-dessus de 2 kHz environ, sur le total.
    std::vector<float> highs(size_t(f.count), 0.0f);
    for (int t = 0; t < f.count; ++t) {
        float high = 0.0f, all = 1e-9f;
        for (int b = 0; b < kBands; ++b) {
            const float amp = std::expm1(f.bands[size_t(t) * kBands + size_t(b)]);
            all += amp;
            if (b >= 30) high += amp;
        }
        highs[size_t(t)] = high / all;
    }
    const std::vector<float> highShare = movingMean(highs, int(f.rate * 2.0f));
    size_t first = 0;
    for (int i = 0; i < points; ++i) {
        const float time = float(i) / Partition::kEnergyRate;
        const int t = std::clamp(int((time - f.offset) * f.rate), 0, f.count - 1);
        const float level = std::clamp((10.0f * std::log10(power[size_t(t)] + 1e-10f) + 24.0f) / 16.0f, 0.0f, 1.0f);
        while (first < onsets.size() && onsets[first].time < time - 2.0f) ++first;
        int hits = 0;
        for (size_t k = first; k < onsets.size() && onsets[k].time < time + 2.0f; ++k) ++hits;
        const float rate = std::min(1.4f, float(hits) / 4.0f / 15.0f);
        out[size_t(i)] = std::clamp(level * (0.5f + highShare[size_t(t)]) * rate / 1.15f, 0.0f, 1.0f);
    }
    return out;
}

// Spectre réduit à huit bandes. Chaque bande est ramenée entre son niveau de
// fond et ses pointes, pour que toutes bougent autant.
std::vector<float> reducedSpectrum(const Frames& f, float duration) {
    const int bands = Partition::kSpectrumBands, per = kBands / bands;
    const int rows = std::max(1, int(duration * Partition::kSpectrumRate) + 1);
    std::vector<float> out(size_t(rows) * size_t(bands), 0.0f);
    if (f.count < 1) return out;
    for (int r = 0; r < rows; ++r) {
        const int a = std::clamp(int((float(r) / Partition::kSpectrumRate - f.offset) * f.rate), 0, f.count - 1);
        const int b = std::clamp(int((float(r + 1) / Partition::kSpectrumRate - f.offset) * f.rate), a + 1, f.count);
        for (int k = 0; k < bands; ++k) {
            float best = 0.0f;
            for (int t = a; t < b; ++t) {
                float sum = 0.0f;
                for (int j = 0; j < per; ++j) sum += f.bands[size_t(t) * kBands + size_t(k * per + j)];
                best = std::max(best, sum / float(per));
            }
            out[size_t(r) * size_t(bands) + size_t(k)] = best;
        }
    }
    for (int k = 0; k < bands; ++k) {
        std::vector<float> column(static_cast<size_t>(rows));
        for (int r = 0; r < rows; ++r) column[size_t(r)] = out[size_t(r) * size_t(bands) + size_t(k)];
        const float low = percentile(column, 0.30f), high = percentile(column, 0.98f);
        for (int r = 0; r < rows; ++r) {
            float& v = out[size_t(r) * size_t(bands) + size_t(k)];
            v = high - low > 1e-4f ? std::clamp((v - low) / (high - low), 0.0f, 1.0f) : 0.0f;
        }
    }
    return out;
}

} // namespace

void Partition::spectrumAt(float time, float* out) const {
    const size_t rows = spectrum.size() / size_t(kSpectrumBands);
    for (int k = 0; k < kSpectrumBands; ++k) out[k] = 0.0f;
    if (rows == 0 || time < 0.0f) return;
    const size_t r = size_t(time * kSpectrumRate);
    if (r >= rows) return;
    for (int k = 0; k < kSpectrumBands; ++k) out[k] = spectrum[r * size_t(kSpectrumBands) + size_t(k)];
}

float Partition::driveAt(float time) const {
    if (drive.empty()) return 0.0f;
    const float x = std::clamp(time * kEnergyRate, 0.0f, float(drive.size() - 1));
    const size_t i = size_t(x);
    const size_t j = std::min(drive.size() - 1, i + 1);
    return drive[i] + (drive[j] - drive[i]) * (x - float(i));
}

float Partition::energyAt(float time) const {
    if (energy.empty()) return 0.0f;
    const float x = std::clamp(time * kEnergyRate, 0.0f, float(energy.size() - 1));
    const size_t i = size_t(x);
    const size_t j = std::min(energy.size() - 1, i + 1);
    return energy[i] + (energy[j] - energy[i]) * (x - float(i));
}

Partition analyze(const float* mono, size_t count, int sampleRate, AnalysisTrace* trace) {
    Partition p;
    if (!mono || count == 0 || sampleRate <= 0) return p;
    p.duration = float(double(count) / double(sampleRate));

    const Frames f = computeFrames(mono, count, sampleRate);
    if (trace) {
        trace->frameRate = f.rate;
        trace->frameOffset = f.offset;
        trace->frames = f.count;
        trace->spectrum = f.bands;
        for (int g = 0; g < kBandCount; ++g) trace->flux[g] = f.flux[g];
    }
    p.energy = energyCurve(f, p.duration);
    p.spectrum = reducedSpectrum(f, p.duration);
    if (f.count < 8) {
        p.sections.push_back({0.0f, p.duration, 0.0f, 0.0f, 0.0f});
        return p;
    }
    p.onsets = pickOnsets(f);
    p.drive = driveCurve(f, p.onsets, p.duration);

    const std::vector<float> env = onsetEnvelope(f);
    const std::vector<float> periods = findPeriods(env, f.rate);
    std::vector<int> beatFrames;
    if (!periods.empty()) beatFrames = trackBeats(env, periods);
    // Un morceau hargneux joué à demi-tempo se ressent au tempo double : on
    // y ajoute un temps entre deux.
    if (beatFrames.size() >= 4) {
        std::vector<int> doubled;
        for (size_t i = 0; i < beatFrames.size(); ++i) {
            doubled.push_back(beatFrames[i]);
            if (i + 1 >= beatFrames.size()) break;
            const float gap = float(beatFrames[i + 1] - beatFrames[i]) / f.rate;
            const float time = f.time((beatFrames[i] + beatFrames[i + 1]) / 2);
            const float hargne = p.drive[std::min(p.drive.size() - 1, size_t(std::max(0.0f, time) * Partition::kEnergyRate))];
            if (gap > 60.0f / 105.0f && hargne > 0.65f) doubled.push_back((beatFrames[i] + beatFrames[i + 1]) / 2);
        }
        beatFrames.swap(doubled);
    }

    if (beatFrames.size() >= 4) {
        std::vector<float> gaps;
        for (size_t i = 1; i < beatFrames.size(); ++i) gaps.push_back(float(beatFrames[i] - beatFrames[i - 1]));
        // La moyenne des écarts centraux est plus fine que la médiane, que la
        // trame de 10 ms arrondit.
        std::sort(gaps.begin(), gaps.end());
        double sum = 0.0;
        const size_t a = gaps.size() / 4, b = gaps.size() - gaps.size() / 4;
        for (size_t i = a; i < b; ++i) sum += gaps[i];
        p.tempo = 60.0f * f.rate / float(sum / double(b - a));

        const float top = std::max(1e-6f, percentile(env, 0.98f));
        const int phase = findDownbeat(f, beatFrames);
        double onGrid = 0.0, all = 0.0;
        for (float v : env) all += v;
        for (size_t i = 0; i < beatFrames.size(); ++i) {
            const float strength = peakAround(env, beatFrames[i], 2);
            const int bar = int((i + size_t(Partition::kBeatsPerBar) - size_t(phase)) % size_t(Partition::kBeatsPerBar));
            p.beats.push_back({f.time(beatFrames[i]), std::min(1.0f, strength / top), bar});
        }
        // Confiance : part de la courbe d'attaques à moins de 30 ms d'un
        // temps ou d'un demi-temps, rapportée à ce que donnerait le hasard.
        const int tolerance = std::max(1, int(f.rate * 0.03f));
        std::vector<char> near(env.size(), 0);
        auto mark = [&](int frame) {
            for (int t = std::max(0, frame - tolerance); t <= std::min(int(env.size()) - 1, frame + tolerance); ++t)
                near[size_t(t)] = 1;
        };
        for (size_t i = 0; i < beatFrames.size(); ++i) {
            mark(beatFrames[i]);
            if (i + 1 < beatFrames.size()) mark((beatFrames[i] + beatFrames[i + 1]) / 2);
        }
        size_t covered = 0;
        for (size_t t = 0; t < env.size(); ++t) {
            if (!near[t]) continue;
            ++covered;
            onGrid += env[t];
        }
        const float chance = float(covered) / float(env.size());
        const float share = all > 0.0 ? float(onGrid / all) : 0.0f;
        p.confidence = chance < 1.0f ? std::clamp((share - chance) / (1.0f - chance), 0.0f, 1.0f) : 0.0f;
    }

    // Sections, sur des frontières de mesure.
    std::vector<int> barFrames;
    for (size_t i = 0; i < p.beats.size(); ++i)
        if (p.beats[i].bar == 0) barFrames.push_back(beatFrames[i]);
    std::vector<float> cuts{0.0f};
    for (int b : findBoundaries(f, barFrames)) cuts.push_back(f.time(barFrames[size_t(b)]));
    cuts.push_back(p.duration);
    const float beatLength = p.tempo > 0.0f ? 60.0f / p.tempo : 0.5f;
    for (size_t i = 0; i + 1 < cuts.size(); ++i) {
        Section s{cuts[i], cuts[i + 1], 0.0f, 0.0f, 0.0f};
        const int a = std::clamp(int(s.start * f.rate), 0, f.count - 1);
        const int b = std::clamp(int(s.end * f.rate), a + 1, f.count);
        double bright = 0.0, weight = 0.0;
        for (int t = a; t < b; ++t) {
            bright += double(f.centroid[size_t(t)]) * f.power[size_t(t)];
            weight += f.power[size_t(t)];
        }
        s.brightness = weight > 0.0 ? float(bright / weight) : 0.0f;
        const int ea = int(s.start * Partition::kEnergyRate);
        const int eb = std::min(int(p.energy.size()), std::max(ea + 1, int(s.end * Partition::kEnergyRate)));
        for (int k = ea; k < eb; ++k) s.energy += p.energy[size_t(k)] / float(eb - ea);
        int hits = 0;
        for (const Onset& o : p.onsets) hits += o.strength >= 0.5f && o.time >= s.start && o.time < s.end;
        s.density = float(hits) * beatLength / std::max(0.001f, s.end - s.start);
        p.sections.push_back(s);
    }
    return p;
}

} // namespace music

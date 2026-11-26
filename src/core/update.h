#pragma once

#include <atomic>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>

namespace core {

// Vrai si `latest` est une version plus récente que `current`. Les deux sont
// de la forme v1.2.3. Une version de développement, du type v1.2.3-4-gabcdef,
// n'est jamais comparée : elle ne vient pas d'une release.
inline bool parseVersion(const char* text, int out[3]) {
    if (!text || *text != 'v') return false;
    int used = 0;
    if (std::sscanf(text, "v%d.%d.%d%n", &out[0], &out[1], &out[2], &used) != 3) return false;
    return text[used] == '\0';
}

inline bool newerVersion(const char* latest, const char* current) {
    int a[3], b[3];
    if (!parseVersion(latest, a) || !parseVersion(current, b)) return false;
    for (int i = 0; i < 3; i++)
        if (a[i] != b[i]) return a[i] > b[i];
    return false;
}

// Tag de la dernière release, lu dans une réponse de l'API de GitHub.
inline std::string tagFromRelease(const std::string& json) {
    const size_t key = json.find("\"tag_name\"");
    if (key == std::string::npos) return "";
    const size_t colon = json.find(':', key);
    if (colon == std::string::npos) return "";
    const size_t open = json.find('"', colon + 1);
    if (open == std::string::npos) return "";
    const size_t close = json.find('"', open + 1);
    if (close == std::string::npos || close - open > 40) return "";
    return json.substr(open + 1, close - open - 1);
}

// Recherche d'une nouvelle version, en arrière-plan. Une seule requête, vers
// l'API publique de GitHub, sans rien envoyer d'autre que la demande elle-même.
// Seule la version Windows la fait : sur Android le Play Store s'en charge.
class UpdateCheck {
  public:
    static constexpr const char* kReleasesUrl = "https://github.com/Bornholm/soundwave/releases/latest";

    // Lance la recherche si la plateforme la permet et que `current` est une
    // version publiée.
    void start(const char* current);
    // Version plus récente trouvée, vide sinon.
    std::string available() const;

    // Lit le tag de la dernière release. Bloquant, quelques secondes au plus.
    static bool fetchLatest(std::string& tag, std::string& error);

  private:
    // Partagé avec le fil de la requête, qui peut finir après le jeu.
    struct Shared {
        std::mutex mutex;
        std::string available;
    };
    std::shared_ptr<Shared> shared_ = std::make_shared<Shared>();
    std::atomic<bool> started_{false};
};

} // namespace core

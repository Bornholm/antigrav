#include "core/update.h"

#include "core/log.h"

#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#elif defined(SOUNDWAVE_UPDATE_CHECK)
#include <dlfcn.h>
#endif

namespace core {

#ifdef _WIN32

bool UpdateCheck::fetchLatest(std::string& tag, std::string& error) {
    // Nom de client générique : rien qui désigne le joueur ou sa machine.
    HINTERNET session = WinHttpOpen(L"Soundwave", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        error = "WinHttpOpen";
        return false;
    }
    WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000);
    HINTERNET connection = WinHttpConnect(session, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = connection
        ? WinHttpOpenRequest(connection, L"GET", L"/repos/Bornholm/soundwave/releases/latest", nullptr,
                             WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
        : nullptr;
    std::string body;
    bool ok = false;
    if (request &&
        WinHttpSendRequest(request, L"Accept: application/vnd.github+json\r\n", static_cast<DWORD>(-1L),
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr)) {
        DWORD status = 0, size = sizeof(status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
        for (;;) {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request, &available) || available == 0) break;
            std::vector<char> chunk(available);
            DWORD read = 0;
            if (!WinHttpReadData(request, chunk.data(), available, &read) || read == 0) break;
            body.append(chunk.data(), read);
            if (body.size() > 1 << 20) break; // une réponse normale fait quelques Ko
        }
        if (status != 200) {
            error = "HTTP " + std::to_string(status);
        } else {
            tag = tagFromRelease(body);
            ok = !tag.empty();
            if (!ok) error = "réponse sans tag";
        }
    } else {
        error = "erreur WinHTTP " + std::to_string(GetLastError());
    }
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return ok;
}

#elif defined(SOUNDWAVE_UPDATE_CHECK)

namespace {

// Le strict nécessaire de l'API de libcurl. Ces valeurs font partie de son
// ABI et ne changent pas d'une version à l'autre.
constexpr int kOptWriteData = 10001, kOptUrl = 10002, kOptUserAgent = 10018;
constexpr int kOptWriteFunction = 20011;
constexpr int kOptTimeout = 13, kOptFollow = 52, kOptConnectTimeout = 78, kOptNoSignal = 99;
constexpr int kInfoResponseCode = 0x200002;

size_t appendBody(char* data, size_t size, size_t count, void* user) {
    std::string& body = *static_cast<std::string*>(user);
    if (body.size() > (1u << 20)) return 0; // une réponse normale fait quelques Ko
    body.append(data, size * count);
    return size * count;
}

} // namespace

bool UpdateCheck::fetchLatest(std::string& tag, std::string& error) {
    // Chargée à la demande : le jeu n'en dépend pas pour démarrer.
    void* lib = dlopen("libcurl.so.4", RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        error = "libcurl introuvable";
        return false;
    }
    using Init = void* (*)();
    using SetOpt = int (*)(void*, int, ...);
    using Perform = int (*)(void*);
    using GetInfo = int (*)(void*, int, ...);
    using Cleanup = void (*)(void*);
    auto init = reinterpret_cast<Init>(dlsym(lib, "curl_easy_init"));
    auto setopt = reinterpret_cast<SetOpt>(dlsym(lib, "curl_easy_setopt"));
    auto perform = reinterpret_cast<Perform>(dlsym(lib, "curl_easy_perform"));
    auto getinfo = reinterpret_cast<GetInfo>(dlsym(lib, "curl_easy_getinfo"));
    auto cleanup = reinterpret_cast<Cleanup>(dlsym(lib, "curl_easy_cleanup"));
    void* curl = init && setopt && perform && getinfo && cleanup ? init() : nullptr;
    if (!curl) {
        error = "libcurl inutilisable";
        dlclose(lib);
        return false;
    }
    std::string body;
    using Write = size_t (*)(char*, size_t, size_t, void*);
    setopt(curl, kOptUrl, "https://api.github.com/repos/Bornholm/soundwave/releases/latest");
    // Nom de client générique : rien qui désigne le joueur ou sa machine.
    setopt(curl, kOptUserAgent, "Soundwave");
    setopt(curl, kOptWriteFunction, static_cast<Write>(appendBody));
    setopt(curl, kOptWriteData, static_cast<void*>(&body));
    setopt(curl, kOptFollow, 1L);
    setopt(curl, kOptConnectTimeout, 5L);
    setopt(curl, kOptTimeout, 10L);
    setopt(curl, kOptNoSignal, 1L); // appelé hors du fil principal
    const int result = perform(curl);
    long status = 0;
    getinfo(curl, kInfoResponseCode, &status);
    cleanup(curl);
    dlclose(lib);
    if (result != 0) {
        error = "erreur libcurl " + std::to_string(result);
        return false;
    }
    if (status != 200) {
        error = "HTTP " + std::to_string(status);
        return false;
    }
    tag = tagFromRelease(body);
    if (tag.empty()) error = "réponse sans tag";
    return !tag.empty();
}

#else

bool UpdateCheck::fetchLatest(std::string&, std::string& error) {
    error = "non disponible sur cette plateforme";
    return false;
}

#endif

void UpdateCheck::start(const char* current) {
#ifdef SOUNDWAVE_UPDATE_CHECK
    int parts[3];
    if (!parseVersion(current, parts) || started_.exchange(true)) return;
    const std::string mine = current;
    std::thread([shared = shared_, mine] {
        std::string tag, error;
        if (!fetchLatest(tag, error)) {
            logInfo("Recherche de mise à jour impossible : %s", error.c_str());
            return;
        }
        if (!newerVersion(tag.c_str(), mine.c_str())) return;
        logInfo("Nouvelle version disponible : %s", tag.c_str());
        std::lock_guard<std::mutex> lock(shared->mutex);
        shared->available = tag;
    }).detach();
#else
    (void)current;
#endif
}

std::string UpdateCheck::available() const {
    std::lock_guard<std::mutex> lock(shared_->mutex);
    return shared_->available;
}

} // namespace core

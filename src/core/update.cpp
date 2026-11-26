#include "core/update.h"

#include "core/log.h"

#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
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

#else

bool UpdateCheck::fetchLatest(std::string&, std::string& error) {
    error = "non disponible sur cette plateforme";
    return false;
}

#endif

void UpdateCheck::start(const char* current) {
#ifdef _WIN32
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

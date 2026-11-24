#include "net/share.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace net {

namespace {

const uint32_t kShareMagic = 0x53474141u; // "AAGS"

void setNonBlocking(int sock) {
    int flags = ::fcntl(sock, F_GETFL, 0);
    ::fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

// En-tête d'un envoi : marque, taille, identifiant. Petit-boutiste, comme le
// reste du protocole.
void writeHeader(uint8_t* out, uint64_t id, uint32_t size) {
    std::memcpy(out, &kShareMagic, 4);
    std::memcpy(out + 4, &size, 4);
    std::memcpy(out + 8, &id, 8);
}

} // namespace

bool Share::serve(uint16_t port) {
    close();
    listener = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) return false;
    int yes = 1;
    ::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (::bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || ::listen(listener, kMaxLinks) != 0) {
        ::close(listener);
        listener = -1;
        return false;
    }
    setNonBlocking(listener);
    return true;
}

void Share::offer(uint64_t id, std::shared_ptr<const std::vector<uint8_t>> data) {
    if (id == offeredId && offered) return;
    offeredId = id;
    offered = std::move(data);
    // Les envois de l'ancien bloc n'ont plus lieu d'être.
    for (Link& link : links) {
        if (link.sock >= 0) ::close(link.sock);
        link = Link{};
    }
}

void Share::fetch(uint32_t ip, uint16_t port, uint64_t id) {
    if (client >= 0) ::close(client);
    client = ::socket(AF_INET, SOCK_STREAM, 0);
    wantedId = id;
    headerGot = 0;
    expected = 0;
    done = false;
    received.clear();
    fromIp = ip;
    fromPort = port;
    if (client < 0) return;
    setNonBlocking(client);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ip;
    addr.sin_port = htons(port);
    // Connexion non bloquante : elle aboutit pendant les pump() suivants.
    if (::connect(client, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 && errno != EINPROGRESS) {
        ::close(client);
        client = -1;
    }
}

float Share::progress() const {
    if (done) return 1.0f;
    if (expected == 0) return 0.0f;
    return static_cast<float>(received.size()) / static_cast<float>(expected);
}

bool Share::take(uint64_t id) {
    if (!done || id != wantedId) return false;
    done = false;
    return true;
}

void Share::pump() {
    // Hôte : nouveaux clients, puis suite des envois.
    if (listener >= 0) {
        for (;;) {
            int sock = ::accept(listener, nullptr, nullptr);
            if (sock < 0) break;
            Link* free = nullptr;
            for (Link& link : links)
                if (link.sock < 0) free = &link;
            if (!free || !offered) {
                ::close(sock);
                continue;
            }
            setNonBlocking(sock);
            *free = Link{};
            free->sock = sock;
            free->data = offered;
            writeHeader(free->header, offeredId, static_cast<uint32_t>(offered->size()));
        }
        for (Link& link : links) {
            if (link.sock < 0) continue;
            bool failed = false;
            while (link.headerSent < sizeof(link.header) && !failed) {
                ssize_t n = ::send(link.sock, link.header + link.headerSent, sizeof(link.header) - link.headerSent, MSG_NOSIGNAL);
                if (n > 0) link.headerSent += static_cast<size_t>(n);
                else if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                else failed = true;
            }
            while (link.headerSent == sizeof(link.header) && link.sent < link.data->size() && !failed) {
                ssize_t n = ::send(link.sock, link.data->data() + link.sent, link.data->size() - link.sent, MSG_NOSIGNAL);
                if (n > 0) link.sent += static_cast<size_t>(n);
                else if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                else failed = true;
            }
            if (failed || (link.headerSent == sizeof(link.header) && link.sent == link.data->size())) {
                // Fermeture propre : le client lit jusqu'à la fin avant de voir la coupure.
                ::shutdown(link.sock, SHUT_WR);
                ::close(link.sock);
                link = Link{};
            }
        }
    }

    // Client : lecture de l'en-tête puis des données.
    if (client >= 0 && !done) {
        bool closed = false;
        while (headerGot < sizeof(header)) {
            ssize_t n = ::recv(client, header + headerGot, sizeof(header) - headerGot, 0);
            if (n > 0) {
                headerGot += static_cast<size_t>(n);
            } else {
                closed = n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != ENOTCONN && errno != EINPROGRESS);
                break;
            }
        }
        if (headerGot == sizeof(header) && expected == 0) {
            uint32_t magic = 0, size = 0;
            uint64_t id = 0;
            std::memcpy(&magic, header, 4);
            std::memcpy(&size, header + 4, 4);
            std::memcpy(&id, header + 8, 8);
            // Mauvais bloc ou taille folle : on coupe, le jeu redemandera.
            if (magic != kShareMagic || id != wantedId || size == 0 || size > kMaxSize) closed = true;
            else {
                expected = size;
                received.reserve(size);
            }
        }
        uint8_t chunk[65536];
        while (!closed && expected > 0 && received.size() < expected) {
            const size_t want = std::min(sizeof(chunk), expected - received.size());
            ssize_t n = ::recv(client, chunk, want, 0);
            if (n > 0) received.insert(received.end(), chunk, chunk + n);
            else {
                closed = n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK);
                break;
            }
        }
        if (expected > 0 && received.size() == expected) {
            done = true;
            ::close(client);
            client = -1;
        } else if (closed) {
            ::close(client);
            client = -1;
            received.clear();
            expected = 0;
            headerGot = 0;
        }
    }
}

void Share::close() {
    if (listener >= 0) ::close(listener);
    listener = -1;
    for (Link& link : links) {
        if (link.sock >= 0) ::close(link.sock);
        link = Link{};
    }
    if (client >= 0) ::close(client);
    client = -1;
    offered.reset();
    offeredId = 0;
    done = false;
    received.clear();
    expected = 0;
    headerGot = 0;
}

} // namespace net

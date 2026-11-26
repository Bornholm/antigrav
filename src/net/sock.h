#pragma once

// Sockets : les mêmes appels sur POSIX et sur Windows. Les deux API se
// ressemblent, sauf pour l'initialisation, la fermeture, le mode non bloquant,
// les codes d'erreur et le type des tampons.

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <cstddef>

namespace net::sock {

#ifdef _WIN32

// Winsock demande une initialisation avant la première socket.
inline void init() {
    static bool started = false;
    if (started) return;
    WSADATA data;
    started = WSAStartup(MAKEWORD(2, 2), &data) == 0;
}

inline int open(int type) {
    init();
    SOCKET s = ::socket(AF_INET, type, 0);
    return s == INVALID_SOCKET ? -1 : static_cast<int>(s);
}

inline void close(int s) { ::closesocket(static_cast<SOCKET>(s)); }

inline void setNonBlocking(int s) {
    u_long on = 1;
    ::ioctlsocket(static_cast<SOCKET>(s), FIONBIO, &on);
}

inline void option(int s, int level, int name, int value) {
    ::setsockopt(static_cast<SOCKET>(s), level, name, reinterpret_cast<const char*>(&value), sizeof(value));
}

inline int accept(int listener) {
    SOCKET s = ::accept(static_cast<SOCKET>(listener), nullptr, nullptr);
    return s == INVALID_SOCKET ? -1 : static_cast<int>(s);
}

// Rien à lire ou à écrire pour l'instant.
inline bool wouldBlock() { return WSAGetLastError() == WSAEWOULDBLOCK; }

// Connexion non bloquante encore en cours.
inline bool pending() {
    const int e = WSAGetLastError();
    return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS || e == WSAEALREADY || e == WSAENOTCONN;
}

inline void shutdownWrite(int s) { ::shutdown(static_cast<SOCKET>(s), SD_SEND); }

inline long send(int s, const void* data, size_t size) {
    return ::send(static_cast<SOCKET>(s), static_cast<const char*>(data), static_cast<int>(size), 0);
}

inline long recv(int s, void* data, size_t size) {
    return ::recv(static_cast<SOCKET>(s), static_cast<char*>(data), static_cast<int>(size), 0);
}

inline long sendTo(int s, const void* data, size_t size, const sockaddr_in& to) {
    return ::sendto(static_cast<SOCKET>(s), static_cast<const char*>(data), static_cast<int>(size), 0,
                    reinterpret_cast<const sockaddr*>(&to), sizeof(to));
}

inline long recvFrom(int s, void* data, size_t size, sockaddr_in& from) {
    int len = sizeof(from);
    return ::recvfrom(static_cast<SOCKET>(s), static_cast<char*>(data), static_cast<int>(size), 0,
                      reinterpret_cast<sockaddr*>(&from), &len);
}

#else

inline void init() {}

inline int open(int type) { return ::socket(AF_INET, type, 0); }

inline void close(int s) { ::close(s); }

inline void setNonBlocking(int s) { ::fcntl(s, F_SETFL, ::fcntl(s, F_GETFL, 0) | O_NONBLOCK); }

inline void option(int s, int level, int name, int value) {
    ::setsockopt(s, level, name, &value, sizeof(value));
}

inline int accept(int listener) { return ::accept(listener, nullptr, nullptr); }

inline bool wouldBlock() { return errno == EAGAIN || errno == EWOULDBLOCK; }

inline bool pending() { return wouldBlock() || errno == ENOTCONN || errno == EINPROGRESS; }

inline void shutdownWrite(int s) { ::shutdown(s, SHUT_WR); }

// MSG_NOSIGNAL : un pair parti ne doit pas tuer le jeu par SIGPIPE.
inline long send(int s, const void* data, size_t size) { return ::send(s, data, size, MSG_NOSIGNAL); }

inline long recv(int s, void* data, size_t size) { return ::recv(s, data, size, 0); }

inline long sendTo(int s, const void* data, size_t size, const sockaddr_in& to) {
    return ::sendto(s, data, size, 0, reinterpret_cast<const sockaddr*>(&to), sizeof(to));
}

inline long recvFrom(int s, void* data, size_t size, sockaddr_in& from) {
    socklen_t len = sizeof(from);
    return ::recvfrom(s, data, size, 0, reinterpret_cast<sockaddr*>(&from), &len);
}

#endif

} // namespace net::sock

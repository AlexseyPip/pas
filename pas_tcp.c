/*
    pas_tcp.h - single-header TCP socket wrapper (stb-style)

    - No malloc: user-provided buffers for send/recv.
    - Uses OS sockets only: Winsock2 (Windows), BSD sockets (Unix).
    - connect, send, recv, close, optional timeouts.

    Usage:
        In ONE translation unit:
            #define PAS_TCP_IMPLEMENTATION
            #include "pas_tcp.h"

        In others:
            #include "pas_tcp.h"
*/

#ifndef PAS_TCP_H
#define PAS_TCP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* User allocates pas_tcp_socket_t (stack or static). No malloc. */
typedef struct pas_tcp_socket {
    void *_opaque;  /* platform SOCKET */
} pas_tcp_socket_t;

/* Initialize socket. Returns 0 on success, -1 on failure. */
int pas_tcp_init(pas_tcp_socket_t *sock);

/* Connect to host:port. Returns 0 on success, -1 on failure. */
int pas_tcp_connect(pas_tcp_socket_t *sock, const char *host, int port);

/* Set send/recv timeout in ms (0 = leave default/blocking). Returns 0 on success. */
int pas_tcp_set_timeout(pas_tcp_socket_t *sock, int timeout_ms);

/* Send data. Returns bytes sent, or < 0 on error. */
int pas_tcp_send(pas_tcp_socket_t *sock, const void *data, size_t size);

/* Receive up to size bytes. Returns bytes received, 0 on peer close, < 0 on error. */
int pas_tcp_recv(pas_tcp_socket_t *sock, void *buffer, size_t size);

/* Close socket. Safe to call with NULL or after init failure. */
void pas_tcp_close(pas_tcp_socket_t *sock);

#ifdef __cplusplus
}
#endif

/* ========== Implementation ========== */

#ifdef PAS_TCP_IMPLEMENTATION

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define PAS_TCP_INVALID ((void*)(intptr_t)(SOCKET)INVALID_SOCKET)
    #define PAS_TCP_TO_FD(s) ((SOCKET)(intptr_t)(s))
    #define PAS_TCP_FROM_FD(fd) ((void*)(intptr_t)(fd))
    #define PAS_TCP_CLOSE(fd) closesocket(fd)
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <errno.h>
    #define PAS_TCP_INVALID ((void*)(intptr_t)-1)
    #define PAS_TCP_TO_FD(s) ((int)(intptr_t)(s))
    #define PAS_TCP_FROM_FD(fd) ((void*)(intptr_t)(fd))
    #define PAS_TCP_CLOSE(fd) close(fd)
#endif

static int pas_tcp_wsa_init(void)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    static int done;
    if (!done) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
            return -1;
        done = 1;
    }
#endif
    return 0;
}

int pas_tcp_init(pas_tcp_socket_t *sock)
{
    if (!sock)
        return -1;
    if (pas_tcp_wsa_init() != 0)
        return -1;
    sock->_opaque = PAS_TCP_INVALID;
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    {
        SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
        if (s == INVALID_SOCKET) return -1;
        sock->_opaque = PAS_TCP_FROM_FD(s);
    }
#else
    {
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) return -1;
        sock->_opaque = PAS_TCP_FROM_FD(s);
    }
#endif
    return 0;
}

int pas_tcp_connect(pas_tcp_socket_t *sock, const char *host, int port)
{
    struct addrinfo hints, *res = NULL;
    char port_str[8];
    int r;

    if (!sock || sock->_opaque == PAS_TCP_INVALID || !host || port <= 0 || port > 65535)
        return -1;

    snprintf(port_str, sizeof(port_str), "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port_str, &hints, &res) != 0)
        return -1;

    r = connect(PAS_TCP_TO_FD(sock->_opaque), res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);
    return (r == 0) ? 0 : -1;
}

int pas_tcp_set_timeout(pas_tcp_socket_t *sock, int timeout_ms)
{
    if (!sock || sock->_opaque == PAS_TCP_INVALID)
        return -1;
    if (timeout_ms <= 0)
        return 0; /* 0 = no change */
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    {
        DWORD t = (DWORD)timeout_ms;
        if (setsockopt(PAS_TCP_TO_FD(sock->_opaque), SOL_SOCKET, SO_RCVTIMEO, (const char *)&t, sizeof(t)) != 0)
            return -1;
        if (setsockopt(PAS_TCP_TO_FD(sock->_opaque), SOL_SOCKET, SO_SNDTIMEO, (const char *)&t, sizeof(t)) != 0)
            return -1;
    }
#else
    {
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (setsockopt(PAS_TCP_TO_FD(sock->_opaque), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0)
            return -1;
        if (setsockopt(PAS_TCP_TO_FD(sock->_opaque), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0)
            return -1;
    }
#endif
    return 0;
}

int pas_tcp_send(pas_tcp_socket_t *sock, const void *data, size_t size)
{
    if (!sock || sock->_opaque == PAS_TCP_INVALID || !data)
        return -1;
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    return (int)send(PAS_TCP_TO_FD(sock->_opaque), (const char *)data, (int)size, 0);
#else
    ssize_t n = send(PAS_TCP_TO_FD(sock->_opaque), data, size, 0);
    return (int)n;
#endif
}

int pas_tcp_recv(pas_tcp_socket_t *sock, void *buffer, size_t size)
{
    if (!sock || sock->_opaque == PAS_TCP_INVALID || !buffer)
        return -1;
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    return recv(PAS_TCP_TO_FD(sock->_opaque), (char *)buffer, (int)size, 0);
#else
    ssize_t n = recv(PAS_TCP_TO_FD(sock->_opaque), buffer, size, 0);
    return (int)n;
#endif
}

void pas_tcp_close(pas_tcp_socket_t *sock)
{
    if (!sock || sock->_opaque == PAS_TCP_INVALID)
        return;
    PAS_TCP_CLOSE(PAS_TCP_TO_FD(sock->_opaque));
    sock->_opaque = PAS_TCP_INVALID;
}

#endif /* PAS_TCP_IMPLEMENTATION */

#endif /* PAS_TCP_H */

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

/* Set send/recv timeout in ms (0 = use default). Returns 0 on success. */
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

#include <string.h>

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define PAS_TCP_SOCK_VALID(s) ((s) != INVALID_SOCKET)
    #define PAS_TCP_ERRNO WSAGetLastError()
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <errno.h>
    #define INVALID_SOCKET (-1)
    #define SOCKET int
    #define PAS_TCP_SOCK_VALID(s) ((s) >= 0)
    #define PAS_TCP_ERRNO errno
#endif

struct pas_tcp_socket {
    SOCKET fd;
};

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

pas_tcp_socket_t *pas_tcp_socket_init(void)
{
    pas_tcp_socket_t *s;
    if (pas_tcp_wsa_init() != 0)
        return NULL;
    s = (pas_tcp_socket_t *)((char *)0 + sizeof(pas_tcp_socket_t));
    (void)s;
    /* We need to allocate a small struct. Since no malloc, use a static slot. */
    {
        static pas_tcp_socket_t g_sock;
        g_sock.fd = INVALID_SOCKET;
        s = &g_sock;
    }
    s->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (!PAS_TCP_SOCK_VALID(s->fd))
        return NULL;
    return s;
}

int pas_tcp_connect(pas_tcp_socket_t *sock, const char *host, int port)
{
    struct addrinfo hints, *res = NULL;
    char port_str[8];
    int r;

    if (!sock || !host || port <= 0 || port > 65535)
        return -1;

    snprintf(port_str, sizeof(port_str), "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port_str, &hints, &res) != 0)
        return -1;

    r = connect(sock->fd, res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);
    return (r == 0) ? 0 : -1;
}

int pas_tcp_set_timeout(pas_tcp_socket_t *sock, int timeout_ms)
{
    if (!sock || !PAS_TCP_SOCK_VALID(sock->fd))
        return -1;
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    {
        DWORD t = (DWORD)timeout_ms;
        if (setsockopt(sock->fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&t, sizeof(t)) != 0)
            return -1;
        if (setsockopt(sock->fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&t, sizeof(t)) != 0)
            return -1;
    }
#else
    {
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (setsockopt(sock->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0)
            return -1;
        if (setsockopt(sock->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0)
            return -1;
    }
#endif
    return 0;
}

int pas_tcp_send(pas_tcp_socket_t *sock, const void *data, size_t size)
{
    if (!sock || !PAS_TCP_SOCK_VALID(sock->fd) || !data)
        return -1;
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    return send(sock->fd, (const char *)data, (int)size, 0);
#else
    ssize_t n = send(sock->fd, data, size, 0);
    return (int)n;
#endif
}

int pas_tcp_recv(pas_tcp_socket_t *sock, void *buffer, size_t size)
{
    if (!sock || !PAS_TCP_SOCK_VALID(sock->fd) || !buffer)
        return -1;
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    return recv(sock->fd, (char *)buffer, (int)size, 0);
#else
    ssize_t n = recv(sock->fd, buffer, size, 0);
    return (int)n;
#endif
}

void pas_tcp_close(pas_tcp_socket_t *sock)
{
    if (!sock) return;
    if (PAS_TCP_SOCK_VALID(sock->fd)) {
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
        closesocket(sock->fd);
#else
        close(sock->fd);
#endif
        sock->fd = INVALID_SOCKET;
    }
}

#endif /* PAS_TCP_IMPLEMENTATION */

#endif /* PAS_TCP_H */

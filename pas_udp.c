/*
    pas_udp.h - single-header UDP socket wrapper (stb-style)

    - No malloc: user-provided buffers. User allocates pas_udp_socket_t.
    - Uses OS sockets only: Winsock2 (Windows), BSD sockets (Unix).
    - init, bind (optional), sendto, recvfrom, close.

    Usage:
        In ONE translation unit:
            #define PAS_UDP_IMPLEMENTATION
            #include "pas_udp.h"

        In others:
            #include "pas_udp.h"
*/

#ifndef PAS_UDP_H
#define PAS_UDP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* User allocates. Max 64 bytes for sockaddr. */
#define PAS_UDP_ADDR_MAX 64

typedef struct pas_udp_socket {
    void *_opaque;  /* platform SOCKET */
} pas_udp_socket_t;

typedef struct pas_udp_addr {
    unsigned char data[PAS_UDP_ADDR_MAX];
    int len;
} pas_udp_addr_t;

/* Initialize UDP socket. Returns 0 on success, -1 on failure. */
int pas_udp_init(pas_udp_socket_t *sock);

/* Optionally bind to local address. addr_str "0.0.0.0" or "127.0.0.1", port 0 = any. Returns 0 on success. */
int pas_udp_bind(pas_udp_socket_t *sock, const char *addr_str, int port);

/* Set recv timeout in ms (0 = blocking). Returns 0 on success. */
int pas_udp_set_timeout(pas_udp_socket_t *sock, int timeout_ms);

/* Send to host:port. Returns bytes sent, or < 0 on error. */
int pas_udp_sendto(pas_udp_socket_t *sock, const void *data, size_t size,
                   const char *host, int port);

/* Receive into buffer. Fills *peer_addr if non-NULL (for reply). Returns bytes received, < 0 on error. */
int pas_udp_recvfrom(pas_udp_socket_t *sock, void *buffer, size_t size,
                     pas_udp_addr_t *peer_addr);

/* Send to pre-resolved address (from recvfrom peer). Returns bytes sent, or < 0. */
int pas_udp_sendto_addr(pas_udp_socket_t *sock, const void *data, size_t size,
                        const pas_udp_addr_t *addr);

/* Close socket. Safe to call with NULL. */
void pas_udp_close(pas_udp_socket_t *sock);

#ifdef __cplusplus
}
#endif

/* ========== Implementation ========== */

#ifdef PAS_UDP_IMPLEMENTATION

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
    #define PAS_UDP_INVALID ((void*)(intptr_t)(SOCKET)INVALID_SOCKET)
    #define PAS_UDP_TO_FD(s) ((SOCKET)(intptr_t)(s))
    #define PAS_UDP_FROM_FD(fd) ((void*)(intptr_t)(fd))
    #define PAS_UDP_CLOSE(fd) closesocket(fd)
    #define PAS_UDP_ERRNO WSAGetLastError()
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <errno.h>
    #define PAS_UDP_INVALID ((void*)(intptr_t)-1)
    #define PAS_UDP_TO_FD(s) ((int)(intptr_t)(s))
    #define PAS_UDP_FROM_FD(fd) ((void*)(intptr_t)(fd))
    #define PAS_UDP_CLOSE(fd) close(fd)
    #define PAS_UDP_ERRNO errno
#endif

static int pas_udp_wsa_init(void)
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

int pas_udp_init(pas_udp_socket_t *sock)
{
    if (!sock)
        return -1;
    if (pas_udp_wsa_init() != 0)
        return -1;
    sock->_opaque = PAS_UDP_INVALID;
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    {
        SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s == INVALID_SOCKET) return -1;
        sock->_opaque = PAS_UDP_FROM_FD(s);
    }
#else
    {
        int s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0) return -1;
        sock->_opaque = PAS_UDP_FROM_FD(s);
    }
#endif
    return 0;
}

int pas_udp_bind(pas_udp_socket_t *sock, const char *addr_str, int port)
{
    struct sockaddr_in addr;
    if (!sock || sock->_opaque == PAS_UDP_INVALID || !addr_str)
        return -1;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)(port & 0xFFFF));
    if (addr_str[0] == '0' && (addr_str[1] == '\0' || (addr_str[1] == '.' && addr_str[2] == '0')))
        if (inet_pton(AF_INET, addr_str, &addr.sin_addr) != 1)
        return -1;
    if (bind(PAS_UDP_TO_FD(sock->_opaque), (struct sockaddr *)&addr, sizeof(addr)) != 0)
        return -1;
    return 0;
}

int pas_udp_set_timeout(pas_udp_socket_t *sock, int timeout_ms)
{
    if (!sock || sock->_opaque == PAS_UDP_INVALID)
        return -1;
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    {
        DWORD t = (DWORD)timeout_ms;
        if (setsockopt(PAS_UDP_TO_FD(sock->_opaque), SOL_SOCKET, SO_RCVTIMEO, (const char *)&t, sizeof(t)) != 0)
            return -1;
    }
#else
    {
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (setsockopt(PAS_UDP_TO_FD(sock->_opaque), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0)
            return -1;
    }
#endif
    return 0;
}

static int pas_udp_resolve(const char *host, int port, struct sockaddr_in *out)
{
    struct addrinfo hints, *res = NULL;
    char port_str[8];
    if (!host || port <= 0 || port > 65535 || !out)
        return -1;
    snprintf(port_str, sizeof(port_str), "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host, port_str, &hints, &res) != 0)
        return -1;
    if (!res || res->ai_addrlen < sizeof(struct sockaddr_in)) {
        if (res) freeaddrinfo(res);
        return -1;
    }
    memcpy(out, res->ai_addr, sizeof(struct sockaddr_in));
    freeaddrinfo(res);
    return 0;
}

int pas_udp_sendto(pas_udp_socket_t *sock, const void *data, size_t size,
                   const char *host, int port)
{
    struct sockaddr_in addr;
    if (!sock || sock->_opaque == PAS_UDP_INVALID || !data)
        return -1;
    if (pas_udp_resolve(host, port, &addr) != 0)
        return -1;
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    return (int)sendto(PAS_UDP_TO_FD(sock->_opaque), (const char *)data, (int)size, 0,
                       (struct sockaddr *)&addr, sizeof(addr));
#else
    ssize_t n = sendto(PAS_UDP_TO_FD(sock->_opaque), data, size, 0,
                       (struct sockaddr *)&addr, sizeof(addr));
    return (int)n;
#endif
}

int pas_udp_recvfrom(pas_udp_socket_t *sock, void *buffer, size_t size,
                     pas_udp_addr_t *peer_addr)
{
    struct sockaddr_storage ss;
    socklen_t ss_len = sizeof(ss);
    int n;

    if (!sock || sock->_opaque == PAS_UDP_INVALID || !buffer)
        return -1;

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    n = recvfrom(PAS_UDP_TO_FD(sock->_opaque), (char *)buffer, (int)size, 0,
                 (struct sockaddr *)&ss, &ss_len);
#else
    n = (int)recvfrom(PAS_UDP_TO_FD(sock->_opaque), buffer, size, 0,
                      (struct sockaddr *)&ss, &ss_len);
#endif
    if (n < 0)
        return -1;
    if (peer_addr) {
        size_t copy = (ss_len < (socklen_t)sizeof(peer_addr->data)) ? (size_t)ss_len : sizeof(peer_addr->data);
        memcpy(peer_addr->data, &ss, copy);
        peer_addr->len = (int)copy;
    }
    return n;
}

int pas_udp_sendto_addr(pas_udp_socket_t *sock, const void *data, size_t size,
                        const pas_udp_addr_t *addr)
{
    if (!sock || sock->_opaque == PAS_UDP_INVALID || !data || !addr || addr->len <= 0)
        return -1;
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    return (int)sendto(PAS_UDP_TO_FD(sock->_opaque), (const char *)data, (int)size, 0,
                       (struct sockaddr *)addr->data, addr->len);
#else
    ssize_t n = sendto(PAS_UDP_TO_FD(sock->_opaque), data, size, 0,
                       (struct sockaddr *)addr->data, (socklen_t)addr->len);
    return (int)n;
#endif
}

void pas_udp_close(pas_udp_socket_t *sock)
{
    if (!sock || sock->_opaque == PAS_UDP_INVALID)
        return;
    PAS_UDP_CLOSE(PAS_UDP_TO_FD(sock->_opaque));
    sock->_opaque = PAS_UDP_INVALID;
}

#endif /* PAS_UDP_IMPLEMENTATION */

#endif /* PAS_UDP_H */

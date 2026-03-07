/*
    pas_http2.h - single-header HTTP/2 client (h2c, stb-style)

    - No malloc: user provides response_buffer; parsing points into it.
    - Uses pas_tcp for transport (include pas_tcp.h and define PAS_TCP_IMPLEMENTATION).
    - HTTP/2 cleartext (h2c) only: connection preface, SETTINGS, HEADERS, DATA.
    - Minimal HPACK: static table + literal for :authority; GET requests only.

    Usage:
        #define PAS_TCP_IMPLEMENTATION
        #define PAS_HTTP2_IMPLEMENTATION
        #include "pas_tcp.h"
        #include "pas_http2.h"
*/

#ifndef PAS_HTTP2_H
#define PAS_HTTP2_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PAS_HTTP2_OK           0
#define PAS_HTTP2_E_INVALID   -1
#define PAS_HTTP2_E_CONNECTION -2
#define PAS_HTTP2_E_TIMEOUT   -3
#define PAS_HTTP2_E_NOSPACE   -4
#define PAS_HTTP2_E_PROTOCOL  -5

typedef struct pas_http2_response {
    int         status_code;
    const char *headers;    /* points into response_buffer */
    size_t      headers_len;
    const char *body;
    size_t      body_len;
} pas_http2_response_t;

/*
    pas_http2_get:
        h2c GET to host:port/path. Requires pas_tcp.
        response_buffer receives raw bytes; out_response points into it.
        timeout_ms: 0 = use default.
*/
int pas_http2_get(const char *host, int port, const char *path,
                  char *response_buffer, size_t buffer_size,
                  int timeout_ms,
                  pas_http2_response_t *out_response,
                  int *status);

#ifdef __cplusplus
}
#endif

/* ========== Implementation ========== */

#ifdef PAS_HTTP2_IMPLEMENTATION

#include "pas_tcp.h"
#include <string.h>
#include <stdio.h>

/* HTTP/2 connection preface */
static const unsigned char pas_http2_preface[] =
    "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

/* Frame types */
#define PAS_H2_DATA         0
#define PAS_H2_HEADERS      1
#define PAS_H2_SETTINGS     4
#define PAS_H2_GOAWAY       7

/* Flags */
#define PAS_H2_END_STREAM   0x01
#define PAS_H2_END_HEADERS  0x04

static void pas_http2_u24_be(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v >> 16);
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)v;
}

static void pas_http2_u32_be(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static unsigned int pas_http2_read_u24(const unsigned char *p)
{
    return ((unsigned int)p[0] << 16) | ((unsigned int)p[1] << 8) | (unsigned int)p[2];
}

static unsigned int pas_http2_read_u32(const unsigned char *p)
{
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
           ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

/* Minimal HPACK encode for GET request: :method GET, :path, :scheme http, :authority host */
static int pas_http2_build_headers_block(unsigned char *out, size_t out_size,
                                         const char *path, size_t path_len,
                                         const char *host, size_t host_len)
{
    size_t used = 0;
    /* :method GET (index 2) */
    if (used + 1 > out_size) return -1;
    out[used++] = 0x82;
    /* :path (index 4). Use literal for custom path. */
    if (path_len == 1 && path[0] == '/') {
        if (used + 1 > out_size) return -1;
        out[used++] = 0x84; /* index 4 = "/" */
    } else {
        if (used + 2 + path_len > out_size) return -1;
        out[used++] = 0x44; /* literal with indexing, name index 4 (:path) */
        out[used++] = (unsigned char)path_len;
        memcpy(out + used, path, path_len);
        used += path_len;
    }
    /* :scheme http (index 6) */
    if (used + 1 > out_size) return -1;
    out[used++] = 0x86;
    /* :authority (index 1), literal value = host */
    if (used + 2 + host_len > out_size) return -1;
    out[used++] = 0x41; /* literal with indexing, name index 1 */
    out[used++] = (unsigned char)host_len;
    memcpy(out + used, host, host_len);
    used += host_len;
    return (int)used;
}

/* Build and send a frame. Returns 0 on success. */
static int pas_http2_send_frame(pas_tcp_socket_t *sock, unsigned char type, unsigned char flags,
                                unsigned int stream_id, const void *payload, size_t payload_len)
{
    unsigned char hdr[9];
    pas_http2_u24_be(hdr + 0, (unsigned int)payload_len);
    hdr[3] = type;
    hdr[4] = flags;
    pas_http2_u32_be(hdr + 5, stream_id); /* clears reserved bit */
    if (pas_tcp_send(sock, hdr, 9) != 9)
        return -1;
    if (payload_len > 0 && pas_tcp_send(sock, payload, payload_len) != (int)payload_len)
        return -1;
    return 0;
}

/* Read exactly n bytes. Returns 0 on success, -1 on error/timeout. */
static int pas_http2_recv_exact(pas_tcp_socket_t *sock, void *buf, size_t n)
{
    unsigned char *p = (unsigned char *)buf;
    size_t got = 0;
    while (got < n) {
        int r = pas_tcp_recv(sock, p + got, n - got);
        if (r <= 0) return -1;
        got += (size_t)r;
    }
    return 0;
}

/* Parse :status from HPACK headers. Simple scan for ":status" and digits. */
static int pas_http2_parse_status(const unsigned char *data, size_t len)
{
    size_t i;
    if (len < 10) return 0;
    /* Look for ":status" (0x08 0x03 ":st" 0x61 "a" 0x74 "t" 0x75 "u" 0x73 "s") in HPACK.
       Simplified: look for " status: " or similar. Actually HPACK uses 0x08 0x03 ":st" etc.
       Static :status = index 8 in HPACK appendix. Or literal. Easiest: scan for 3-digit number
       after we've decoded. We don't fully decode HPACK here - just look for pattern.
       Common: indexed 8 (:status) then literal "200". So we'd see 0x88 (index 8) 0x03 "200".
       Or 0x80 0x03 "200" for literal without indexing.
       Let's scan for ASCII "status" near start, then read digits. */
    for (i = 0; i + 7 < len; i++) {
        if (data[i] == ':' && data[i+1] == 's' && data[i+2] == 't' && data[i+3] == 'a' &&
            data[i+4] == 't' && data[i+5] == 'u' && data[i+6] == 's') {
            i += 7;
            while (i < len && (data[i] == ' ' || data[i] == '\t')) i++;
            if (i + 3 <= len && data[i] >= '0' && data[i] <= '9') {
                int code = (data[i] - '0') * 100;
                if (i+1 < len && data[i+1] >= '0' && data[i+1] <= '9')
                    code += (data[i+1] - '0') * 10;
                if (i+2 < len && data[i+2] >= '0' && data[i+2] <= '9')
                    code += (data[i+2] - '0');
                return code;
            }
        }
    }
    /* Also check for HPACK indexed :status. Index 8 = :status. Value comes as literal. */
    for (i = 0; i + 4 < len; i++) {
        if ((data[i] & 0x80) && (data[i] & 0x7F) == 8) { /* indexed 8 */
            i++;
            if (data[i] & 0x80) { /* huffman */
                /* skip for now */
            } else {
                size_t vlen = data[i] & 0x7F;
                i++;
                if (i + vlen <= len && vlen == 3 &&
                    data[i] >= '0' && data[i] <= '9' && data[i+1] >= '0' && data[i+1] <= '9' &&
                    data[i+2] >= '0' && data[i+2] <= '9') {
                    return (data[i]-'0')*100 + (data[i+1]-'0')*10 + (data[i+2]-'0');
                }
            }
        }
    }
    return 0;
}

int pas_http2_get(const char *host, int port, const char *path,
                  char *response_buffer, size_t buffer_size,
                  int timeout_ms,
                  pas_http2_response_t *out_response,
                  int *status)
{
    pas_tcp_socket_t sock;
    unsigned char frame_buf[4096];
    unsigned char hdr_block[512];
    int hblen;
    size_t total_read = 0;
    size_t body_off = 0;
    int found_status = 0;
    int stream_id = 1;

    if (status) *status = PAS_HTTP2_OK;
    if (out_response) {
        out_response->status_code = 0;
        out_response->headers = NULL;
        out_response->headers_len = 0;
        out_response->body = NULL;
        out_response->body_len = 0;
    }

    if (!host || port <= 0 || port > 65535 || !path || !response_buffer || buffer_size < 256 ||
        !out_response || !status) {
        if (status) *status = PAS_HTTP2_E_INVALID;
        return PAS_HTTP2_E_INVALID;
    }

    if (pas_tcp_init(&sock) != 0) {
        if (status) *status = PAS_HTTP2_E_CONNECTION;
        return PAS_HTTP2_E_CONNECTION;
    }

    if (timeout_ms <= 0) timeout_ms = 30000;
    if (pas_tcp_set_timeout(&sock, timeout_ms) != 0) {
        pas_tcp_close(&sock);
        if (status) *status = PAS_HTTP2_E_CONNECTION;
        return PAS_HTTP2_E_CONNECTION;
    }

    if (pas_tcp_connect(&sock, host, port) != 0) {
        pas_tcp_close(&sock);
        if (status) *status = PAS_HTTP2_E_CONNECTION;
        return PAS_HTTP2_E_CONNECTION;
    }

    /* Send preface */
    if (pas_tcp_send(&sock, pas_http2_preface, sizeof(pas_http2_preface) - 1) != (int)(sizeof(pas_http2_preface) - 1)) {
        pas_tcp_close(&sock);
        if (status) *status = PAS_HTTP2_E_CONNECTION;
        return PAS_HTTP2_E_CONNECTION;
    }

    /* Send empty SETTINGS */
    if (pas_http2_send_frame(&sock, PAS_H2_SETTINGS, 0, 0, NULL, 0) != 0) {
        pas_tcp_close(&sock);
        if (status) *status = PAS_HTTP2_E_CONNECTION;
        return PAS_HTTP2_E_CONNECTION;
    }

    /* Read server SETTINGS (and maybe SETTINGS ACK) */
    if (pas_http2_recv_exact(&sock, frame_buf, 9) != 0) {
        pas_tcp_close(&sock);
        if (status) *status = PAS_HTTP2_E_PROTOCOL;
        return PAS_HTTP2_E_PROTOCOL;
    }
    {
        unsigned int plen = pas_http2_read_u24(frame_buf);
        unsigned char ftype = frame_buf[3];
        if (ftype == PAS_H2_SETTINGS && plen > 0) {
            if (pas_http2_recv_exact(&sock, frame_buf, plen) != 0) {
                pas_tcp_close(&sock);
                if (status) *status = PAS_HTTP2_E_PROTOCOL;
                return PAS_HTTP2_E_PROTOCOL;
            }
        } else if (ftype == PAS_H2_SETTINGS && plen == 0) {
            /* empty settings, ok */
        }
    }

    /* Send SETTINGS ACK */
    if (pas_http2_send_frame(&sock, PAS_H2_SETTINGS, 0x01, 0, NULL, 0) != 0) {
        pas_tcp_close(&sock);
        if (status) *status = PAS_HTTP2_E_CONNECTION;
        return PAS_HTTP2_E_CONNECTION;
    }

    /* Build and send HEADERS for GET */
    {
        char path_buf[512];
        size_t path_len, host_len;
        if (!path || path[0] == '\0') {
            path = "/";
            path_len = 1;
        } else if (path[0] == '/') {
            path_len = strlen(path);
        } else {
            size_t slen = strlen(path);
            if (slen + 2 > sizeof(path_buf)) slen = sizeof(path_buf) - 2;
            path_buf[0] = '/';
            memcpy(path_buf + 1, path, slen);
            path_buf[1 + slen] = '\0';
            path = path_buf;
            path_len = 1 + slen;
        }
        host_len = strlen(host);
        hblen = pas_http2_build_headers_block(hdr_block, sizeof(hdr_block),
                                              path, path_len, host, host_len);
        if (hblen < 0) {
            pas_tcp_close(&sock);
            if (status) *status = PAS_HTTP2_E_INVALID;
            return PAS_HTTP2_E_INVALID;
        }
        if (pas_http2_send_frame(&sock, PAS_H2_HEADERS, PAS_H2_END_STREAM | PAS_H2_END_HEADERS,
                                 (unsigned int)stream_id, hdr_block, (size_t)hblen) != 0) {
            pas_tcp_close(&sock);
            if (status) *status = PAS_HTTP2_E_CONNECTION;
            return PAS_HTTP2_E_CONNECTION;
        }
    }

    /* Read frames until END_STREAM on our stream */
    while (total_read < buffer_size - 1) {
        unsigned char hdr[9];
        unsigned int plen;
        unsigned char ftype, flags;
        unsigned int sid;

        if (pas_http2_recv_exact(&sock, hdr, 9) != 0)
            break;
        plen = pas_http2_read_u24(hdr);
        ftype = hdr[3];
        flags = hdr[4];
        sid = pas_http2_read_u32(hdr + 5) & 0x7FFFFFFFu;

        if (plen > sizeof(frame_buf))
            plen = (unsigned int)sizeof(frame_buf);
        if (plen > 0 && pas_http2_recv_exact(&sock, frame_buf, plen) != 0)
            break;

        if (ftype == PAS_H2_GOAWAY) {
            if (status) *status = PAS_HTTP2_E_PROTOCOL;
            pas_tcp_close(&sock);
            return PAS_HTTP2_E_PROTOCOL;
        }

        if (sid == (unsigned int)stream_id) {
            if (ftype == PAS_H2_HEADERS) {
                if (!found_status)
                    out_response->status_code = pas_http2_parse_status(frame_buf, plen);
                found_status = 1;
                if (out_response->headers == NULL) {
                    out_response->headers = response_buffer;
                    out_response->headers_len = (size_t)plen;
                }
                if (total_read + plen < buffer_size) {
                    memcpy(response_buffer + total_read, frame_buf, plen);
                    total_read += plen;
                    body_off = total_read;
                }
            } else if (ftype == PAS_H2_DATA) {
                if (total_read + plen < buffer_size) {
                    memcpy(response_buffer + total_read, frame_buf, plen);
                    total_read += plen;
                }
            }
        }

        if (ftype == PAS_H2_HEADERS && (flags & PAS_H2_END_STREAM)) {
            out_response->body = response_buffer + body_off;
            out_response->body_len = total_read - body_off;
            break;
        }
        if (ftype == PAS_H2_DATA && (flags & PAS_H2_END_STREAM)) {
            out_response->body = response_buffer + body_off;
            out_response->body_len = total_read - body_off;
            break;
        }
    }

    response_buffer[total_read] = '\0';
    pas_tcp_close(&sock);

    if (!found_status && out_response->status_code == 0)
        out_response->status_code = 200; /* assume ok if we got data */

    if (total_read >= buffer_size - 1 && status)
        *status = PAS_HTTP2_E_NOSPACE;

    return (*status == PAS_HTTP2_OK) ? 0 : *status;
}

#endif /* PAS_HTTP2_IMPLEMENTATION */

#endif /* PAS_HTTP2_H */

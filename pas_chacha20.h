/*
    pas_chacha20.h - ChaCha20 stream cipher (stb-style)

    - No malloc: user-provided buffers. User allocates pas_chacha20_ctx_t.
    - RFC 7539: 256-bit key, 96-bit nonce, 32-bit block counter.
    - pas_chacha20_xor encrypts/decrypts (same operation).

    Usage:
        In ONE translation unit:
            #define PAS_CHACHA20_IMPLEMENTATION
            #include "pas_chacha20.h"

        In others:
            #include "pas_chacha20.h"
*/

#ifndef PAS_CHACHA20_H
#define PAS_CHACHA20_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* User allocates (stack or static). */
typedef struct pas_chacha20_ctx {
    uint32_t state[16];
    uint8_t  block[64];
    int      block_used;
} pas_chacha20_ctx_t;

#define PAS_CHACHA20_KEY_SIZE  32
#define PAS_CHACHA20_NONCE_SIZE 12

/* Initialize context with 32-byte key, 12-byte nonce, initial block counter (usually 0). */
void pas_chacha20_init(pas_chacha20_ctx_t *ctx,
                       const uint8_t *key,
                       const uint8_t *nonce,
                       uint32_t counter);

/* XOR len bytes: dst[i] = src[i] ^ keystream. dst and src may overlap (use same buffer for in-place). */
void pas_chacha20_xor(pas_chacha20_ctx_t *ctx, uint8_t *dst, const uint8_t *src, size_t len);

#ifdef __cplusplus
}
#endif

/* ========== Implementation ========== */

#ifdef PAS_CHACHA20_IMPLEMENTATION

#include <string.h>

static uint32_t pas_chacha20_rotl32(uint32_t x, int n)
{
    return (x << n) | (x >> (32 - n));
}

static void pas_chacha20_quarter(uint32_t *x, int a, int b, int c, int d)
{
    x[a] += x[b]; x[d] ^= x[a]; x[d] = pas_chacha20_rotl32(x[d], 16);
    x[c] += x[d]; x[b] ^= x[c]; x[b] = pas_chacha20_rotl32(x[b], 12);
    x[a] += x[b]; x[d] ^= x[a]; x[d] = pas_chacha20_rotl32(x[d],  8);
    x[c] += x[d]; x[b] ^= x[c]; x[b] = pas_chacha20_rotl32(x[b],  7);
}

static void pas_chacha20_block(const uint32_t in[16], uint8_t out[64])
{
    uint32_t x[16];
    int i;

    memcpy(x, in, sizeof(x));

    for (i = 0; i < 10; i++) {
        pas_chacha20_quarter(x,  0,  4,  8, 12);
        pas_chacha20_quarter(x,  1,  5,  9, 13);
        pas_chacha20_quarter(x,  2,  6, 10, 14);
        pas_chacha20_quarter(x,  3,  7, 11, 15);
        pas_chacha20_quarter(x,  0,  5, 10, 15);
        pas_chacha20_quarter(x,  1,  6, 11, 12);
        pas_chacha20_quarter(x,  2,  7,  8, 13);
        pas_chacha20_quarter(x,  3,  4,  9, 14);
    }

    for (i = 0; i < 16; i++) {
        uint32_t v = x[i] + in[i];
        out[i*4 + 0] = (uint8_t)(v      );
        out[i*4 + 1] = (uint8_t)(v >>  8);
        out[i*4 + 2] = (uint8_t)(v >> 16);
        out[i*4 + 3] = (uint8_t)(v >> 24);
    }
}

static uint32_t pas_chacha20_u32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void pas_chacha20_init(pas_chacha20_ctx_t *ctx,
                       const uint8_t *key,
                       const uint8_t *nonce,
                       uint32_t counter)
{
    if (!ctx || !key || !nonce) return;

    ctx->state[0]  = 0x61707865u;
    ctx->state[1]  = 0x3320646eu;
    ctx->state[2]  = 0x79622d32u;
    ctx->state[3]  = 0x6b206574u;
    ctx->state[4]  = pas_chacha20_u32le(key +  0);
    ctx->state[5]  = pas_chacha20_u32le(key +  4);
    ctx->state[6]  = pas_chacha20_u32le(key +  8);
    ctx->state[7]  = pas_chacha20_u32le(key + 12);
    ctx->state[8]  = pas_chacha20_u32le(key + 16);
    ctx->state[9]  = pas_chacha20_u32le(key + 20);
    ctx->state[10] = pas_chacha20_u32le(key + 24);
    ctx->state[11] = pas_chacha20_u32le(key + 28);
    ctx->state[12] = counter;
    ctx->state[13] = pas_chacha20_u32le(nonce + 0);
    ctx->state[14] = pas_chacha20_u32le(nonce + 4);
    ctx->state[15] = pas_chacha20_u32le(nonce + 8);
    ctx->block_used = 64;
}

void pas_chacha20_xor(pas_chacha20_ctx_t *ctx, uint8_t *dst, const uint8_t *src, size_t len)
{
    size_t i;

    if (!ctx || !dst || !src) return;

    for (i = 0; i < len; i++) {
        if (ctx->block_used >= 64) {
            pas_chacha20_block(ctx->state, ctx->block);
            ctx->state[12]++;
            ctx->block_used = 0;
        }
        dst[i] = src[i] ^ ctx->block[ctx->block_used++];
    }
}

#endif /* PAS_CHACHA20_IMPLEMENTATION */

#endif /* PAS_CHACHA20_H */

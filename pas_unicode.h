/*
    pas_unicode.h - single-header, stb-style, dependency-free Unicode helpers

    Design goals:
    - No dynamic allocation
    - No libc locale dependencies
    - Works in freestanding / OS-dev environments
    - C89-compatible header (implementation C99-ish where needed)

    Usage:
        // In ONE translation unit:
        #define PAS_UNICODE_IMPLEMENTATION
        #include "pas_unicode.h"

        // In all other translation units:
        #include "pas_unicode.h"

    This header focuses on:
    - Code point representation
    - UTF-8 encode/decode
    - UTF-16 encode/decode
    - Basic classification for ASCII subset (always safe even without tables)
    - Simple iteration helpers

    Future extensions can add:
    - Full Unicode category tables
    - Case conversion
    - Normalization, collation, etc.
*/

#ifndef PAS_UNICODE_H
#define PAS_UNICODE_H

/* Configuration
   You can override:
     - PASUDEF       : linkage specifier for non-inline functions
     - PASU_INLINE   : inline keyword
*/

#if !defined(PASUDEF)
    #if defined(PAS_UNICODE_STATIC)
        #define PASUDEF static
    #else
        #define PASUDEF extern
    #endif
#endif

#if !defined(PASU_INLINE)
    #if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)
        #define PASU_INLINE static __inline
    #else
        #define PASU_INLINE static
    #endif
#endif

/* Basic types */

#if defined(_MSC_VER) && (_MSC_VER < 1600)
    typedef unsigned char  pasu_uint8;
    typedef unsigned short pasu_uint16;
    typedef unsigned int   pasu_uint32;
    typedef int            pasu_int32;
    typedef unsigned long  pasu_size;
#else
    #include <stddef.h>
    #include <stdint.h>
    typedef uint8_t  pasu_uint8;
    typedef uint16_t pasu_uint16;
    typedef uint32_t pasu_uint32;
    typedef int32_t  pasu_int32;
    typedef size_t   pasu_size;
#endif

/* A Unicode scalar value (0..0x10FFFF, excluding surrogates) */
typedef pasu_uint32 pasu_codepoint;

/* Error codes */
typedef enum pasu_status {
    PASU_OK = 0,
    PASU_E_INVALID = -1,   /* ill-formed sequence */
    PASU_E_RANGE   = -2,   /* code point out of Unicode range */
    PASU_E_SURROG  = -3,   /* surrogate code point where not allowed */
    PASU_E_TRUNC   = -4,   /* truncated input sequence */
    PASU_E_NOSPACE = -5    /* output buffer too small */
} pasu_status;

/* ==============================
   Query helpers
   ============================== */

/* Returns non-zero if cp is a valid Unicode scalar value (not surrogate, in range). */
PASU_INLINE int pasu_is_valid_scalar(pasu_codepoint cp)
{
    if (cp > 0x10FFFFu)
        return 0;
    if (cp >= 0xD800u && cp <= 0xDFFFu)
        return 0;
    return 1;
}

/* ASCII helpers - always safe and not table-based */

PASU_INLINE int pasu_is_ascii(pasu_codepoint cp)
{
    return cp < 0x80u;
}

PASU_INLINE int pasu_is_ascii_digit(pasu_codepoint cp)
{
    return (cp >= '0' && cp <= '9');
}

PASU_INLINE int pasu_is_ascii_upper(pasu_codepoint cp)
{
    return (cp >= 'A' && cp <= 'Z');
}

PASU_INLINE int pasu_is_ascii_lower(pasu_codepoint cp)
{
    return (cp >= 'a' && cp <= 'z');
}

PASU_INLINE int pasu_is_ascii_alpha(pasu_codepoint cp)
{
    return pasu_is_ascii_upper(cp) || pasu_is_ascii_lower(cp);
}

PASU_INLINE int pasu_is_ascii_alnum(pasu_codepoint cp)
{
    return pasu_is_ascii_alpha(cp) || pasu_is_ascii_digit(cp);
}

PASU_INLINE int pasu_is_ascii_space(pasu_codepoint cp)
{
    return cp == ' '  || cp == '\t' ||
           cp == '\n' || cp == '\r' ||
           cp == '\v' || cp == '\f';
}

/* ==============================
   UTF-8
   ============================== */

/*
    pasu_utf8_decode:
      - s    : pointer to first byte
      - len  : number of available bytes from s
      - out  : receives decoded code point
      - used : receives number of bytes consumed

      Returns PASU_OK or an error code.
      On error, *out is set to 0xFFFD (replacement char) and *used is
      set to minimal number of bytes consumed (usually 1).
*/
PASUDEF pasu_status pasu_utf8_decode(const pasu_uint8 *s, pasu_size len,
                                     pasu_codepoint *out, pasu_size *used);

/*
    pasu_utf8_encode:
      - cp   : code point to encode
      - out  : buffer of at least 4 bytes
      - used : receives number of bytes written

      Returns PASU_OK or an error code.
*/
PASUDEF pasu_status pasu_utf8_encode(pasu_codepoint cp,
                                     pasu_uint8 out[4], pasu_size *used);

/*
    pasu_utf8_next:
      Helper to iterate over a UTF-8 buffer.

        pos: in/out index into the buffer
        len: total buffer length
        out: decoded code point

      On PASU_OK, *pos is advanced by the number of bytes consumed.
*/
PASU_INLINE pasu_status pasu_utf8_next(const pasu_uint8 *s, pasu_size len,
                                       pasu_size *pos, pasu_codepoint *out)
{
    pasu_status st;
    pasu_size used = 0;
    if (*pos >= len)
        return PASU_E_TRUNC;
    st = pasu_utf8_decode(s + *pos, len - *pos, out, &used);
    *pos += used;
    return st;
}

/* ==============================
   UTF-16 (LE/BE agnostic, uses 16-bit units)
   ============================== */

/*
    pasu_utf16_decode:
      - s    : pointer to first 16-bit unit
      - len  : number of available 16-bit units
      - out  : receives decoded code point
      - used : receives number of units consumed (1 or 2)

      Returns PASU_OK or an error code.
*/
PASUDEF pasu_status pasu_utf16_decode(const pasu_uint16 *s, pasu_size len,
                                      pasu_codepoint *out, pasu_size *used);

/*
    pasu_utf16_encode:
      - cp   : code point to encode
      - out  : buffer of at least 2 units
      - used : receives number of units written (1 or 2)

      Returns PASU_OK or an error code.
*/
PASUDEF pasu_status pasu_utf16_encode(pasu_codepoint cp,
                                      pasu_uint16 out[2], pasu_size *used);

/*
    pasu_utf16_next:
      Iteration helper similar to pasu_utf8_next, but in 16-bit units.
*/
PASU_INLINE pasu_status pasu_utf16_next(const pasu_uint16 *s, pasu_size len,
                                        pasu_size *pos, pasu_codepoint *out)
{
    pasu_status st;
    pasu_size used = 0;
    if (*pos >= len)
        return PASU_E_TRUNC;
    st = pasu_utf16_decode(s + *pos, len - *pos, out, &used);
    *pos += used;
    return st;
}

/* ==============================
   Conversions and length helpers
   ============================== */

/*
    pasu_utf8_to_utf16:
      Convert UTF-8 byte buffer to UTF-16 (16-bit code units).

      Returns number of 16-bit units written to dst.
      On error, returns number of units written before error and
      *status (if non-NULL) is set to:
        - PASU_E_INVALID / PASU_E_RANGE / PASU_E_SURROG on bad input
        - PASU_E_NOSPACE if dst_capacity is not enough
*/
PASUDEF pasu_size pasu_utf8_to_utf16(const pasu_uint8 *src, pasu_size src_len,
                                     pasu_uint16 *dst, pasu_size dst_capacity,
                                     pasu_status *status);

/*
    pasu_utf16_to_utf8:
      Convert UTF-16 buffer (16-bit units) to UTF-8 bytes.

      Returns number of bytes written to dst.
      Error reporting is the same as for pasu_utf8_to_utf16.
*/
PASUDEF pasu_size pasu_utf16_to_utf8(const pasu_uint16 *src, pasu_size src_len,
                                     pasu_uint8 *dst, pasu_size dst_capacity,
                                     pasu_status *status);

/*
    pasu_utf8_to_utf32:
      Convert UTF-8 byte buffer to UTF-32 (array of code points).
*/
PASUDEF pasu_size pasu_utf8_to_utf32(const pasu_uint8 *src, pasu_size src_len,
                                     pasu_codepoint *dst, pasu_size dst_capacity,
                                     pasu_status *status);

/*
    pasu_utf32_to_utf8:
      Convert UTF-32 (array of code points) to UTF-8 bytes.
*/
PASUDEF pasu_size pasu_utf32_to_utf8(const pasu_codepoint *src, pasu_size src_len,
                                     pasu_uint8 *dst, pasu_size dst_capacity,
                                     pasu_status *status);

/*
    pasu_utf16_to_utf32:
      Convert UTF-16 buffer to UTF-32 (array of code points).
*/
PASUDEF pasu_size pasu_utf16_to_utf32(const pasu_uint16 *src, pasu_size src_len,
                                      pasu_codepoint *dst, pasu_size dst_capacity,
                                      pasu_status *status);

/*
    pasu_utf32_to_utf16:
      Convert UTF-32 (array of code points) to UTF-16 buffer.
*/
PASUDEF pasu_size pasu_utf32_to_utf16(const pasu_codepoint *src, pasu_size src_len,
                                      pasu_uint16 *dst, pasu_size dst_capacity,
                                      pasu_status *status);

/*
    pasu_utf8_length:
      Returns number of Unicode scalar values (code points) in UTF-8 buffer.
      On error, returns number of successfully counted code points before
      error and sets *status (if non-NULL) to the error code.
*/
PASUDEF pasu_size pasu_utf8_length(const pasu_uint8 *str, pasu_size len,
                                   pasu_status *status);

/*
    pasu_utf16_length:
      Returns number of Unicode scalar values (code points) in UTF-16 buffer.
      Error reporting is the same as for pasu_utf8_length.
*/
PASUDEF pasu_size pasu_utf16_length(const pasu_uint16 *str, pasu_size len,
                                    pasu_status *status);

/*
    pasu_utf32_length:
      Returns number of Unicode scalar values (code points) in UTF-32 buffer.
      Each element is expected to be a valid Unicode scalar value.
*/
PASUDEF pasu_size pasu_utf32_length(const pasu_codepoint *str, pasu_size len,
                                    pasu_status *status);

/*
    C-string helpers for UTF-8 / UTF-16:
      These work with null-terminated strings and always try to keep
      dst null-terminated (when dst_capacity > 0), even on error.
*/
PASUDEF pasu_size pasu_utf8_to_utf16_cstr(const pasu_uint8 *src,
                                          pasu_uint16 *dst, pasu_size dst_capacity,
                                          pasu_status *status);

PASUDEF pasu_size pasu_utf16_to_utf8_cstr(const pasu_uint16 *src,
                                          pasu_uint8 *dst, pasu_size dst_capacity,
                                          pasu_status *status);

PASUDEF pasu_size pasu_utf8_length_cstr(const pasu_uint8 *src,
                                        pasu_status *status);

/*
    C-string helpers for UTF-32 (null-terminated array of code points).
      dst is always null-terminated when dst_capacity > 0.
*/
PASUDEF pasu_size pasu_utf8_to_utf32_cstr(const pasu_uint8 *src,
                                           pasu_codepoint *dst, pasu_size dst_capacity,
                                           pasu_status *status);

PASUDEF pasu_size pasu_utf32_to_utf8_cstr(const pasu_codepoint *src,
                                          pasu_uint8 *dst, pasu_size dst_capacity,
                                          pasu_status *status);

PASUDEF pasu_size pasu_utf16_to_utf32_cstr(const pasu_uint16 *src,
                                           pasu_codepoint *dst, pasu_size dst_capacity,
                                           pasu_status *status);

PASUDEF pasu_size pasu_utf32_to_utf16_cstr(const pasu_codepoint *src,
                                           pasu_uint16 *dst, pasu_size dst_capacity,
                                           pasu_status *status);

PASUDEF pasu_size pasu_utf32_length_cstr(const pasu_codepoint *src,
                                         pasu_status *status);

/* ==============================
   C11 types (char16_t / char32_t)
   ============================== */

#if defined(__STDC_UTF_16__) && defined(__STDC_UTF_32__) && \
    defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define PASU_USE_C11_TYPES 1
#include <uchar.h>

/*
    _c11 variants use char16_t* / char32_t* and call the base API.
    Same semantics: no malloc, errors via status, null-terminated output.
*/
PASUDEF pasu_size pasu_utf8_to_utf16_cstr_c11(const pasu_uint8 *src,
                                              char16_t *dst, pasu_size dst_capacity,
                                              pasu_status *status);

PASUDEF pasu_size pasu_utf16_to_utf8_cstr_c11(const char16_t *src,
                                              pasu_uint8 *dst, pasu_size dst_capacity,
                                              pasu_status *status);

PASUDEF pasu_size pasu_utf8_to_utf32_cstr_c11(const pasu_uint8 *src,
                                              char32_t *dst, pasu_size dst_capacity,
                                              pasu_status *status);

PASUDEF pasu_size pasu_utf32_to_utf8_cstr_c11(const char32_t *src,
                                              pasu_uint8 *dst, pasu_size dst_capacity,
                                              pasu_status *status);

PASUDEF pasu_size pasu_utf16_to_utf32_cstr_c11(const char16_t *src,
                                                char32_t *dst, pasu_size dst_capacity,
                                                pasu_status *status);

PASUDEF pasu_size pasu_utf32_to_utf16_cstr_c11(const char32_t *src,
                                                char16_t *dst, pasu_size dst_capacity,
                                                pasu_status *status);

PASUDEF pasu_size pasu_utf8_length_cstr_c11(const pasu_uint8 *src,
                                            pasu_status *status);

PASUDEF pasu_size pasu_utf32_length_cstr_c11(const char32_t *src,
                                             pasu_status *status);

#endif /* __STDC_UTF_16__ && __STDC_UTF_32__ && C11 */

/* ==============================
   Implementation
   ============================== */

#ifdef PAS_UNICODE_IMPLEMENTATION

/* --- UTF-8 helpers --- */

PASUDEF pasu_status pasu_utf8_decode(const pasu_uint8 *s, pasu_size len,
                                     pasu_codepoint *out, pasu_size *used)
{
    pasu_uint8 b0, b1, b2, b3;
    pasu_codepoint cp;

    if (out)  *out  = 0xFFFDu;
    if (used) *used = 0;

    if (!s || len == 0) {
        return PASU_E_TRUNC;
    }

    b0 = s[0];

    /* 1-byte (ASCII) */
    if (b0 < 0x80u) {
        if (out)  *out  = b0;
        if (used) *used = 1;
        return PASU_OK;
    }

    /* Determine sequence length by leading bits */
    if ((b0 & 0xE0u) == 0xC0u) {
        /* 110xxxxx, 2-byte sequence */
        if (len < 2) {
            if (used) *used = 1;
            return PASU_E_TRUNC;
        }
        b1 = s[1];
        if ((b1 & 0xC0u) != 0x80u) {
            if (used) *used = 1;
            return PASU_E_INVALID;
        }
        cp = ((pasu_codepoint)(b0 & 0x1Fu) << 6) |
             (pasu_codepoint)(b1 & 0x3Fu);

        /* Overlong (must be >= 0x80) */
        if (cp < 0x80u) {
            if (used) *used = 1;
            return PASU_E_INVALID;
        }

        if (!pasu_is_valid_scalar(cp)) {
            if (used) *used = 2;
            return PASU_E_RANGE;
        }

        if (out)  *out  = cp;
        if (used) *used = 2;
        return PASU_OK;
    }
    else if ((b0 & 0xF0u) == 0xE0u) {
        /* 1110xxxx, 3-byte sequence */
        if (len < 3) {
            if (used) *used = (len < 2 ? 1 : 2);
            return PASU_E_TRUNC;
        }
        b1 = s[1]; b2 = s[2];
        if ((b1 & 0xC0u) != 0x80u || (b2 & 0xC0u) != 0x80u) {
            if (used) *used = 1;
            return PASU_E_INVALID;
        }

        cp = ((pasu_codepoint)(b0 & 0x0Fu) << 12) |
             ((pasu_codepoint)(b1 & 0x3Fu) << 6) |
             (pasu_codepoint)(b2 & 0x3Fu);

        /* Overlong (must be >= 0x800) */
        if (cp < 0x800u) {
            if (used) *used = 1;
            return PASU_E_INVALID;
        }

        if (!pasu_is_valid_scalar(cp)) {
            if (used) *used = 3;
            return PASU_E_RANGE;
        }

        if (out)  *out  = cp;
        if (used) *used = 3;
        return PASU_OK;
    }
    else if ((b0 & 0xF8u) == 0xF0u) {
        /* 11110xxx, 4-byte sequence */
        if (len < 4) {
            if (used) *used = (len < 2 ? 1 : (len < 3 ? 2 : 3));
            return PASU_E_TRUNC;
        }
        b1 = s[1]; b2 = s[2]; b3 = s[3];
        if ((b1 & 0xC0u) != 0x80u || (b2 & 0xC0u) != 0x80u || (b3 & 0xC0u) != 0x80u) {
            if (used) *used = 1;
            return PASU_E_INVALID;
        }

        cp = ((pasu_codepoint)(b0 & 0x07u) << 18) |
             ((pasu_codepoint)(b1 & 0x3Fu) << 12) |
             ((pasu_codepoint)(b2 & 0x3Fu) << 6)  |
             (pasu_codepoint)(b3 & 0x3Fu);

        /* Overlong (must be >= 0x10000) */
        if (cp < 0x10000u) {
            if (used) *used = 1;
            return PASU_E_INVALID;
        }

        if (!pasu_is_valid_scalar(cp)) {
            if (used) *used = 4;
            return PASU_E_RANGE;
        }

        if (out)  *out  = cp;
        if (used) *used = 4;
        return PASU_OK;
    }
    else {
        /* Invalid leading byte */
        if (used) *used = 1;
        return PASU_E_INVALID;
    }
}

PASUDEF pasu_status pasu_utf8_encode(pasu_codepoint cp,
                                     pasu_uint8 out[4], pasu_size *used)
{
    if (!pasu_is_valid_scalar(cp))
        return (cp > 0x10FFFFu) ? PASU_E_RANGE : PASU_E_SURROG;

    if (cp < 0x80u) {
        out[0] = (pasu_uint8)cp;
        if (used) *used = 1;
        return PASU_OK;
    }
    else if (cp < 0x800u) {
        out[0] = (pasu_uint8)(0xC0u | (cp >> 6));
        out[1] = (pasu_uint8)(0x80u | (cp & 0x3Fu));
        if (used) *used = 2;
        return PASU_OK;
    }
    else if (cp < 0x10000u) {
        out[0] = (pasu_uint8)(0xE0u | (cp >> 12));
        out[1] = (pasu_uint8)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (pasu_uint8)(0x80u | (cp & 0x3Fu));
        if (used) *used = 3;
        return PASU_OK;
    }
    else {
        out[0] = (pasu_uint8)(0xF0u | (cp >> 18));
        out[1] = (pasu_uint8)(0x80u | ((cp >> 12) & 0x3Fu));
        out[2] = (pasu_uint8)(0x80u | ((cp >> 6) & 0x3Fu));
        out[3] = (pasu_uint8)(0x80u | (cp & 0x3Fu));
        if (used) *used = 4;
        return PASU_OK;
    }
}

/* --- UTF-16 helpers --- */

PASUDEF pasu_status pasu_utf16_decode(const pasu_uint16 *s, pasu_size len,
                                      pasu_codepoint *out, pasu_size *used)
{
    pasu_uint16 w1, w2;
    pasu_codepoint cp;

    if (out)  *out  = 0xFFFDu;
    if (used) *used = 0;

    if (!s || len == 0) {
        return PASU_E_TRUNC;
    }

    w1 = s[0];

    if (w1 < 0xD800u || w1 > 0xDFFFu) {
        /* Basic Multilingual Plane, single unit */
        cp = (pasu_codepoint)w1;
        if (!pasu_is_valid_scalar(cp)) {
            if (used) *used = 1;
            return PASU_E_RANGE;
        }
        if (out)  *out  = cp;
        if (used) *used = 1;
        return PASU_OK;
    }

    /* Surrogate pair */
    if (w1 > 0xDBFFu) {
        /* Low surrogate without leading high surrogate */
        if (used) *used = 1;
        return PASU_E_SURROG;
    }

    /* High surrogate */
    if (len < 2) {
        if (used) *used = 1;
        return PASU_E_TRUNC;
    }

    w2 = s[1];
    if (w2 < 0xDC00u || w2 > 0xDFFFu) {
        if (used) *used = 1;
        return PASU_E_SURROG;
    }

    cp = 0x10000u;
    cp += (((pasu_codepoint)w1 - 0xD800u) << 10);
    cp += ((pasu_codepoint)w2 - 0xDC00u);

    if (!pasu_is_valid_scalar(cp)) {
        if (used) *used = 2;
        return PASU_E_RANGE;
    }

    if (out)  *out  = cp;
    if (used) *used = 2;
    return PASU_OK;
}

PASUDEF pasu_status pasu_utf16_encode(pasu_codepoint cp,
                                      pasu_uint16 out[2], pasu_size *used)
{
    if (!pasu_is_valid_scalar(cp))
        return (cp > 0x10FFFFu) ? PASU_E_RANGE : PASU_E_SURROG;

    if (cp < 0x10000u) {
        out[0] = (pasu_uint16)cp;
        if (used) *used = 1;
        return PASU_OK;
    } else {
        pasu_codepoint v = cp - 0x10000u;
        out[0] = (pasu_uint16)(0xD800u + (v >> 10));
        out[1] = (pasu_uint16)(0xDC00u + (v & 0x3FFu));
        if (used) *used = 2;
        return PASU_OK;
    }
}

/* --- Conversions and length helpers --- */

PASUDEF pasu_size pasu_utf8_to_utf16(const pasu_uint8 *src, pasu_size src_len,
                                     pasu_uint16 *dst, pasu_size dst_capacity,
                                     pasu_status *status)
{
    pasu_size i = 0;
    pasu_size j = 0;
    pasu_status st = PASU_OK;

    if (status)
        *status = PASU_OK;

    if (src_len && !src) {
        if (status) *status = PASU_E_INVALID;
        return 0;
    }

    while (i < src_len) {
        pasu_codepoint cp;
        pasu_size used8 = 0;
        pasu_uint16 tmp[2];
        pasu_size used16 = 0;

        st = pasu_utf8_decode(src + i, src_len - i, &cp, &used8);
        if (st != PASU_OK) {
            if (status) *status = st;
            return j;
        }
        i += used8;

        st = pasu_utf16_encode(cp, tmp, &used16);
        if (st != PASU_OK) {
            if (status) *status = st;
            return j;
        }

        if (j + used16 > dst_capacity) {
            if (status) *status = PASU_E_NOSPACE;
            return j;
        }

        dst[j++] = tmp[0];
        if (used16 == 2)
            dst[j++] = tmp[1];
    }

    return j;
}

PASUDEF pasu_size pasu_utf16_to_utf8(const pasu_uint16 *src, pasu_size src_len,
                                     pasu_uint8 *dst, pasu_size dst_capacity,
                                     pasu_status *status)
{
    pasu_size i = 0;
    pasu_size j = 0;
    pasu_status st = PASU_OK;

    if (status)
        *status = PASU_OK;

    if (src_len && !src) {
        if (status) *status = PASU_E_INVALID;
        return 0;
    }

    while (i < src_len) {
        pasu_codepoint cp;
        pasu_size used16 = 0;
        pasu_uint8 tmp[4];
        pasu_size used8 = 0;

        st = pasu_utf16_decode(src + i, src_len - i, &cp, &used16);
        if (st != PASU_OK) {
            if (status) *status = st;
            return j;
        }
        i += used16;

        st = pasu_utf8_encode(cp, tmp, &used8);
        if (st != PASU_OK) {
            if (status) *status = st;
            return j;
        }

        if (j + used8 > dst_capacity) {
            if (status) *status = PASU_E_NOSPACE;
            return j;
        }

        dst[j++] = tmp[0];
        if (used8 > 1) dst[j++] = tmp[1];
        if (used8 > 2) dst[j++] = tmp[2];
        if (used8 > 3) dst[j++] = tmp[3];
    }

    return j;
}

PASUDEF pasu_size pasu_utf8_length(const pasu_uint8 *str, pasu_size len,
                                   pasu_status *status)
{
    pasu_size pos = 0;
    pasu_size count = 0;
    pasu_status st = PASU_OK;

    if (status)
        *status = PASU_OK;

    if (len && !str) {
        if (status) *status = PASU_E_INVALID;
        return 0;
    }

    while (pos < len) {
        pasu_codepoint cp;
        pasu_size used = 0;

        st = pasu_utf8_decode(str + pos, len - pos, &cp, &used);
        if (st != PASU_OK) {
            if (status) *status = st;
            return count;
        }

        pos += used;
        ++count;
    }

    return count;
}

PASUDEF pasu_size pasu_utf16_length(const pasu_uint16 *str, pasu_size len,
                                    pasu_status *status)
{
    pasu_size pos = 0;
    pasu_size count = 0;
    pasu_status st = PASU_OK;

    if (status)
        *status = PASU_OK;

    if (len && !str) {
        if (status) *status = PASU_E_INVALID;
        return 0;
    }

    while (pos < len) {
        pasu_codepoint cp;
        pasu_size used = 0;

        st = pasu_utf16_decode(str + pos, len - pos, &cp, &used);
        if (st != PASU_OK) {
            if (status) *status = st;
            return count;
        }

        pos += used;
        ++count;
    }

    return count;
}

PASUDEF pasu_size pasu_utf8_to_utf32(const pasu_uint8 *src, pasu_size src_len,
                                     pasu_codepoint *dst, pasu_size dst_capacity,
                                     pasu_status *status)
{
    pasu_size i = 0;
    pasu_size j = 0;
    pasu_status st = PASU_OK;

    if (status)
        *status = PASU_OK;

    if (src_len && !src) {
        if (status) *status = PASU_E_INVALID;
        return 0;
    }

    while (i < src_len) {
        pasu_codepoint cp;
        pasu_size used8 = 0;

        st = pasu_utf8_decode(src + i, src_len - i, &cp, &used8);
        if (st != PASU_OK) {
            if (status) *status = st;
            return j;
        }
        i += used8;

        if (j >= dst_capacity) {
            if (status) *status = PASU_E_NOSPACE;
            return j;
        }

        dst[j++] = cp;
    }

    return j;
}

PASUDEF pasu_size pasu_utf32_to_utf8(const pasu_codepoint *src, pasu_size src_len,
                                     pasu_uint8 *dst, pasu_size dst_capacity,
                                     pasu_status *status)
{
    pasu_size i = 0;
    pasu_size j = 0;
    pasu_status st = PASU_OK;

    if (status)
        *status = PASU_OK;

    if (src_len && !src) {
        if (status) *status = PASU_E_INVALID;
        return 0;
    }

    while (i < src_len) {
        pasu_codepoint cp = src[i++];
        pasu_uint8 tmp[4];
        pasu_size used8 = 0;

        st = pasu_utf8_encode(cp, tmp, &used8);
        if (st != PASU_OK) {
            if (status) *status = st;
            return j;
        }

        if (j + used8 > dst_capacity) {
            if (status) *status = PASU_E_NOSPACE;
            return j;
        }

        dst[j++] = tmp[0];
        if (used8 > 1) dst[j++] = tmp[1];
        if (used8 > 2) dst[j++] = tmp[2];
        if (used8 > 3) dst[j++] = tmp[3];
    }

    return j;
}

PASUDEF pasu_size pasu_utf16_to_utf32(const pasu_uint16 *src, pasu_size src_len,
                                      pasu_codepoint *dst, pasu_size dst_capacity,
                                      pasu_status *status)
{
    pasu_size i = 0;
    pasu_size j = 0;
    pasu_status st = PASU_OK;

    if (status)
        *status = PASU_OK;

    if (src_len && !src) {
        if (status) *status = PASU_E_INVALID;
        return 0;
    }

    while (i < src_len) {
        pasu_codepoint cp;
        pasu_size used16 = 0;

        st = pasu_utf16_decode(src + i, src_len - i, &cp, &used16);
        if (st != PASU_OK) {
            if (status) *status = st;
            return j;
        }
        i += used16;

        if (j >= dst_capacity) {
            if (status) *status = PASU_E_NOSPACE;
            return j;
        }

        dst[j++] = cp;
    }

    return j;
}

PASUDEF pasu_size pasu_utf32_to_utf16(const pasu_codepoint *src, pasu_size src_len,
                                      pasu_uint16 *dst, pasu_size dst_capacity,
                                      pasu_status *status)
{
    pasu_size i = 0;
    pasu_size j = 0;
    pasu_status st = PASU_OK;

    if (status)
        *status = PASU_OK;

    if (src_len && !src) {
        if (status) *status = PASU_E_INVALID;
        return 0;
    }

    while (i < src_len) {
        pasu_codepoint cp = src[i++];
        pasu_uint16 tmp[2];
        pasu_size used16 = 0;

        st = pasu_utf16_encode(cp, tmp, &used16);
        if (st != PASU_OK) {
            if (status) *status = st;
            return j;
        }

        if (j + used16 > dst_capacity) {
            if (status) *status = PASU_E_NOSPACE;
            return j;
        }

        dst[j++] = tmp[0];
        if (used16 == 2)
            dst[j++] = tmp[1];
    }

    return j;
}

PASUDEF pasu_size pasu_utf32_length(const pasu_codepoint *str, pasu_size len,
                                    pasu_status *status)
{
    pasu_size i = 0;

    if (status)
        *status = PASU_OK;

    if (len && !str) {
        if (status) *status = PASU_E_INVALID;
        return 0;
    }

    while (i < len) {
        pasu_codepoint cp = str[i];

        if (!pasu_is_valid_scalar(cp)) {
            if (status) {
                if (cp > 0x10FFFFu)
                    *status = PASU_E_RANGE;
                else if (cp >= 0xD800u && cp <= 0xDFFFu)
                    *status = PASU_E_SURROG;
                else
                    *status = PASU_E_RANGE;
            }
            return i;
        }

        ++i;
    }

    return len;
}

/* --- C-string helpers (UTF-8 / UTF-16) --- */

PASUDEF pasu_size pasu_utf8_to_utf16_cstr(const pasu_uint8 *src,
                                          pasu_uint16 *dst, pasu_size dst_capacity,
                                          pasu_status *status)
{
    pasu_size src_len = 0;
    pasu_size written = 0;
    pasu_status st = PASU_OK;

    if (status)
        *status = PASU_OK;

    if (!dst || dst_capacity == 0) {
        if (status) *status = PASU_E_NOSPACE;
        return 0;
    }

    if (!src) {
        dst[0] = 0;
        if (status) *status = PASU_E_INVALID;
        return 0;
    }

    while (src[src_len] != 0) {
        ++src_len;
    }

    if (src_len == 0) {
        dst[0] = 0;
        return 0;
    }

    if (dst_capacity <= 1) {
        dst[0] = 0;
        if (status) *status = PASU_E_NOSPACE;
        return 0;
    }

    written = pasu_utf8_to_utf16(src, src_len,
                                 dst, dst_capacity - 1,
                                 &st);

    if (written >= dst_capacity)
        written = dst_capacity - 1;

    dst[written] = 0;

    if (status)
        *status = st;

    return written;
}

PASUDEF pasu_size pasu_utf16_to_utf8_cstr(const pasu_uint16 *src,
                                          pasu_uint8 *dst, pasu_size dst_capacity,
                                          pasu_status *status)
{
    pasu_size src_len = 0;
    pasu_size written = 0;
    pasu_status st = PASU_OK;

    if (status)
        *status = PASU_OK;

    if (!dst || dst_capacity == 0) {
        if (status) *status = PASU_E_NOSPACE;
        return 0;
    }

    if (!src) {
        dst[0] = 0;
        if (status) *status = PASU_E_INVALID;
        return 0;
    }

    while (src[src_len] != 0) {
        ++src_len;
    }

    if (src_len == 0) {
        dst[0] = 0;
        return 0;
    }

    if (dst_capacity <= 1) {
        dst[0] = 0;
        if (status) *status = PASU_E_NOSPACE;
        return 0;
    }

    written = pasu_utf16_to_utf8(src, src_len,
                                 dst, dst_capacity - 1,
                                 &st);

    if (written >= dst_capacity)
        written = dst_capacity - 1;

    dst[written] = 0;

    if (status)
        *status = st;

    return written;
}

PASUDEF pasu_size pasu_utf8_length_cstr(const pasu_uint8 *src,
                                        pasu_status *status)
{
    pasu_size len = 0;

    if (status)
        *status = PASU_OK;

    if (!src) {
        if (status) *status = PASU_E_INVALID;
        return 0;
    }

    while (src[len] != 0) {
        ++len;
    }

    return pasu_utf8_length(src, len, status);
}

/* --- C-string helpers for UTF-32 --- */

PASUDEF pasu_size pasu_utf8_to_utf32_cstr(const pasu_uint8 *src,
                                           pasu_codepoint *dst, pasu_size dst_capacity,
                                           pasu_status *status)
{
    pasu_size src_len = 0;
    pasu_size written = 0;
    pasu_status st = PASU_OK;

    if (status)
        *status = PASU_OK;

    if (!dst || dst_capacity == 0) {
        if (status) *status = PASU_E_NOSPACE;
        return 0;
    }

    if (!src) {
        dst[0] = 0;
        if (status) *status = PASU_E_INVALID;
        return 0;
    }

    while (src[src_len] != 0) {
        ++src_len;
    }

    if (src_len == 0) {
        dst[0] = 0;
        return 0;
    }

    if (dst_capacity <= 1) {
        dst[0] = 0;
        if (status) *status = PASU_E_NOSPACE;
        return 0;
    }

    written = pasu_utf8_to_utf32(src, src_len,
                                 dst, dst_capacity - 1,
                                 &st);

    if (written >= dst_capacity)
        written = dst_capacity - 1;

    dst[written] = 0;

    if (status)
        *status = st;

    return written;
}

PASUDEF pasu_size pasu_utf32_to_utf8_cstr(const pasu_codepoint *src,
                                          pasu_uint8 *dst, pasu_size dst_capacity,
                                          pasu_status *status)
{
    pasu_size src_len = 0;
    pasu_size written = 0;
    pasu_status st = PASU_OK;

    if (status)
        *status = PASU_OK;

    if (!dst || dst_capacity == 0) {
        if (status) *status = PASU_E_NOSPACE;
        return 0;
    }

    if (!src) {
        dst[0] = 0;
        if (status) *status = PASU_E_INVALID;
        return 0;
    }

    while (src[src_len] != 0) {
        ++src_len;
    }

    if (src_len == 0) {
        dst[0] = 0;
        return 0;
    }

    if (dst_capacity <= 1) {
        dst[0] = 0;
        if (status) *status = PASU_E_NOSPACE;
        return 0;
    }

    written = pasu_utf32_to_utf8(src, src_len,
                                 dst, dst_capacity - 1,
                                 &st);

    if (written >= dst_capacity)
        written = dst_capacity - 1;

    dst[written] = 0;

    if (status)
        *status = st;

    return written;
}

PASUDEF pasu_size pasu_utf16_to_utf32_cstr(const pasu_uint16 *src,
                                           pasu_codepoint *dst, pasu_size dst_capacity,
                                           pasu_status *status)
{
    pasu_size src_len = 0;
    pasu_size written = 0;
    pasu_status st = PASU_OK;

    if (status)
        *status = PASU_OK;

    if (!dst || dst_capacity == 0) {
        if (status) *status = PASU_E_NOSPACE;
        return 0;
    }

    if (!src) {
        dst[0] = 0;
        if (status) *status = PASU_E_INVALID;
        return 0;
    }

    while (src[src_len] != 0) {
        ++src_len;
    }

    if (src_len == 0) {
        dst[0] = 0;
        return 0;
    }

    if (dst_capacity <= 1) {
        dst[0] = 0;
        if (status) *status = PASU_E_NOSPACE;
        return 0;
    }

    written = pasu_utf16_to_utf32(src, src_len,
                                 dst, dst_capacity - 1,
                                 &st);

    if (written >= dst_capacity)
        written = dst_capacity - 1;

    dst[written] = 0;

    if (status)
        *status = st;

    return written;
}

PASUDEF pasu_size pasu_utf32_to_utf16_cstr(const pasu_codepoint *src,
                                           pasu_uint16 *dst, pasu_size dst_capacity,
                                           pasu_status *status)
{
    pasu_size src_len = 0;
    pasu_size written = 0;
    pasu_status st = PASU_OK;

    if (status)
        *status = PASU_OK;

    if (!dst || dst_capacity == 0) {
        if (status) *status = PASU_E_NOSPACE;
        return 0;
    }

    if (!src) {
        dst[0] = 0;
        if (status) *status = PASU_E_INVALID;
        return 0;
    }

    while (src[src_len] != 0) {
        ++src_len;
    }

    if (src_len == 0) {
        dst[0] = 0;
        return 0;
    }

    if (dst_capacity <= 1) {
        dst[0] = 0;
        if (status) *status = PASU_E_NOSPACE;
        return 0;
    }

    written = pasu_utf32_to_utf16(src, src_len,
                                  dst, dst_capacity - 1,
                                  &st);

    if (written >= dst_capacity)
        written = dst_capacity - 1;

    dst[written] = 0;

    if (status)
        *status = st;

    return written;
}

PASUDEF pasu_size pasu_utf32_length_cstr(const pasu_codepoint *src,
                                         pasu_status *status)
{
    pasu_size len = 0;

    if (status)
        *status = PASU_OK;

    if (!src) {
        if (status) *status = PASU_E_INVALID;
        return 0;
    }

    while (src[len] != 0) {
        ++len;
    }

    return pasu_utf32_length(src, len, status);
}

#if defined(PASU_USE_C11_TYPES)

/* --- C11 (char16_t / char32_t) wrappers --- */

PASUDEF pasu_size pasu_utf8_to_utf16_cstr_c11(const pasu_uint8 *src,
                                              char16_t *dst, pasu_size dst_capacity,
                                              pasu_status *status)
{
    return pasu_utf8_to_utf16_cstr(src, (pasu_uint16 *)dst, dst_capacity, status);
}

PASUDEF pasu_size pasu_utf16_to_utf8_cstr_c11(const char16_t *src,
                                              pasu_uint8 *dst, pasu_size dst_capacity,
                                              pasu_status *status)
{
    return pasu_utf16_to_utf8_cstr((const pasu_uint16 *)src, dst, dst_capacity, status);
}

PASUDEF pasu_size pasu_utf8_to_utf32_cstr_c11(const pasu_uint8 *src,
                                              char32_t *dst, pasu_size dst_capacity,
                                              pasu_status *status)
{
    return pasu_utf8_to_utf32_cstr(src, (pasu_codepoint *)dst, dst_capacity, status);
}

PASUDEF pasu_size pasu_utf32_to_utf8_cstr_c11(const char32_t *src,
                                              pasu_uint8 *dst, pasu_size dst_capacity,
                                              pasu_status *status)
{
    return pasu_utf32_to_utf8_cstr((const pasu_codepoint *)src, dst, dst_capacity, status);
}

PASUDEF pasu_size pasu_utf16_to_utf32_cstr_c11(const char16_t *src,
                                                char32_t *dst, pasu_size dst_capacity,
                                                pasu_status *status)
{
    return pasu_utf16_to_utf32_cstr((const pasu_uint16 *)src, (pasu_codepoint *)dst, dst_capacity, status);
}

PASUDEF pasu_size pasu_utf32_to_utf16_cstr_c11(const char32_t *src,
                                                char16_t *dst, pasu_size dst_capacity,
                                                pasu_status *status)
{
    return pasu_utf32_to_utf16_cstr((const pasu_codepoint *)src, (pasu_uint16 *)dst, dst_capacity, status);
}

PASUDEF pasu_size pasu_utf8_length_cstr_c11(const pasu_uint8 *src,
                                            pasu_status *status)
{
    return pasu_utf8_length_cstr(src, status);
}

PASUDEF pasu_size pasu_utf32_length_cstr_c11(const char32_t *src,
                                             pasu_status *status)
{
    return pasu_utf32_length_cstr((const pasu_codepoint *)src, status);
}

#endif /* PASU_USE_C11_TYPES */

#endif /* PAS_UNICODE_IMPLEMENTATION */

#endif /* PAS_UNICODE_H */


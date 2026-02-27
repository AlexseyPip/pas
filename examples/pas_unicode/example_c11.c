/*
    example_c11.c - C11 char16_t / char32_t APIs (when PASU_USE_C11_TYPES is defined)
    From repo root: gcc -std=c11 -o examples/pas_unicode/example_c11 examples/pas_unicode/example_c11.c -I.
*/

#define PAS_UNICODE_IMPLEMENTATION
#include "pas_unicode.h"
#include <stdio.h>

#if defined(PASU_USE_C11_TYPES)
#include <uchar.h>
#endif

int main(void)
{
#if !defined(PASU_USE_C11_TYPES)
    (void)printf("PASU_USE_C11_TYPES not defined. Skip.\n");
    return 0;
#else
    const pasu_uint8 *utf8_src = (const pasu_uint8 *)"Hi \xF0\x9F\x98\x80";
    char16_t utf16_buf[64];
    char32_t utf32_buf[64];
    pasu_uint8 utf8_back[64];
    pasu_status st;
    pasu_size n16, n32, n8, cp8, cp32;

    n16 = pasu_utf8_to_utf16_cstr_c11(utf8_src, utf16_buf, 64, &st);
    if (st != PASU_OK) {
        (void)fprintf(stderr, "utf8_to_utf16_cstr_c11 error: %d\n", (int)st);
        return 1;
    }

    n32 = pasu_utf16_to_utf32_cstr_c11(utf16_buf, utf32_buf, 64, &st);
    if (st != PASU_OK) {
        (void)fprintf(stderr, "utf16_to_utf32_cstr_c11 error: %d\n", (int)st);
        return 1;
    }

    cp8 = pasu_utf8_length_cstr_c11(utf8_src, &st);
    if (st != PASU_OK) return 1;
    cp32 = pasu_utf32_length_cstr_c11(utf32_buf, &st);
    if (st != PASU_OK) return 1;
    (void)printf("UTF-8 code points: %zu, UTF-32 code points: %zu\n", (size_t)cp8, (size_t)cp32);

    n8 = pasu_utf32_to_utf8_cstr_c11(utf32_buf, utf8_back, 64, &st);
    if (st != PASU_OK) {
        (void)fprintf(stderr, "utf32_to_utf8_cstr_c11 error: %d\n", (int)st);
        return 1;
    }
    (void)printf("UTF-8 back: %s\n", (const char *)utf8_back);

    (void)n16;
    (void)n32;
    (void)n8;
    return 0;
#endif
}

/*
    example_minimal.c - Basic buffer conversions using pas_unicode.h
    From repo root: gcc -o examples/pas_unicode/example_minimal examples/pas_unicode/example_minimal.c -I.
*/

#define PAS_UNICODE_IMPLEMENTATION
#include "pas_unicode.h"
#include <stdio.h>

int main(void)
{
    const pasu_uint8 utf8_src[] = { 'H', 'i', ',', ' ', 0xF0, 0x9F, 0x98, 0x80 };
    pasu_uint16 utf16_buf[32];
    pasu_codepoint utf32_buf[32];
    pasu_uint8 utf8_back[32];
    pasu_status st;
    pasu_size u16_len, u32_len, u8_len, cp_count;

    u16_len = pasu_utf8_to_utf16(utf8_src, sizeof(utf8_src),
                                 utf16_buf, 32, &st);
    if (st != PASU_OK) {
        (void)fprintf(stderr, "utf8_to_utf16 error: %d\n", (int)st);
        return 1;
    }

    u32_len = pasu_utf16_to_utf32(utf16_buf, u16_len,
                                  utf32_buf, 32, &st);
    if (st != PASU_OK) {
        (void)fprintf(stderr, "utf16_to_utf32 error: %d\n", (int)st);
        return 1;
    }

    cp_count = pasu_utf32_length(utf32_buf, u32_len, &st);
    if (st != PASU_OK) {
        (void)fprintf(stderr, "utf32_length error: %d\n", (int)st);
        return 1;
    }
    (void)printf("Code points: %zu\n", (size_t)cp_count);

    u8_len = pasu_utf32_to_utf8(utf32_buf, u32_len,
                                utf8_back, 32, &st);
    if (st != PASU_OK) {
        (void)fprintf(stderr, "utf32_to_utf8 error: %d\n", (int)st);
        return 1;
    }

    (void)printf("UTF-8 bytes back: %zu\n", (size_t)u8_len);
    return 0;
}

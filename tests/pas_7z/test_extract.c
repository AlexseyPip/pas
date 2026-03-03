/*
    test_extract.c - Test pas_7z_extract (Copy/stored only).
    From repo root: gcc -o tests/pas_7z/test_extract tests/pas_7z/test_extract.c -I.
*/

#define PAS_7Z_IMPLEMENTATION
#include "pas_7z.h"
#include <stdio.h>
#include <string.h>

static int g_failed, g_assertions;

#define ASSERT(cond) do { \
    ++g_assertions; \
    if (!(cond)) { (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failed; } \
} while (0)
#define ASSERT_EQ(a, b) ASSERT((a) == (b))

static const unsigned char minimal_7z[] = {
    0x37,0x7A,0xBC,0xAF,0x27,0x1C,0x00,0x02, 0x00,0x00,0x00,0x00,
    0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x2D,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x68,0x65,0x6C,0x6C,0x6F,
    0x01,0x04,0x06,0x00,0x01,0x09,0x05,0x00,
    0x07,0x0B,0x01,0x00,0x01,0x01,0x00,0x0C,0x05,0x00,
    0x08,0x0D,0x01,0x09,0x05,0x00,0x00,0x05,0x01,0x0E,0x00,0x11,0x00,
    0x61,0x00,0x2E,0x00,0x74,0x00,0x78,0x00,0x74,0x00,0x00,0x00,0x00,0x00
};

int main(void) {
    pas_7z_status status;
    pas_7z_t *arch;
    pas_7z_file_t *file;
    char out[64];
    size_t n;

    g_failed = 0;
    g_assertions = 0;

    arch = pas_7z_open(minimal_7z, sizeof(minimal_7z), &status);
    if (!arch || status != PAS_7Z_OK) {
        (void)fprintf(stderr, "pas_7z_open failed: %d\n", status);
        return 1;
    }

    file = pas_7z_find(arch, "a.txt");
    ASSERT(file != NULL);
    ASSERT(!pas_7z_is_compressed(file));

    n = pas_7z_extract(file, out, sizeof(out), &status);
    ASSERT(status == PAS_7Z_OK);
    ASSERT_EQ(n, 5u);
    ASSERT(memcmp(out, "hello", 5) == 0);

    n = pas_7z_extract(file, out, 3, &status);
    ASSERT(status == PAS_7Z_E_NOSPACE);
    ASSERT_EQ(n, 0u);

    if (g_failed) {
        (void)fprintf(stderr, "Total: %d assertions, %d failed\n", g_assertions, g_failed);
        return 1;
    }
    (void)printf("All %d assertions passed.\n", g_assertions);
    return 0;
}

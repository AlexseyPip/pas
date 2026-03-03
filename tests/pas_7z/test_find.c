/*
    test_find.c - Test pas_7z_find.
    From repo root: gcc -o tests/pas_7z/test_find tests/pas_7z/test_find.c -I.
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

/* Minimal valid 7z Store: one file "a.txt" = "hello" */
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

    g_failed = 0;
    g_assertions = 0;

    arch = pas_7z_open(minimal_7z, sizeof(minimal_7z), &status);
    if (!arch || status != PAS_7Z_OK) {
        (void)fprintf(stderr, "pas_7z_open failed: %d\n", status);
        return 1;
    }

    file = pas_7z_find(arch, "a.txt");
    ASSERT(file != NULL);
    ASSERT(strcmp(pas_7z_name(file), "a.txt") == 0);
    ASSERT(pas_7z_size(file) == 5);
    ASSERT(!pas_7z_is_dir(file));
    ASSERT(!pas_7z_is_compressed(file));

    file = pas_7z_find(arch, "nonexistent");
    ASSERT(file == NULL);

    if (g_failed) {
        (void)fprintf(stderr, "Total: %d assertions, %d failed\n", g_assertions, g_failed);
        return 1;
    }
    (void)printf("All %d assertions passed.\n", g_assertions);
    return 0;
}

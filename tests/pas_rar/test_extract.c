/*
    test_extract.c - Test pas_rar_extract (store/uncompressed only).
    From repo root: gcc -o tests/pas_rar/test_extract tests/pas_rar/test_extract.c -I.
*/

#define PAS_RAR_IMPLEMENTATION
#include "pas_rar.h"
#include <stdio.h>
#include <string.h>

static int g_failed, g_assertions;

#define ASSERT(cond) do { \
    ++g_assertions; \
    if (!(cond)) { (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failed; } \
} while (0)
#define ASSERT_EQ(a, b) ASSERT((a) == (b))

/* Minimal RAR4 with "a.txt" containing "hello". */
static const unsigned char minimal_rar4[] = {
    0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x00,
    0x00, 0x00, 0x73, 0x00, 0x00, 0x07, 0x00,
    0x00, 0x00, 0x74, 0x00, 0x80, 0x29, 0x00, 0x05, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x30,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x61, 0x2e, 0x74, 0x78, 0x74,
    0x68, 0x65, 0x6c, 0x6c, 0x6f
};

int main(void) {
    pas_rar_status status;
    pas_rar_t *rar;
    pas_rar_file_t *file;
    char out[64];
    size_t n;

    g_failed = 0;
    g_assertions = 0;

    rar = pas_rar_open(minimal_rar4, sizeof(minimal_rar4), &status);
    ASSERT(rar != NULL);

    file = pas_rar_find(rar, "a.txt");
    ASSERT(file != NULL);
    ASSERT(!pas_rar_is_compressed(file));

    n = pas_rar_extract(file, out, sizeof(out), &status);
    ASSERT(status == PAS_RAR_OK);
    ASSERT_EQ(n, 5u);
    ASSERT(memcmp(out, "hello", 5) == 0);

    n = pas_rar_extract(file, out, 3, &status);
    ASSERT(status == PAS_RAR_E_NOSPACE);
    ASSERT_EQ(n, 0u);

    if (g_failed) {
        (void)fprintf(stderr, "Total: %d assertions, %d failed\n", g_assertions, g_failed);
        return 1;
    }
    (void)printf("All %d assertions passed.\n", g_assertions);
    return 0;
}

/*
    test_open.c - Test pas_7z_open.
    From repo root: gcc -o tests/pas_7z/test_open tests/pas_7z/test_open.c -I.
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

/* Valid 7z signature (6 bytes) + version + start header - open still fails without full header */
static const unsigned char sig_only[] = { '7', 'z', 0xBC, 0xAF, 0x27, 0x1C, 0, 2, 0, 0, 0, 0 };

static void test_invalid_data(void) {
    pas_7z_t *arch;
    pas_7z_status status;
    unsigned char bad[] = { 0, 0, 0 };

    arch = pas_7z_open(bad, sizeof(bad), &status);
    ASSERT(arch == NULL);
    ASSERT(status == PAS_7Z_E_INVALID);

    arch = pas_7z_open(NULL, 100, &status);
    ASSERT(arch == NULL);
}

static void test_too_small(void) {
    pas_7z_t *arch;
    pas_7z_status status;

    arch = pas_7z_open(sig_only, 6, &status);
    ASSERT(arch == NULL);
}

int main(void) {
    g_failed = 0;
    g_assertions = 0;
    test_invalid_data();
    test_too_small();
    if (g_failed) {
        (void)fprintf(stderr, "Total: %d assertions, %d failed\n", g_assertions, g_failed);
        return 1;
    }
    (void)printf("All %d assertions passed.\n", g_assertions);
    return 0;
}

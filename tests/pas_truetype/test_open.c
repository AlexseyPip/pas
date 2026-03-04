/*
    test_open.c - Basic tests for pas_truetype.
    From repo root:
        gcc -o tests/pas_truetype/test_open tests/pas_truetype/test_open.c -I.
*/

#define PAS_TRUETYPE_IMPLEMENTATION
#include "pas_truetype.h"

#include <stdio.h>

static int g_failed, g_assertions;

#define ASSERT(cond) do { \
    ++g_assertions; \
    if (!(cond)) { (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failed; } \
} while (0)
#define ASSERT_EQ(a, b) ASSERT((a) == (b))

static void test_invalid_buffer(void) {
    pas_tt_font_t font;
    pas_tt_status status;
    unsigned char bad1[] = { 0, 1, 2 }; /* too small */
    unsigned char bad2[12] = { 0 };     /* not a valid sfnt version */

    ASSERT_EQ(pas_tt_init_font(&font, bad1, sizeof(bad1), &status), 0);
    ASSERT_EQ(status, PAS_TT_E_INVALID);

    ASSERT_EQ(pas_tt_init_font(&font, bad2, sizeof(bad2), &status), 0);
    ASSERT_EQ(status, PAS_TT_E_INVALID);
}

static void test_null_pointers(void) {
    pas_tt_font_t font;
    pas_tt_status status;

    ASSERT_EQ(pas_tt_init_font(&font, NULL, 100, &status), 0);
    ASSERT_EQ(status, PAS_TT_E_INVALID);
}

int main(void) {
    g_failed = 0;
    g_assertions = 0;

    test_invalid_buffer();
    test_null_pointers();

    if (g_failed) {
        (void)fprintf(stderr, "Total: %d assertions, %d failed\n", g_assertions, g_failed);
        return 1;
    }
    (void)printf("All %d assertions passed.\n", g_assertions);
    return 0;
}


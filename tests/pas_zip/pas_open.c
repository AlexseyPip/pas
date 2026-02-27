/*
    test_open.c - Test pas_zip_open.
    From repo root: gcc -o tests/pas_zip/test_open tests/pas_zip/test_open.c -I.
*/

#define PAS_ZIP_IMPLEMENTATION
#include "pas_zip.h"
#include <stdio.h>
#include <string.h>

static int g_failed, g_assertions;

#define ASSERT(cond) do { \
    ++g_assertions; \
    if (!(cond)) { (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failed; } \
} while (0)
#define ASSERT_EQ(a, b) ASSERT((a) == (b))

static void test_valid_zip(void) {
    unsigned char zip_buf[1024];
    size_t written;
    pas_zip_status status;
    pas_zip_t *zip;

    written = pas_zip_create(
        (const char *[]){ "a.txt" },
        (const void *[]){ "hello" },
        (size_t[]){ 5 },
        1, zip_buf, sizeof(zip_buf), &status
    );
    ASSERT(status == PAS_ZIP_OK);
    ASSERT(written > 0);

    zip = pas_zip_open(zip_buf, written, &status);
    ASSERT(zip != NULL);
    ASSERT(status == PAS_ZIP_OK);
    ASSERT(zip->num_entries == 1);
}

static void test_invalid_data(void) {
    pas_zip_t *zip;
    pas_zip_status status;
    unsigned char bad[] = { 0, 0, 0 };

    zip = pas_zip_open(bad, sizeof(bad), &status);
    ASSERT(zip == NULL);
    ASSERT(status == PAS_ZIP_E_INVALID);

    zip = pas_zip_open(NULL, 100, &status);
    ASSERT(zip == NULL);
}

int main(void) {
    g_failed = 0;
    g_assertions = 0;
    test_valid_zip();
    test_invalid_data();
    if (g_failed) {
        (void)fprintf(stderr, "Total: %d assertions, %d failed\n", g_assertions, g_failed);
        return 1;
    }
    (void)printf("All %d assertions passed.\n", g_assertions);
    return 0;
}

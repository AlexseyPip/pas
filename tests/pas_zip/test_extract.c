/*
    test_extract.c - Test pas_zip_extract (Store only).
    From repo root: gcc -o tests/pas_zip/test_extract tests/pas_zip/test_extract.c -I.
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

int main(void) {
    unsigned char zip_buf[1024];
    size_t written;
    pas_zip_status status;
    pas_zip_t *zip;
    pas_zip_file_t *file;
    char out[64];
    size_t n;

    g_failed = 0;
    g_assertions = 0;

    written = pas_zip_create(
        (const char *[]){ "test.txt" },
        (const void *[]){ "Hello, ZIP!" },
        (size_t[]){ 11 },
        1, zip_buf, sizeof(zip_buf), &status
    );
    ASSERT(status == PAS_ZIP_OK);
    ASSERT(written > 0);

    zip = pas_zip_open(zip_buf, written, &status);
    ASSERT(zip != NULL);

    file = pas_zip_find(zip, "test.txt");
    ASSERT(file != NULL);
    ASSERT(!pas_zip_is_compressed(file));

    n = pas_zip_extract(file, out, sizeof(out), &status);
    ASSERT(status == PAS_ZIP_OK);
    ASSERT_EQ(n, 11u);
    ASSERT(memcmp(out, "Hello, ZIP!", 11) == 0);

    n = pas_zip_extract(file, out, 5, &status);
    ASSERT(status == PAS_ZIP_E_NOSPACE);
    ASSERT_EQ(n, 0u);

    if (g_failed) {
        (void)fprintf(stderr, "Total: %d assertions, %d failed\n", g_assertions, g_failed);
        return 1;
    }
    (void)printf("All %d assertions passed.\n", g_assertions);
    return 0;
}

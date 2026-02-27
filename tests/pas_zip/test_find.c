/*
    test_find.c - Test pas_zip_find.
    From repo root: gcc -o tests/pas_zip/test_find tests/pas_zip/test_find.c -I.
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

    g_failed = 0;
    g_assertions = 0;

    written = pas_zip_create(
        (const char *[]){ "foo.txt", "bar.bin", "baz" },
        (const void *[]){ "a", "bb", "ccc" },
        (size_t[]){ 1, 2, 3 },
        3, zip_buf, sizeof(zip_buf), &status
    );
    ASSERT(status == PAS_ZIP_OK);
    ASSERT(written > 0);

    zip = pas_zip_open(zip_buf, written, &status);
    ASSERT(zip != NULL);
    ASSERT(status == PAS_ZIP_OK);

    file = pas_zip_find(zip, "foo.txt");
    ASSERT(file != NULL);
    ASSERT(strcmp(pas_zip_name(file), "foo.txt") == 0);
    ASSERT_EQ(pas_zip_size(file), 1u);

    file = pas_zip_find(zip, "bar.bin");
    ASSERT(file != NULL);
    ASSERT_EQ(pas_zip_size(file), 2u);

    file = pas_zip_find(zip, "baz");
    ASSERT(file != NULL);
    ASSERT_EQ(pas_zip_size(file), 3u);

    file = pas_zip_find(zip, "nonexistent");
    ASSERT(file == NULL);

    file = pas_zip_find(zip, "FOO.TXT");
    ASSERT(file == NULL);

    if (g_failed) {
        (void)fprintf(stderr, "Total: %d assertions, %d failed\n", g_assertions, g_failed);
        return 1;
    }
    (void)printf("All %d assertions passed.\n", g_assertions);
    return 0;
}

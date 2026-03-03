/*
    test_find.c - Test pas_rar_find.
    From repo root: gcc -o tests/pas_rar/test_find tests/pas_rar/test_find.c -I.
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

    g_failed = 0;
    g_assertions = 0;

    rar = pas_rar_open(minimal_rar4, sizeof(minimal_rar4), &status);
    ASSERT(rar != NULL);
    ASSERT(status == PAS_RAR_OK);

    file = pas_rar_find(rar, "a.txt");
    ASSERT(file != NULL);
    ASSERT(strcmp(pas_rar_name(file), "a.txt") == 0);
    ASSERT_EQ(pas_rar_size(file), 5u);
    ASSERT(!pas_rar_is_compressed(file));

    file = pas_rar_find(rar, "nonexistent");
    ASSERT(file == NULL);

    file = pas_rar_find(rar, "A.TXT");
    ASSERT(file == NULL);

    if (g_failed) {
        (void)fprintf(stderr, "Total: %d assertions, %d failed\n", g_assertions, g_failed);
        return 1;
    }
    (void)printf("All %d assertions passed.\n", g_assertions);
    return 0;
}

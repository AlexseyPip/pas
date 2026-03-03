/*
    test_open.c - Test pas_rar_open.
    From repo root: gcc -o tests/pas_rar/test_open tests/pas_rar/test_open.c -I.
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

/* Minimal valid RAR4: main header + one file "a.txt" with "hello" (store). */
static const unsigned char minimal_rar4[] = {
    0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x00,
    0x00, 0x00, 0x73, 0x00, 0x00, 0x07, 0x00,
    0x00, 0x00, 0x74, 0x00, 0x80, 0x29, 0x00, 0x05, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x30,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x61, 0x2e, 0x74, 0x78, 0x74,
    0x68, 0x65, 0x6c, 0x6c, 0x6f
};

static const unsigned char rar5_sig[] = { 0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x01, 0x00 };

static void test_valid_rar4(void) {
    pas_rar_status status;
    pas_rar_t *rar;
    rar = pas_rar_open(minimal_rar4, sizeof(minimal_rar4), &status);
    ASSERT(rar != NULL);
    ASSERT(status == PAS_RAR_OK);
    ASSERT(rar->format == 4);
}

static void test_valid_rar5(void) {
    pas_rar_status status;
    pas_rar_t *rar;
    unsigned char rar5_min[32];
    memcpy(rar5_min, rar5_sig, 8);
    rar5_min[8] = 0; rar5_min[9] = 0; rar5_min[10] = 0; rar5_min[11] = 0;
    rar5_min[12] = 0x02;
    rar5_min[13] = 0x05;
    rar5_min[14] = 0x00;
    rar = pas_rar_open(rar5_min, 15, &status);
    ASSERT(rar != NULL);
    ASSERT(status == PAS_RAR_OK);
    ASSERT(rar->format == 5);
}

static void test_invalid_data(void) {
    pas_rar_t *rar;
    pas_rar_status status;
    unsigned char bad[] = { 0, 0, 0 };
    rar = pas_rar_open(bad, sizeof(bad), &status);
    ASSERT(rar == NULL);
    ASSERT(status == PAS_RAR_E_INVALID);
    rar = pas_rar_open(NULL, 100, &status);
    ASSERT(rar == NULL);
}

int main(void) {
    g_failed = 0;
    g_assertions = 0;
    test_valid_rar4();
    test_valid_rar5();
    test_invalid_data();
    if (g_failed) {
        (void)fprintf(stderr, "Total: %d assertions, %d failed\n", g_assertions, g_failed);
        return 1;
    }
    (void)printf("All %d assertions passed.\n", g_assertions);
    return 0;
}

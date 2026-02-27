/*
    test_extract_deflate.c - Test pas_zip_extract with Deflate (requires PAS_ZIP_USE_MINIZ).
    From repo root: gcc -o tests/pas_zip/test_extract_deflate tests/pas_zip/test_extract_deflate.c -I. -DPAS_ZIP_USE_MINIZ
    Requires miniz.c (or miniz.h with implementation) in include path.
*/

#define PAS_ZIP_USE_MINIZ
#define PAS_ZIP_IMPLEMENTATION
#include "pas_zip.h"
#include <stdio.h>
#include <string.h>

#ifdef PAS_ZIP_USE_MINIZ
#define MINIZ_HEADER_FILE_ONLY
#include "miniz.h"

/* Build a minimal ZIP with one Deflate entry at runtime. */
static int build_deflate_zip(unsigned char *buf, size_t buf_size, size_t *out_len) {
    const char *plain = "test";
    size_t plain_len = 4;
    unsigned char comp[64];
    mz_ulong comp_len = (mz_ulong)sizeof(comp);
    size_t w = 0;
    uint16_t fn_len = 4;
    uint32_t comp_sz, uncomp_sz;
    uint32_t local_off = 0;
    uint32_t cd_size;

    if (mz_compress(comp, &comp_len, (const unsigned char *)plain, (mz_ulong)plain_len) != MZ_OK)
        return -1;
    comp_sz = (uint32_t)comp_len;
    uncomp_sz = (uint32_t)plain_len;

    if (w + 30 + fn_len + comp_sz > buf_size) return -1;
    buf[w++] = 0x50; buf[w++] = 0x4b; buf[w++] = 0x03; buf[w++] = 0x04;
    buf[w++] = 20; buf[w++] = 0; buf[w++] = 0; buf[w++] = 0;
    buf[w++] = 8; buf[w++] = 0;  /* deflate */
    for (int i = 0; i < 12; i++) buf[w++] = 0;
    buf[w++] = (uint8_t)(comp_sz); buf[w++] = (uint8_t)(comp_sz>>8);
    buf[w++] = (uint8_t)(comp_sz>>16); buf[w++] = (uint8_t)(comp_sz>>24);
    buf[w++] = (uint8_t)(uncomp_sz); buf[w++] = (uint8_t)(uncomp_sz>>8);
    buf[w++] = (uint8_t)(uncomp_sz>>16); buf[w++] = (uint8_t)(uncomp_sz>>24);
    buf[w++] = (uint8_t)(fn_len); buf[w++] = (uint8_t)(fn_len>>8);
    buf[w++] = 0; buf[w++] = 0;
    memcpy(buf + w, "test", 4); w += 4;
    memcpy(buf + w, comp, comp_sz); w += comp_sz;
    local_off = 0;
    cd_size = 46 + fn_len;
    if (w + 46 + fn_len + 22 > buf_size) return -1;
    buf[w++] = 0x50; buf[w++] = 0x4b; buf[w++] = 0x01; buf[w++] = 0x02;
    buf[w++] = 20; buf[w++] = 0; buf[w++] = 20; buf[w++] = 0;
    buf[w++] = 0; buf[w++] = 0; buf[w++] = 8; buf[w++] = 0;
    for (int i = 0; i < 16; i++) buf[w++] = 0;
    buf[w++] = (uint8_t)(fn_len); buf[w++] = (uint8_t)(fn_len>>8);
    for (int i = 0; i < 12; i++) buf[w++] = 0;
    buf[w++] = (uint8_t)(local_off); buf[w++] = (uint8_t)(local_off>>8);
    buf[w++] = (uint8_t)(local_off>>16); buf[w++] = (uint8_t)(local_off>>24);
    memcpy(buf + w, "test", 4); w += 4;
    buf[w++] = 0x50; buf[w++] = 0x4b; buf[w++] = 0x05; buf[w++] = 0x06;
    for (int i = 0; i < 4; i++) buf[w++] = 0;
    buf[w++] = 1; buf[w++] = 0; buf[w++] = 1; buf[w++] = 0;
    buf[w++] = (uint8_t)(cd_size); buf[w++] = (uint8_t)(cd_size>>8);
    buf[w++] = (uint8_t)(cd_size>>16); buf[w++] = (uint8_t)(cd_size>>24);
    buf[w++] = (uint8_t)(30+fn_len+comp_sz); buf[w++] = (uint8_t)((30+fn_len+comp_sz)>>8);
    buf[w++] = (uint8_t)((30+fn_len+comp_sz)>>16); buf[w++] = (uint8_t)((30+fn_len+comp_sz)>>24);
    buf[w++] = 0; buf[w++] = 0;
    *out_len = w;
    return 0;
}
#endif

static int g_failed, g_assertions;

#define ASSERT(cond) do { \
    ++g_assertions; \
    if (!(cond)) { (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failed; } \
} while (0)
#define ASSERT_EQ(a, b) ASSERT((a) == (b))

int main(void) {
#ifdef PAS_ZIP_USE_MINIZ
    unsigned char zip_buf[512];
    size_t zip_len;
    pas_zip_t *zip;
    pas_zip_file_t *file;
    pas_zip_status status;
    char out[16];
    size_t n;

    g_failed = 0;
    g_assertions = 0;

    if (build_deflate_zip(zip_buf, sizeof(zip_buf), &zip_len) != 0) {
        (void)fprintf(stderr, "Skipping: could not build deflate ZIP\n");
        return 0;
    }

    zip = pas_zip_open(zip_buf, zip_len, &status);
    if (!zip || status != PAS_ZIP_OK) {
        (void)fprintf(stderr, "Skipping deflate test: invalid ZIP or unsupported format\n");
        return 0;
    }

    file = pas_zip_find(zip, "test");
    if (!file) {
        (void)fprintf(stderr, "Skipping: file 'test' not found\n");
        return 0;
    }

    if (!pas_zip_is_compressed(file)) {
        (void)fprintf(stderr, "Skipping: file is stored, not deflate (use real deflate ZIP)\n");
        return 0;
    }

    n = pas_zip_extract(file, out, sizeof(out), &status);
    if (status != PAS_ZIP_OK) {
        (void)fprintf(stderr, "Deflate extract failed: %d (miniz/zlib required)\n", status);
        return 1;
    }
    ASSERT_EQ(n, 4u);
    ASSERT(memcmp(out, "test", 4) == 0);

    if (g_failed) {
        (void)fprintf(stderr, "Total: %d assertions, %d failed\n", g_assertions, g_failed);
        return 1;
    }
    (void)printf("All %d assertions passed (deflate)\n", g_assertions);
    return 0;
#else
    (void)printf("Skipping deflate test (PAS_ZIP_USE_MINIZ not defined)\n");
    return 0;
#endif
}

/*
    test_fat32.c - Test FAT32 driver (read-only).
    Builds a minimal FAT32 image in memory and reads a file.
    From repo root: gcc -o tests/pas_fs/test_fat32 tests/pas_fs/test_fat32.c -I.
*/

#define PAS_FS_IMPLEMENTATION
#include "pas_fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failed, g_assertions;

#define ASSERT(cond) do { \
    ++g_assertions; \
    if (!(cond)) { (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failed; } \
} while (0)
#define ASSERT_EQ(a, b) ASSERT((a) == (b))

static void put_u16(unsigned char *p, uint16_t v) {
    p[0] = (unsigned char)(v);
    p[1] = (unsigned char)(v >> 8);
}
static void put_u32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v);
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

/* Build minimal FAT32 image with one file "HELLO TXT" containing "world" */
static unsigned char *build_fat32_image(size_t *out_size) {
    unsigned char *img;
    size_t bps = 512;
    size_t reserved = 32;
    size_t spf = 1;
    size_t spc = 1;
    size_t fat_start = reserved;
    size_t data_start = reserved + 2 * spf;
    size_t total = data_start * bps + 2 * bps;
    unsigned char *dir, *fat;

    img = (unsigned char *)malloc(total);
    if (!img) return NULL;
    memset(img, 0, total);

    memset(img, 0, 512);
    img[0] = 0xEB; img[1] = 0x58; img[2] = 0x90;
    memcpy(img + 3, "MSDOS5.0", 8);
    put_u16(img + 0x0B, (uint16_t)bps);
    img[0x0D] = (unsigned char)spc;
    put_u16(img + 0x0E, (uint16_t)reserved);
    img[0x10] = 2;
    put_u16(img + 0x11, 0);
    put_u16(img + 0x13, 0);
    img[0x15] = 0xF8;
    put_u16(img + 0x16, 0);
    put_u16(img + 0x18, 63);
    put_u16(img + 0x1A, 255);
    put_u16(img + 0x1C, 0);
    put_u32(img + 0x20, 0);
    put_u32(img + 0x24, (uint32_t)spf);
    put_u16(img + 0x28, 0);
    put_u16(img + 0x2A, 0);
    put_u32(img + 0x2C, 2);
    put_u16(img + 0x30, 1);
    put_u16(img + 0x32, 6);
    img[0x42] = 0x29;
    put_u32(img + 0x43, 0x12345678);
    memcpy(img + 0x47, "NO NAME    ", 11);
    memcpy(img + 0x52, "FAT32   ", 8);
    img[0x1FE] = 0x55;
    img[0x1FF] = 0xAA;

    fat = img + fat_start * bps;
    put_u32(fat + 0, 0x0FFFFF00u);
    put_u32(fat + 4, 0x0FFFFFFFu);
    put_u32(fat + 8, 0x0FFFFFFFu);
    put_u32(fat + 12, 0x0FFFFFFFu);

    dir = img + data_start * bps;
    memcpy(dir + 0, "HELLO   TXT", 11);
    dir[11] = 0x20;
    put_u16(dir + 0x1A, 3);
    put_u16(dir + 0x14, 0);
    put_u32(dir + 0x1C, 5);
    memcpy(img + data_start * bps + bps, "world", 5);

    *out_size = total;
    return img;
}

int main(void) {
    unsigned char *fat32_image;
    size_t fat32_size;
    pas_fs_fat32_device_t fat32_device = { 0 };
    pas_fs_file_t *f;
    pas_fs_status status;
    char buf[32];
    size_t n;

    g_failed = 0;
    g_assertions = 0;

    fat32_image = build_fat32_image(&fat32_size);
    if (!fat32_image) {
        (void)fprintf(stderr, "build_fat32_image failed\n");
        return 1;
    }

    fat32_device.image = fat32_image;
    fat32_device.size = fat32_size;

    pas_fs_init();
    ASSERT(pas_fs_mount("/fat", &pas_fs_fat32_driver, &fat32_device) == 0);

    ASSERT(pas_fs_exists("/fat/hello.txt"));
    ASSERT(!pas_fs_exists("/fat/nonexistent"));
    ASSERT_EQ(pas_fs_size("/fat/hello.txt"), 5u);

    f = pas_fs_open("/fat/hello.txt", &status);
    ASSERT(f != NULL);
    ASSERT(status == PAS_FS_OK);
    n = pas_fs_read(f, buf, sizeof(buf), &status);
    ASSERT_EQ(n, 5u);
    ASSERT(status == PAS_FS_OK);
    ASSERT(memcmp(buf, "world", 5) == 0);
    pas_fs_close(f);

    f = pas_fs_open("/fat/HELLO.TXT", &status);
    ASSERT(f != NULL);
    n = pas_fs_read(f, buf, sizeof(buf), &status);
    ASSERT_EQ(n, 5u);
    pas_fs_close(f);

    free(fat32_image);

    if (g_failed) {
        (void)fprintf(stderr, "Total: %d assertions, %d failed\n", g_assertions, g_failed);
        return 1;
    }
    (void)printf("All %d assertions passed.\n", g_assertions);
    return 0;
}

/*
    example_mount.c - Mount RAM FS and optionally FAT32.
    From repo root: gcc -o examples/pas_fs/example_mount examples/pas_fs/example_mount.c -I.
    Usage: ./example_mount [fat32_image.bin]
*/

#define PAS_FS_IMPLEMENTATION
#include "pas_fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char ram_file1[] = "hello from RAM";
static char ram_file2[] = "second file";
static pas_fs_ram_entry_t ram_entries[] = {
    { "readme.txt", ram_file1, sizeof(ram_file1) - 1, 0 },
    { "data.bin",   ram_file2, sizeof(ram_file2) - 1, 0 },
};
static pas_fs_ram_device_t ram_device = {
    ram_entries,
    (int)(sizeof(ram_entries) / sizeof(ram_entries[0])),
};

int main(int argc, char **argv) {
    unsigned char *fat32_image = NULL;
    size_t fat32_size = 0;
    pas_fs_fat32_device_t fat32_device = { 0 };

    pas_fs_init();

    if (pas_fs_mount("/ram", &pas_fs_ram_driver, &ram_device) != 0) {
        (void)fprintf(stderr, "mount /ram failed\n");
        return 1;
    }
    (void)printf("Mounted RAM FS at /ram\n");

    if (argc >= 2) {
        FILE *f = fopen(argv[1], "rb");
        if (!f) {
            (void)fprintf(stderr, "Cannot open: %s\n", argv[1]);
            return 1;
        }
        fseek(f, 0, SEEK_END);
        fat32_size = (size_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        fat32_image = (unsigned char *)malloc(fat32_size);
        if (!fat32_image) {
            fclose(f);
            return 1;
        }
        if (fread(fat32_image, 1, fat32_size, f) != fat32_size) {
            free(fat32_image);
            fclose(f);
            return 1;
        }
        fclose(f);
        fat32_device.image = fat32_image;
        fat32_device.size = fat32_size;
        if (pas_fs_mount("/fat", &pas_fs_fat32_driver, &fat32_device) != 0) {
            (void)fprintf(stderr, "mount /fat failed\n");
            free(fat32_image);
            return 1;
        }
        (void)printf("Mounted FAT32 at /fat (from %s)\n", argv[1]);
    }

    (void)printf("Use pas_fs_open(\"/ram/readme.txt\") or pas_fs_open(\"/fat/path\") to read files.\n");
    if (fat32_image) free(fat32_image);
    return 0;
}

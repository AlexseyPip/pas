/*
    example_read.c - Read files from virtual FS.
    From repo root: gcc -o examples/pas_fs/example_read examples/pas_fs/example_read.c -I.
    Usage: ./example_read [path]
    Default path: /ram/readme.txt
*/

#define PAS_FS_IMPLEMENTATION
#include "pas_fs.h"
#include <stdio.h>
#include <string.h>

static char ram_file1[] = "Hello, pas_fs!";
static char ram_file2[] = "binary\0data";
static pas_fs_ram_entry_t ram_entries[] = {
    { "readme.txt", ram_file1, sizeof(ram_file1) - 1, 0 },
    { "data.bin",   ram_file2, sizeof(ram_file2),     0 },
};
static pas_fs_ram_device_t ram_device = {
    ram_entries,
    (int)(sizeof(ram_entries) / sizeof(ram_entries[0])),
};

int main(int argc, char **argv) {
    const char *path = (argc >= 2) ? argv[1] : "/ram/readme.txt";
    pas_fs_file_t *f;
    pas_fs_status status;
    char buf[256];
    size_t n, total = 0;

    pas_fs_init();
    if (pas_fs_mount("/ram", &pas_fs_ram_driver, &ram_device) != 0) {
        (void)fprintf(stderr, "mount failed\n");
        return 1;
    }

    if (!pas_fs_exists(path)) {
        (void)fprintf(stderr, "File not found: %s\n", path);
        return 1;
    }

    (void)printf("Size: %zu bytes\n", pas_fs_size(path));

    f = pas_fs_open(path, &status);
    if (!f || status != PAS_FS_OK) {
        (void)fprintf(stderr, "open failed: %d\n", status);
        return 1;
    }

    while ((n = pas_fs_read(f, buf, sizeof(buf), &status)) > 0 && status == PAS_FS_OK) {
        total += n;
        (void)fwrite(buf, 1, n, stdout);
    }
    pas_fs_close(f);
    (void)printf("\nRead %zu bytes total.\n", total);
    return 0;
}

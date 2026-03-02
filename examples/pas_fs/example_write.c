/*
    example_write.c - Write to RAM FS (writable entry).
    From repo root: gcc -o examples/pas_fs/example_write examples/pas_fs/example_write.c -I.
    Usage: ./example_write
*/

#define PAS_FS_IMPLEMENTATION
#include "pas_fs.h"
#include <stdio.h>
#include <string.h>

static char ram_buffer[64];
static pas_fs_ram_entry_t ram_entries[] = {
    { "writable.txt", ram_buffer, sizeof(ram_buffer), 1 },
};
static pas_fs_ram_device_t ram_device = {
    ram_entries,
    1,
};

int main(void) {
    const char *data = "Written by pas_fs_ram_write!";
    pas_fs_file_t *f;
    pas_fs_status status;
    size_t sz;

    pas_fs_init();
    if (pas_fs_mount("/ram", &pas_fs_ram_driver, &ram_device) != 0) {
        (void)fprintf(stderr, "mount failed\n");
        return 1;
    }

    f = pas_fs_open("/ram/writable.txt", &status);
    if (!f || status != PAS_FS_OK) {
        (void)fprintf(stderr, "open failed\n");
        return 1;
    }

    sz = (size_t)pas_fs_ram_write(f, data, strlen(data) + 1, &status);
    pas_fs_close(f);
    if (status != PAS_FS_OK || sz == (size_t)-1) {
        (void)fprintf(stderr, "write failed: %d\n", status);
        return 1;
    }

    (void)printf("Wrote %zu bytes. Reading back:\n", sz);
    (void)printf("%s\n", ram_buffer);
    return 0;
}

/*
    test_errors.c - Test pas_fs error codes.
    From repo root: gcc -o tests/pas_fs/test_errors tests/pas_fs/test_errors.c -I.
*/

#define PAS_FS_IMPLEMENTATION
#include "pas_fs.h"
#include <stdio.h>
#include <string.h>

static int g_failed, g_assertions;

#define ASSERT(cond) do { \
    ++g_assertions; \
    if (!(cond)) { (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failed; } \
} while (0)

static char ram_buf[] = "x";
static pas_fs_ram_entry_t ram_entries[] = {
    { "a.txt", ram_buf, 1, 0 },
};
static pas_fs_ram_device_t ram_device = {
    ram_entries,
    1,
};

int main(void) {
    pas_fs_file_t *f;
    pas_fs_status status;

    g_failed = 0;
    g_assertions = 0;

    pas_fs_init();
    ASSERT(pas_fs_mount("/ram", &pas_fs_ram_driver, &ram_device) == 0);

    ASSERT(!pas_fs_exists("/ram/nonexistent"));
    ASSERT(pas_fs_size("/ram/nonexistent") == (size_t)-1);

    f = pas_fs_open("/ram/nonexistent", &status);
    ASSERT(f == NULL);
    ASSERT(status == PAS_FS_E_NOT_FOUND);

    f = pas_fs_open("/unmounted/path", &status);
    ASSERT(f == NULL);
    ASSERT(status == PAS_FS_E_NOT_FOUND);

    f = pas_fs_open(NULL, &status);
    ASSERT(f == NULL);

    if (g_failed) {
        (void)fprintf(stderr, "Total: %d assertions, %d failed\n", g_assertions, g_failed);
        return 1;
    }
    (void)printf("All %d assertions passed.\n", g_assertions);
    return 0;
}

/*
    test_mount.c - Test pas_fs_init and pas_fs_mount.
    From repo root: gcc -o tests/pas_fs/test_mount tests/pas_fs/test_mount.c -I.
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
#define ASSERT_EQ(a, b) ASSERT((a) == (b))

static char ram_buf[] = "x";
static pas_fs_ram_entry_t ram_entries[] = {
    { "a.txt", ram_buf, 1, 0 },
};
static pas_fs_ram_device_t ram_device = {
    ram_entries,
    1,
};

int main(void) {
    g_failed = 0;
    g_assertions = 0;

    pas_fs_init();
    ASSERT(pas_fs_mount("/ram", &pas_fs_ram_driver, &ram_device) == 0);
    ASSERT(pas_fs_exists("/ram/a.txt"));
    ASSERT(pas_fs_size("/ram/a.txt") == 1u);

    pas_fs_init();
    ASSERT(pas_fs_mount("/", &pas_fs_ram_driver, &ram_device) == 0);
    ASSERT(pas_fs_exists("/a.txt"));

    pas_fs_init();
    ASSERT(pas_fs_mount("/foo", &pas_fs_ram_driver, &ram_device) == 0);
    ASSERT(pas_fs_exists("/foo/a.txt"));
    ASSERT(!pas_fs_exists("/a.txt"));

    pas_fs_init();
    ASSERT(pas_fs_mount(NULL, &pas_fs_ram_driver, &ram_device) != 0);
    ASSERT(pas_fs_mount("/x", NULL, &ram_device) != 0);

    if (g_failed) {
        (void)fprintf(stderr, "Total: %d assertions, %d failed\n", g_assertions, g_failed);
        return 1;
    }
    (void)printf("All %d assertions passed.\n", g_assertions);
    return 0;
}

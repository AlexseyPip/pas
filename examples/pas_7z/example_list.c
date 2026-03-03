/*
    example_list.c - List files in a 7z archive.
    From repo root: gcc -o examples/pas_7z/example_list examples/pas_7z/example_list.c -I.
    Usage: ./example_list <file.7z>
*/

#define PAS_7Z_IMPLEMENTATION
#include "pas_7z.h"
#include <stdio.h>
#include <stdlib.h>

static void list_callback(const char *name, uint64_t size, int is_dir, void *user) {
    (void)user;
    (void)printf("  %s  %llu bytes  %s\n", name, (unsigned long long)size, is_dir ? "[dir]" : "");
}

int main(int argc, char **argv) {
    FILE *f;
    size_t size;
    unsigned char *data;
    pas_7z_t *arch;
    pas_7z_status status;

    if (argc < 2) {
        (void)fprintf(stderr, "Usage: %s <file.7z>\n", argv[0]);
        return 1;
    }

    f = fopen(argv[1], "rb");
    if (!f) {
        (void)fprintf(stderr, "Cannot open: %s\n", argv[1]);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    data = (unsigned char *)malloc(size);
    if (!data) {
        fclose(f);
        return 1;
    }
    if (fread(data, 1, size, f) != size) {
        free(data);
        fclose(f);
        return 1;
    }
    fclose(f);

    arch = pas_7z_open(data, size, &status);
    if (!arch || status != PAS_7Z_OK) {
        (void)fprintf(stderr, "pas_7z_open failed: %d (packed header not supported)\n", status);
        free(data);
        return 1;
    }

    (void)printf("Contents of %s:\n", argv[1]);
    if (pas_7z_list(arch, list_callback, NULL) != 0) {
        (void)fprintf(stderr, "pas_7z_list failed\n");
        free(data);
        return 1;
    }

    free(data);
    return 0;
}

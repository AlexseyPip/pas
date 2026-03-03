/*
    example_list.c - List files in a RAR archive (RAR4/RAR5).
    From repo root: gcc -o examples/pas_rar/example_list examples/pas_rar/example_list.c -I.
    Usage: ./example_list <file.rar>
*/

#define PAS_RAR_IMPLEMENTATION
#include "pas_rar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void list_callback(const char *name, uint64_t size, void *user) {
    (void)user;
    (void)printf("  %s  (%llu bytes)\n", name, (unsigned long long)size);
}

int main(int argc, char **argv) {
    FILE *f;
    size_t size;
    unsigned char *data;
    pas_rar_t *rar;
    pas_rar_status status;

    if (argc < 2) {
        (void)fprintf(stderr, "Usage: %s <file.rar>\n", argv[0]);
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

    rar = pas_rar_open(data, size, &status);
    if (!rar || status != PAS_RAR_OK) {
        (void)fprintf(stderr, "pas_rar_open failed: %d\n", status);
        free(data);
        return 1;
    }

    (void)printf("Contents of %s (format %d):\n", argv[1], rar->format);
    if (pas_rar_list(rar, list_callback, NULL) != 0) {
        (void)fprintf(stderr, "pas_rar_list failed\n");
        free(data);
        return 1;
    }

    free(data);
    return 0;
}

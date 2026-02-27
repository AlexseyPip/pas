/*
    example_list.c - List files in a ZIP archive.
    From repo root: gcc -o examples/pas_zip/example_list examples/pas_zip/example_list.c -I.
    Usage: ./example_list <file.zip>
*/

#define PAS_ZIP_IMPLEMENTATION
#include "pas_zip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void list_callback(const char *name, size_t size, void *user) {
    (void)user;
    (void)printf("  %s  (%zu bytes)\n", name, size);
}

int main(int argc, char **argv) {
    FILE *f;
    size_t size;
    unsigned char *data;
    pas_zip_t *zip;
    pas_zip_status status;

    if (argc < 2) {
        (void)fprintf(stderr, "Usage: %s <file.zip>\n", argv[0]);
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

    zip = pas_zip_open(data, size, &status);
    if (!zip || status != PAS_ZIP_OK) {
        (void)fprintf(stderr, "pas_zip_open failed: %d\n", status);
        free(data);
        return 1;
    }

    (void)printf("Contents of %s:\n", argv[1]);
    if (pas_zip_list(zip, list_callback, NULL) != 0) {
        (void)fprintf(stderr, "pas_zip_list failed\n");
        free(data);
        return 1;
    }

    free(data);
    return 0;
}

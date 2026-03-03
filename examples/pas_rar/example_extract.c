/*
    example_extract.c - Extract a file from a RAR archive (RAR4/RAR5, store/uncompressed only).
    From repo root: gcc -o examples/pas_rar/example_extract examples/pas_rar/example_extract.c -I.
    Usage: ./example_extract <file.rar> <entry_name> [output_file]
*/

#define PAS_RAR_IMPLEMENTATION
#include "pas_rar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    FILE *f;
    size_t size;
    unsigned char *data;
    pas_rar_t *rar;
    pas_rar_file_t *file;
    pas_rar_status status;
    size_t extracted;
    unsigned char *buf;
    FILE *out;
    uint64_t file_size;

    if (argc < 3) {
        (void)fprintf(stderr, "Usage: %s <file.rar> <entry_name> [output_file]\n", argv[0]);
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

    file = pas_rar_find(rar, argv[2]);
    if (!file) {
        (void)fprintf(stderr, "File not found: %s\n", argv[2]);
        free(data);
        return 1;
    }

    if (pas_rar_is_compressed(file)) {
        (void)fprintf(stderr, "Entry is compressed; pas_rar extracts only store/uncompressed files.\n");
        free(data);
        return 1;
    }

    file_size = pas_rar_size(file);
    if (file_size > (uint64_t)SIZE_MAX - 1) {
        (void)fprintf(stderr, "File too large\n");
        free(data);
        return 1;
    }

    buf = (unsigned char *)malloc((size_t)file_size + 1);
    if (!buf) {
        free(data);
        return 1;
    }

    extracted = pas_rar_extract(file, buf, (size_t)file_size + 1, &status);
    if (status != PAS_RAR_OK || extracted == 0) {
        (void)fprintf(stderr, "pas_rar_extract failed: %d\n", status);
        free(buf);
        free(data);
        return 1;
    }

    if (argc >= 4) {
        out = fopen(argv[3], "wb");
        if (!out) {
            (void)fprintf(stderr, "Cannot write: %s\n", argv[3]);
            free(buf);
            free(data);
            return 1;
        }
        if (fwrite(buf, 1, extracted, out) != extracted) {
            fclose(out);
            free(buf);
            free(data);
            return 1;
        }
        fclose(out);
        (void)printf("Extracted %zu bytes to %s\n", extracted, argv[3]);
    } else {
        buf[extracted] = '\0';
        (void)printf("Extracted %zu bytes (pass 3rd arg to save to file)\n", extracted);
    }

    free(buf);
    free(data);
    return 0;
}

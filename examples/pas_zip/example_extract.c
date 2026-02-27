/*
    example_extract.c - Extract a file from a ZIP archive.
    From repo root: gcc -o examples/pas_zip/example_extract examples/pas_zip/example_extract.c -I.
    Usage: ./example_extract <file.zip> <entry_name> [output_file]
*/

#define PAS_ZIP_IMPLEMENTATION
#include "pas_zip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    FILE *f;
    size_t size;
    unsigned char *data;
    pas_zip_t *zip;
    pas_zip_file_t *file;
    pas_zip_status status;
    size_t extracted;
    unsigned char *buf;
    FILE *out;

    if (argc < 3) {
        (void)fprintf(stderr, "Usage: %s <file.zip> <entry_name> [output_file]\n", argv[0]);
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

    file = pas_zip_find(zip, argv[2]);
    if (!file) {
        (void)fprintf(stderr, "File not found: %s\n", argv[2]);
        free(data);
        return 1;
    }

    buf = (unsigned char *)malloc(pas_zip_size(file) + 1);
    if (!buf) {
        free(data);
        return 1;
    }

    extracted = pas_zip_extract(file, buf, pas_zip_size(file) + 1, &status);
    if (status != PAS_ZIP_OK || extracted == 0) {
        (void)fprintf(stderr, "pas_zip_extract failed: %d\n", status);
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

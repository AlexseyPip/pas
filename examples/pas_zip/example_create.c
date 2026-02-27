/*
    example_create.c - Create a ZIP archive (Store only).
    From repo root: gcc -o examples/pas_zip/example_create examples/pas_zip/example_create.c -I.
    Writes example.zip with a few text entries.
*/

#define PAS_ZIP_IMPLEMENTATION
#include "pas_zip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    static unsigned char buf[65536];
    const char *names[] = { "hello.txt", "world.txt", "readme.txt" };
    const char *contents[] = {
        "Hello, ZIP!\n",
        "World.\n",
        "Created by pas_zip_create (Store only).\n"
    };
    const void *datas[3];
    size_t sizes[3];
    size_t written;
    pas_zip_status status;
    FILE *f;

    datas[0] = contents[0]; sizes[0] = strlen(contents[0]);
    datas[1] = contents[1]; sizes[1] = strlen(contents[1]);
    datas[2] = contents[2]; sizes[2] = strlen(contents[2]);

    written = pas_zip_create(names, datas, sizes, 3, buf, sizeof(buf), &status);
    if (status != PAS_ZIP_OK || written == 0) {
        (void)fprintf(stderr, "pas_zip_create failed: %d\n", status);
        return 1;
    }

    f = fopen("example.zip", "wb");
    if (!f) {
        (void)fprintf(stderr, "Cannot write example.zip\n");
        return 1;
    }
    if (fwrite(buf, 1, written, f) != written) {
        fclose(f);
        return 1;
    }
    fclose(f);

    (void)printf("Created example.zip (%zu bytes)\n", written);
    return 0;
}

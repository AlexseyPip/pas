/*
    example_metrics.c - Print basic metrics from a TrueType font.
    From repo root:
        gcc -o examples/pas_truetype/example_metrics examples/pas_truetype/example_metrics.c -I.
    Usage:
        ./example_metrics <font.ttf>
*/

#define PAS_TRUETYPE_IMPLEMENTATION
#include "pas_truetype.h"

#include <stdio.h>
#include <stdlib.h>

static unsigned char *read_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    unsigned char *buf;
    long sz;

    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    buf = (unsigned char *)malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);

    if (out_size) *out_size = (size_t)sz;
    return buf;
}

int main(int argc, char **argv) {
    const char *path;
    unsigned char *data;
    size_t size = 0;
    pas_tt_font_t font;
    pas_tt_status status;
    int ascent, descent, line_gap;
    float scale;
    int glyph_A, x0, y0, x1, y1;

    if (argc < 2) {
        (void)fprintf(stderr, "Usage: %s <font.ttf>\n", argv[0]);
        return 1;
    }
    path = argv[1];

    data = read_file(path, &size);
    if (!data) {
        (void)fprintf(stderr, "Cannot read font: %s\n", path);
        return 1;
    }

    if (!pas_tt_init_font(&font, data, size, &status) || status != PAS_TT_OK) {
        (void)fprintf(stderr, "pas_tt_init_font failed: %d\n", status);
        free(data);
        return 1;
    }

    pas_tt_get_vmetrics(&font, &ascent, &descent, &line_gap);
    (void)printf("Font metrics (units/em = %u):\n", (unsigned)font.units_per_em);
    (void)printf("  ascent  = %d\n", ascent);
    (void)printf("  descent = %d\n", descent);
    (void)printf("  lineGap = %d\n", line_gap);

    scale = pas_tt_scale_for_pixel_height(&font, 32.0f);
    (void)printf("Scale for 32px: %.6f\n", (double)scale);

    glyph_A = pas_tt_get_glyph_index(&font, 'A');
    (void)printf("Glyph index for 'A' (U+0041): %d\n", glyph_A);

    if (glyph_A > 0) {
        pas_tt_get_glyph_box(&font, glyph_A, &x0, &y0, &x1, &y1);
        (void)printf("  glyph box (font units): x0=%d y0=%d x1=%d y1=%d\n", x0, y0, x1, y1);
        if (pas_tt_get_glyph_bitmap_box(&font, glyph_A, scale, scale, &x0, &y0, &x1, &y1)) {
            (void)printf("  glyph box (32px): x0=%d y0=%d x1=%d y1=%d\n", x0, y0, x1, y1);
        }
    }

    free(data);
    return 0;
}


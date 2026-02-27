/*
    example_text.c - TTF text via stb_truetype; multiline and kerning.
    Requires: data/font.ttf and -DPAS_GFX_USE_STB_TRUETYPE, stb_truetype.h in include path.
    From repo root: gcc -o examples/pas_gfx/example_text examples/pas_gfx/example_text.c -I. -DPAS_GFX_USE_STB_TRUETYPE
*/

#define STB_TRUETYPE_IMPLEMENTATION
#define PAS_GFX_USE_STB_TRUETYPE
#define PAS_GFX_IMPLEMENTATION
#include "pas_gfx.h"
#include <stdio.h>
#include <stdlib.h>

#define W 640
#define H 480
#define PITCH W

static int save_ppm_raw(const char *path, const uint32_t *pixels, int width, int height, int pitch)
{
    FILE *f;
    int y, x;
    unsigned char rgb[3];

    f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            uint32_t c = pixels[y * pitch + x];
            rgb[0] = (unsigned char)((c >> 16) & 0xFF);
            rgb[1] = (unsigned char)((c >> 8) & 0xFF);
            rgb[2] = (unsigned char)(c & 0xFF);
            if (fwrite(rgb, 1, 3, f) != 3) {
                fclose(f);
                return -1;
            }
        }
    }
    fclose(f);
    return 0;
}

static long read_file(const char *path, unsigned char **out_data)
{
    FILE *f;
    long size;

    f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    *out_data = (unsigned char *)malloc((size_t)size);
    if (!*out_data) {
        fclose(f);
        return -1;
    }
    if (fread(*out_data, 1, (size_t)size, f) != (size_t)size) {
        free(*out_data);
        *out_data = NULL;
        fclose(f);
        return -1;
    }
    fclose(f);
    return size;
}

int main(void)
{
#ifdef PAS_GFX_USE_STB_TRUETYPE
    static uint32_t pixels[W * H];
    pas_gfx_fb_t *fb;
    pas_font_t *font;
    unsigned char *ttf_data;
    long ttf_size;

    fb = pas_gfx_init(pixels, W, H, PITCH);
    if (!fb) {
        fprintf(stderr, "pas_gfx_init failed\n");
        return 1;
    }

    ttf_size = read_file("data/font.ttf", &ttf_data);
    if (ttf_size <= 0 || !ttf_data) {
        fprintf(stderr, "Could not read data/font.ttf (create data/ and add a .ttf file)\n");
        return 1;
    }

    font = pas_gfx_font_open(ttf_data, 24.0f);
    if (!font) {
        fprintf(stderr, "pas_gfx_font_open failed\n");
        free(ttf_data);
        return 1;
    }

    pas_gfx_rect(fb, 0, 0, W, H, PAS_GFX_WHITE);

    /* Multiline text */
    pas_gfx_text(fb, font, 20, 40, "Line one\nLine two\nLine three", PAS_GFX_BLACK);

    /* Kerning demo: "AVA" and "To" show kerning */
    pas_gfx_text(fb, font, 20, 150, "AVA Tea", PAS_GFX_BLUE);
    pas_gfx_text(fb, font, 20, 200, "Typography", PAS_GFX_RED);

    if (save_ppm_raw("example_text.ppm", pixels, W, H, PITCH) != 0) {
        fprintf(stderr, "Failed to write example_text.ppm\n");
        free(ttf_data);
        return 1;
    }
    printf("Saved example_text.ppm (%dx%d)\n", W, H);
    free(ttf_data);
    return 0;
#else
    (void)printf("PAS_GFX_USE_STB_TRUETYPE not defined. Build with -DPAS_GFX_USE_STB_TRUETYPE and stb_truetype.h.\n");
    return 0;
#endif
}

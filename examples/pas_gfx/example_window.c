/*
    example_window.c - Window frame, button, built-in font; save to PPM.
    From repo root: gcc -o examples/pas_gfx/example_window examples/pas_gfx/example_window.c -I.
*/

#define PAS_GFX_IMPLEMENTATION
#include "pas_gfx.h"
#include <stdio.h>

#define W 400
#define H 300
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

int main(void)
{
    static uint32_t pixels[W * H];
    pas_gfx_fb_t *fb;

    fb = pas_gfx_init(pixels, W, H, PITCH);
    if (!fb) {
        fprintf(stderr, "pas_gfx_init failed\n");
        return 1;
    }

    pas_gfx_rect(fb, 0, 0, W, H, PAS_GFX_GRAY);

    /* Window with title */
    pas_gfx_window_frame(fb, 40, 30, 320, 200, "Hello Window", PAS_GFX_RGBA(0xFF, 0xE0, 0xE0, 0xE0));

    /* Buttons: unpressed and pressed */
    pas_gfx_button(fb, 80, 260, 100, 28, "OK", 0);
    pas_gfx_button(fb, 220, 260, 100, 28, "Cancel", 1);

    if (save_ppm_raw("example_window.ppm", pixels, W, H, PITCH) != 0) {
        fprintf(stderr, "Failed to write example_window.ppm\n");
        return 1;
    }
    printf("Saved example_window.ppm (%dx%d)\n", W, H);
    return 0;
}

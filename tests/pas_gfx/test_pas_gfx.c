/*
    test_pas_gfx.c - Tests for pas_gfx.h (pixel, line, rect, circle, bitmap, window_frame, button).
    From repo root: gcc -o tests/pas_gfx/test_pas_gfx tests/pas_gfx/test_pas_gfx.c -I.
*/

#define PAS_GFX_IMPLEMENTATION
#include "pas_gfx.h"
#include <stdio.h>
#include <string.h>

static int g_failed, g_assertions;

#define ASSERT(cond) do { \
    ++g_assertions; \
    if (!(cond)) { (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failed; } \
} while (0)
#define ASSERT_EQ(a, b) ASSERT((a) == (b))

static uint32_t get_pixel(const pas_gfx_fb_t *fb, int x, int y)
{
    if (!fb || !fb->pixels) return 0;
    if ((unsigned)x >= (unsigned)fb->width || (unsigned)y >= (unsigned)fb->height) return 0;
    return fb->pixels[y * fb->pitch + x];
}

static void test_pixel(void)
{
    uint32_t buf[16 * 16];
    pas_gfx_fb_t *fb;
    uint32_t c;

    fb = pas_gfx_init(buf, 16, 16, 16);
    ASSERT(fb != NULL);
    memset(buf, 0, sizeof(buf));

    pas_gfx_pixel(fb, 5, 5, PAS_GFX_RED);
    c = get_pixel(fb, 5, 5);
    ASSERT_EQ(c, PAS_GFX_RED);

    pas_gfx_pixel(fb, 0, 0, PAS_GFX_WHITE);
    ASSERT_EQ(get_pixel(fb, 0, 0), PAS_GFX_WHITE);

    /* Out of bounds should not crash; pixel should not be written */
    pas_gfx_pixel(fb, -1, 0, PAS_GFX_BLUE);
    pas_gfx_pixel(fb, 0, -1, PAS_GFX_BLUE);
    pas_gfx_pixel(fb, 16, 0, PAS_GFX_BLUE);
    pas_gfx_pixel(fb, 0, 16, PAS_GFX_BLUE);
    ASSERT_EQ(get_pixel(fb, 0, 0), PAS_GFX_WHITE);
}

static void test_line(void)
{
    uint32_t buf[24 * 24];
    pas_gfx_fb_t *fb;
    int i, x1, y1, x2, y2;

    fb = pas_gfx_init(buf, 24, 24, 24);
    ASSERT(fb != NULL);
    memset(buf, 0, sizeof(buf));

    /* Octants: (dx,dy) signs and |dx| vs |dy| */
    struct { int x1, y1, x2, y2; } segs[] = {
        { 2, 2, 10, 2 },   /* 0: right */
        { 2, 2, 10, 10 },  /* 1 */
        { 2, 2, 2, 10 },   /* 2: down */
        { 2, 2, 2, 10 },   /* 3 */
        { 10, 2, 2, 2 },   /* 4: left */
        { 10, 10, 2, 2 },  /* 5 */
        { 2, 10, 2, 2 },   /* 6: up */
        { 12, 12, 12, 20 },
    };
    for (i = 0; i < (int)(sizeof(segs)/sizeof(segs[0])); i++) {
        pas_gfx_line(fb, segs[i].x1, segs[i].y1, segs[i].x2, segs[i].y2, PAS_GFX_WHITE);
    }

    /* Endpoints should be set */
    ASSERT_EQ(get_pixel(fb, 2, 2), PAS_GFX_WHITE);
    ASSERT_EQ(get_pixel(fb, 10, 2), PAS_GFX_WHITE);
    ASSERT_EQ(get_pixel(fb, 10, 10), PAS_GFX_WHITE);
    ASSERT_EQ(get_pixel(fb, 2, 10), PAS_GFX_WHITE);
    ASSERT_EQ(get_pixel(fb, 12, 12), PAS_GFX_WHITE);
    ASSERT_EQ(get_pixel(fb, 12, 20), PAS_GFX_WHITE);
}

static void test_rect(void)
{
    uint32_t buf[32 * 32];
    pas_gfx_fb_t *fb;
    uint32_t fill;

    fb = pas_gfx_init(buf, 32, 32, 32);
    ASSERT(fb != NULL);
    memset(buf, 0, sizeof(buf));
    fill = PAS_GFX_GREEN;
    pas_gfx_rect(fb, 4, 4, 8, 8, fill);

    ASSERT_EQ(get_pixel(fb, 4, 4), fill);
    ASSERT_EQ(get_pixel(fb, 11, 4), fill);
    ASSERT_EQ(get_pixel(fb, 4, 11), fill);
    ASSERT_EQ(get_pixel(fb, 11, 11), fill);
    ASSERT_EQ(get_pixel(fb, 7, 7), fill);
    ASSERT_EQ(get_pixel(fb, 3, 4), 0u);
    ASSERT_EQ(get_pixel(fb, 12, 4), 0u);
    ASSERT_EQ(get_pixel(fb, 4, 3), 0u);
}

static void test_circle(void)
{
    uint32_t buf[32 * 32];
    pas_gfx_fb_t *fb;
    int cx = 15, cy = 15, r = 8;

    fb = pas_gfx_init(buf, 32, 32, 32);
    ASSERT(fb != NULL);
    memset(buf, 0, sizeof(buf));
    pas_gfx_circle(fb, cx, cy, r, PAS_GFX_BLUE);

    /* Symmetric points on circle: (cx+r, cy), (cx-r, cy), (cx, cy+r), (cx, cy-r) */
    ASSERT_EQ(get_pixel(fb, cx + r, cy), PAS_GFX_BLUE);
    ASSERT_EQ(get_pixel(fb, cx - r, cy), PAS_GFX_BLUE);
    ASSERT_EQ(get_pixel(fb, cx, cy + r), PAS_GFX_BLUE);
    ASSERT_EQ(get_pixel(fb, cx, cy - r), PAS_GFX_BLUE);
    /* Center should not be drawn (outline only) */
    ASSERT_EQ(get_pixel(fb, cx, cy), 0u);
}

static void test_bitmap(void)
{
    uint32_t buf[16 * 16];
    pas_gfx_fb_t *fb;
    uint8_t mask[4] = { 0, 255, 255, 0 };
    uint32_t c;

    fb = pas_gfx_init(buf, 16, 16, 16);
    ASSERT(fb != NULL);
    memset(buf, 0, sizeof(buf));
    pas_gfx_rect(fb, 0, 0, 16, 16, PAS_GFX_WHITE);
    pas_gfx_bitmap(fb, 4, 4, mask, 2, 2, PAS_GFX_RED);

    c = get_pixel(fb, 4, 4);
    ASSERT(c != PAS_GFX_WHITE && c != 0); /* blended */
    c = get_pixel(fb, 5, 4);
    ASSERT((c & 0xFF) != 0 || ((c >> 8) & 0xFF) != 0 || ((c >> 16) & 0xFF) != 0);
    c = get_pixel(fb, 4, 5);
    ASSERT((c & 0xFF) != 0 || ((c >> 8) & 0xFF) != 0 || ((c >> 16) & 0xFF) != 0);
}

static void test_window_frame(void)
{
    uint32_t buf[64 * 64];
    pas_gfx_fb_t *fb;

    fb = pas_gfx_init(buf, 64, 64, 64);
    ASSERT(fb != NULL);
    memset(buf, 0, sizeof(buf));
    pas_gfx_window_frame(fb, 8, 8, 48, 48, "Hi", PAS_GFX_GRAY);

    /* Border (top-left corner) should be white */
    ASSERT_EQ(get_pixel(fb, 8, 8), PAS_GFX_WHITE);
    ASSERT_EQ(get_pixel(fb, 8, 9), PAS_GFX_WHITE);
    ASSERT_EQ(get_pixel(fb, 9, 8), PAS_GFX_WHITE);
    /* Inside content area should be gray background */
    ASSERT_EQ(get_pixel(fb, 10, 24), PAS_GFX_GRAY);
}

static void test_button(void)
{
    uint32_t buf[48 * 32];
    pas_gfx_fb_t *fb;
    uint32_t inner_unpressed, inner_pressed;

    fb = pas_gfx_init(buf, 48, 32, 48);
    ASSERT(fb != NULL);
    memset(buf, 0, sizeof(buf));

    pas_gfx_button(fb, 4, 4, 20, 14, "A", 0);
    inner_unpressed = get_pixel(fb, 10, 10);

    memset(buf, 0, sizeof(buf));
    pas_gfx_button(fb, 4, 4, 20, 14, "A", 1);
    inner_pressed = get_pixel(fb, 11, 11);

    ASSERT(inner_unpressed == PAS_GFX_WHITE || inner_unpressed == PAS_GFX_GRAY);
    ASSERT(inner_pressed == PAS_GFX_WHITE || inner_pressed == PAS_GFX_GRAY);
    ASSERT(inner_unpressed != inner_pressed || inner_pressed == PAS_GFX_GRAY);
}

int main(void)
{
    g_failed = 0;
    g_assertions = 0;

    test_pixel();
    test_line();
    test_rect();
    test_circle();
    test_bitmap();
    test_window_frame();
    test_button();

    if (g_failed) {
        (void)fprintf(stderr, "Total: %d assertions, %d failed\n", g_assertions, g_failed);
        return 1;
    }
    (void)printf("All %d assertions passed.\n", g_assertions);
    return 0;
}

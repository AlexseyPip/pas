/*
pas_truetype.h - tiny TrueType metrics helper (stb-style, no malloc)

Design:
- Single-header, stb-style.
- No dynamic allocation: parses a TrueType/OpenType font from a caller-provided
  memory buffer and keeps only offsets into it.
- Focused on **metrics and layout**, not rasterization:
  - Map Unicode code points to glyph indices (cmap formats 0 and 4).
  - Query font vertical metrics (ascent, descent, line_gap).
  - Query glyph horizontal metrics (advance width, left side bearing).
  - Query glyph bounding box in font units and in pixel space (given scale).

Usage:
    #define PAS_TRUETYPE_IMPLEMENTATION
    #include "pas_truetype.h"

    // elsewhere
    #include "pas_truetype.h"

API is intentionally small. It is suitable for OS / embedded use, and can be
used together with a separate rasterizer (e.g. pas_gfx, scanline renderer, etc.).

Public domain / UNLICENSE, following the rest of PAS.
The implementation is a small, self-contained reader loosely inspired by
the TrueType portions of stb_truetype.h (also public domain).
*/

#ifndef PAS_TRUETYPE_H
#define PAS_TRUETYPE_H

#ifdef __cplusplus
extern "C" {
#endif

/* configuration */
#if defined(PAS_TRUETYPE_STATIC)
#  define PAS_TT_DEF static
#else
#  define PAS_TT_DEF extern
#endif

#include <stddef.h> /* size_t */
#include <stdint.h> /* uint8_t, uint16_t, uint32_t, int16_t */

typedef enum pas_tt_status_e {
    PAS_TT_OK = 0,
    PAS_TT_E_INVALID,     /* malformed font or unsupported sfnt */
    PAS_TT_E_RANGE,       /* offset/length out of range */
    PAS_TT_E_UNSUPPORTED  /* cmap/loca formats we don't handle */
} pas_tt_status;

typedef struct pas_tt_font_s {
    const uint8_t *data;
    size_t size;

    /* basic font metrics */
    uint16_t units_per_em;
    int16_t ascent;
    int16_t descent;
    int16_t line_gap;

    /* glyph + metrics tables */
    uint16_t num_glyphs;
    uint16_t num_hmetrics;
    uint16_t index_to_loc_format; /* from head: 0=ushort offsets*2, 1=uint32 offsets */

    /* cached table offsets (from start of font data) */
    uint32_t cmap_table_offset;
    uint32_t cmap_table_length;
    uint32_t hmtx_offset;
    uint32_t loca_offset;
    uint32_t glyf_offset;
} pas_tt_font_t;

/* Open a TrueType/OpenType font from memory (single font, no TTC).
   - data/size: full font file in memory
   - out_font: caller-provided struct to fill
   Returns non-zero on success, 0 on failure (status optional).
*/
PAS_TT_DEF int
pas_tt_init_font(pas_tt_font_t *out_font, const void *data, size_t size, pas_tt_status *status);

/* Vertical metrics in font units (em space). */
PAS_TT_DEF void
pas_tt_get_vmetrics(const pas_tt_font_t *font, int *ascent, int *descent, int *line_gap);

/* Horizontal metrics for a glyph (advance width and left side bearing). */
PAS_TT_DEF void
pas_tt_get_glyph_hmetrics(const pas_tt_font_t *font, int glyph_index,
                          int *advance_width, int *left_side_bearing);

/* Map Unicode code point (UTF-32) to glyph index using cmap.
   Supports formats 0 and 4; other formats return 0 (missing glyph). */
PAS_TT_DEF int
pas_tt_get_glyph_index(const pas_tt_font_t *font, int codepoint);

/* Glyph bounding box in font units (unscaled), or zeros if glyph missing. */
PAS_TT_DEF void
pas_tt_get_glyph_box(const pas_tt_font_t *font, int glyph_index,
                     int *x0, int *y0, int *x1, int *y1);

/* Scale factor from font units to pixels for given pixel_height (em square). */
PAS_TT_DEF float
pas_tt_scale_for_pixel_height(const pas_tt_font_t *font, float pixel_height);

/* Bounding box in pixel space (integer box after applying scale).
   Returns 1 on success, 0 if glyph missing or invalid. */
PAS_TT_DEF int
pas_tt_get_glyph_bitmap_box(const pas_tt_font_t *font, int glyph_index,
                            float scale_x, float scale_y,
                            int *x0, int *y0, int *x1, int *y1);

#ifdef __cplusplus
} /* extern "C" */
#endif

/* ======================= implementation ======================= */

#ifdef PAS_TRUETYPE_IMPLEMENTATION

#include <string.h> /* memset, memcmp */

/* internal helpers */
static uint16_t pas_tt__u16_be(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static int16_t pas_tt__s16_be(const uint8_t *p) {
    return (int16_t)((p[0] << 8) | p[1]);
}

static uint32_t pas_tt__u32_be(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int pas_tt__in_range(size_t offset, size_t length, size_t size) {
    return offset <= size && length <= size - offset;
}

/* Find sfnt table by tag (e.g. 'cmap', 'head'). Returns non-zero on success. */
static int pas_tt__find_table(const uint8_t *data, size_t size,
                              const char tag[4],
                              uint32_t *out_offset, uint32_t *out_length)
{
    if (!pas_tt__in_range(0, 12, size)) return 0;

    uint16_t numTables = pas_tt__u16_be(data + 4);
    size_t tableDirOffset = 12;
    size_t i;

    if (!pas_tt__in_range(tableDirOffset, (size_t)numTables * 16, size)) return 0;

    for (i = 0; i < numTables; i++) {
        const uint8_t *rec = data + tableDirOffset + i * 16;
        if (rec[0] == (uint8_t)tag[0] && rec[1] == (uint8_t)tag[1] &&
            rec[2] == (uint8_t)tag[2] && rec[3] == (uint8_t)tag[3]) {
            uint32_t offset = pas_tt__u32_be(rec + 8);
            uint32_t length = pas_tt__u32_be(rec + 12);
            if (!pas_tt__in_range(offset, length, size)) return 0;
            if (out_offset) *out_offset = offset;
            if (out_length) *out_length = length;
            return 1;
        }
    }
    return 0;
}

PAS_TT_DEF int
pas_tt_init_font(pas_tt_font_t *out_font, const void *data, size_t size, pas_tt_status *status)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t sfnt_version;
    uint32_t head_off = 0, cmap_off = 0, cmap_len = 0;
    uint32_t hhea_off = 0, maxp_off = 0, hmtx_off = 0;
    uint32_t loca_off = 0, glyf_off = 0;

    if (status) *status = PAS_TT_E_INVALID;
    if (!out_font || !data || size < 12) return 0;

    sfnt_version = pas_tt__u32_be(p);
    if (!(sfnt_version == 0x00010000u || sfnt_version == 0x4F54544Fu)) { /* 0x00010000 or 'OTTO' */
        return 0; /* no TTC support yet */
    }

    if (!pas_tt__find_table(p, size, "head", &head_off, NULL)) return 0;
    if (!pas_tt__find_table(p, size, "cmap", &cmap_off, &cmap_len)) return 0;
    if (!pas_tt__find_table(p, size, "hhea", &hhea_off, NULL)) return 0;
    if (!pas_tt__find_table(p, size, "maxp", &maxp_off, NULL)) return 0;
    if (!pas_tt__find_table(p, size, "hmtx", &hmtx_off, NULL)) return 0;
    if (!pas_tt__find_table(p, size, "loca", &loca_off, NULL)) return 0;
    if (!pas_tt__find_table(p, size, "glyf", &glyf_off, NULL)) return 0;

    if (!pas_tt__in_range(head_off, 54, size)) return 0;
    if (!pas_tt__in_range(hhea_off, 36, size)) return 0;
    if (!pas_tt__in_range(maxp_off, 6, size)) return 0;

    memset(out_font, 0, sizeof(*out_font));
    out_font->data = p;
    out_font->size = size;

    out_font->units_per_em = pas_tt__u16_be(p + head_off + 18);
    out_font->index_to_loc_format = (uint16_t)pas_tt__s16_be(p + head_off + 50);

    out_font->ascent  = pas_tt__s16_be(p + hhea_off + 4);
    out_font->descent = pas_tt__s16_be(p + hhea_off + 6);
    out_font->line_gap = pas_tt__s16_be(p + hhea_off + 8);
    out_font->num_hmetrics = pas_tt__u16_be(p + hhea_off + 34);

    out_font->num_glyphs = pas_tt__u16_be(p + maxp_off + 4);

    out_font->cmap_table_offset = cmap_off;
    out_font->cmap_table_length = cmap_len;
    out_font->hmtx_offset = hmtx_off;
    out_font->loca_offset = loca_off;
    out_font->glyf_offset = glyf_off;

    if (status) *status = PAS_TT_OK;
    return 1;
}

PAS_TT_DEF void
pas_tt_get_vmetrics(const pas_tt_font_t *font, int *ascent, int *descent, int *line_gap)
{
    if (!font) return;
    if (ascent)  *ascent  = font->ascent;
    if (descent) *descent = font->descent;
    if (line_gap)*line_gap= font->line_gap;
}

/* read loca entry for glyph_index -> offset into glyf (byte offset from glyf table start) */
static uint32_t pas_tt__glyph_offset(const pas_tt_font_t *font, int glyph_index)
{
    const uint8_t *data = font->data;
    size_t size = font->size;
    uint32_t loca = font->loca_offset;

    if (glyph_index < 0 || (uint16_t)glyph_index >= font->num_glyphs) return 0;

    if (font->index_to_loc_format == 0) {
        size_t off = loca + (size_t)(glyph_index) * 2;
        if (!pas_tt__in_range(off, 4, size)) return 0;
        uint16_t a = pas_tt__u16_be(data + off);
        uint16_t b = pas_tt__u16_be(data + off + 2);
        if (a == b) return 0; /* empty glyph */
        return (uint32_t)a * 2;
    } else if (font->index_to_loc_format == 1) {
        size_t off = loca + (size_t)(glyph_index) * 4;
        if (!pas_tt__in_range(off, 8, size)) return 0;
        uint32_t a = pas_tt__u32_be(data + off);
        uint32_t b = pas_tt__u32_be(data + off + 4);
        if (a == b) return 0;
        return a;
    }
    return 0;
}

PAS_TT_DEF void
pas_tt_get_glyph_hmetrics(const pas_tt_font_t *font, int glyph_index,
                          int *advance_width, int *left_side_bearing)
{
    const uint8_t *data;
    uint32_t hmtx;
    uint16_t num_hm;
    int glyph = glyph_index;

    if (!font) return;

    data = font->data;
    hmtx = font->hmtx_offset;
    num_hm = font->num_hmetrics;

    if (glyph < 0) glyph = 0;
    if ((uint16_t)glyph >= font->num_glyphs)
        glyph = font->num_glyphs ? (int)(font->num_glyphs - 1) : 0;

    if ((uint16_t)glyph < num_hm) {
        size_t off = hmtx + (size_t)glyph * 4;
        if (!pas_tt__in_range(off, 4, font->size)) return;
        if (advance_width)      *advance_width      = (int)pas_tt__u16_be(data + off + 0);
        if (left_side_bearing)  *left_side_bearing  = (int)pas_tt__s16_be(data + off + 2);
    } else {
        /* use last advance, specific lsb */
        size_t off_aw = hmtx + (size_t)(num_hm - 1) * 4;
        size_t off_lsb = hmtx + (size_t)num_hm * 4 + (size_t)(glyph - num_hm) * 2;
        if (!pas_tt__in_range(off_aw, 4, font->size)) return;
        if (!pas_tt__in_range(off_lsb, 2, font->size)) return;
        if (advance_width)      *advance_width      = (int)pas_tt__u16_be(data + off_aw + 0);
        if (left_side_bearing)  *left_side_bearing  = (int)pas_tt__s16_be(data + off_lsb + 0);
    }
}

/* cmap helpers */
static int pas_tt__cmap_format0_glyph(const uint8_t *table, size_t table_len, int codepoint)
{
    if (table_len < 6 + 256) return 0;
    if (codepoint < 0 || codepoint > 255) return 0;
    return table[6 + codepoint];
}

static int pas_tt__cmap_format4_glyph(const uint8_t *table, size_t table_len, int codepoint)
{
    int segCount, i;
    uint16_t segCountX2, rangeShift;
    const uint8_t *endCount, *startCount, *idDelta, *idRangeOffset;

    if (table_len < 16) return 0;

    segCountX2 = pas_tt__u16_be(table + 6);
    segCount = segCountX2 / 2;
    if (segCount <= 0) return 0;

    if ((size_t)(16 + segCount * 8) > table_len) return 0;

    endCount = table + 14;
    startCount = endCount + 2 + segCount * 2;
    idDelta = startCount + segCount * 2;
    idRangeOffset = idDelta + segCount * 2;
    rangeShift = pas_tt__u16_be(table + 12);
    (void)rangeShift;

    for (i = 0; i < segCount; i++) {
        uint16_t end   = pas_tt__u16_be(endCount + 2 * i);
        uint16_t start = pas_tt__u16_be(startCount + 2 * i);
        uint16_t delta = pas_tt__u16_be(idDelta + 2 * i);
        uint16_t roff  = pas_tt__u16_be(idRangeOffset + 2 * i);

        if ((uint16_t)codepoint < start || (uint16_t)codepoint > end)
            continue;

        if (roff == 0) {
            return (uint16_t)(codepoint + delta);
        } else {
            size_t glyphOffsetAddr =
                (idRangeOffset - table) + 2 * i + roff + 2 * (codepoint - start);
            if (glyphOffsetAddr + 2 > table_len) return 0;
            {
                uint16_t glyphIndex = pas_tt__u16_be(table + glyphOffsetAddr);
                if (glyphIndex == 0) return 0;
                return (uint16_t)(glyphIndex + delta);
            }
        }
    }
    return 0;
}

PAS_TT_DEF int
pas_tt_get_glyph_index(const pas_tt_font_t *font, int codepoint)
{
    const uint8_t *data;
    uint32_t cmap_off, cmap_len;

    if (!font || codepoint < 0) return 0;

    data = font->data;
    cmap_off = font->cmap_table_offset;
    cmap_len = font->cmap_table_length;

    if (!pas_tt__in_range(cmap_off, cmap_len, font->size)) return 0;

    {
        const uint8_t *cmap = data + cmap_off;
        uint16_t version = pas_tt__u16_be(cmap + 0);
        uint16_t numTables = pas_tt__u16_be(cmap + 2);
        size_t i;

        if (version != 0) return 0;
        if (cmap_len < 4 + numTables * 8) return 0;

        /* preference: platform 3 (Windows) unicode, then platform 0 (Unicode) */
        int best_index = -1;
        for (i = 0; i < numTables; i++) {
            const uint8_t *rec = cmap + 4 + i * 8;
            uint16_t platformID = pas_tt__u16_be(rec + 0);
            uint16_t encodingID = pas_tt__u16_be(rec + 2);
            if (platformID == 3 && (encodingID == 1 || encodingID == 10)) {
                best_index = (int)i;
                break;
            }
            if (best_index < 0 && platformID == 0) {
                best_index = (int)i;
            }
        }
        if (best_index < 0) return 0;

        {
            const uint8_t *rec = cmap + 4 + best_index * 8;
            uint32_t subOffset = pas_tt__u32_be(rec + 4);
            if (subOffset > cmap_len) return 0;

            const uint8_t *sub = cmap + subOffset;
            size_t sub_len = cmap_len - subOffset;
            uint16_t format = pas_tt__u16_be(sub + 0);

            if (format == 0) {
                return pas_tt__cmap_format0_glyph(sub, sub_len, codepoint);
            } else if (format == 4) {
                return pas_tt__cmap_format4_glyph(sub, sub_len, codepoint);
            }
        }
    }

    return 0;
}

PAS_TT_DEF void
pas_tt_get_glyph_box(const pas_tt_font_t *font, int glyph_index,
                     int *x0, int *y0, int *x1, int *y1)
{
    const uint8_t *data;
    uint32_t g_off;

    if (x0) *x0 = 0;
    if (y0) *y0 = 0;
    if (x1) *x1 = 0;
    if (y1) *y1 = 0;

    if (!font) return;

    data = font->data;
    g_off = pas_tt__glyph_offset(font, glyph_index);
    if (!g_off) return;

    if (!pas_tt__in_range(font->glyf_offset + g_off, 10, font->size)) return;

    {
        const uint8_t *g = data + font->glyf_offset + g_off;
        /* int16_t numberOfContours = pas_tt__s16_be(g + 0); */
        int16_t xMin = pas_tt__s16_be(g + 2);
        int16_t yMin = pas_tt__s16_be(g + 4);
        int16_t xMax = pas_tt__s16_be(g + 6);
        int16_t yMax = pas_tt__s16_be(g + 8);

        if (x0) *x0 = xMin;
        if (y0) *y0 = yMin;
        if (x1) *x1 = xMax;
        if (y1) *y1 = yMax;
    }
}

PAS_TT_DEF float
pas_tt_scale_for_pixel_height(const pas_tt_font_t *font, float pixel_height)
{
    if (!font || font->units_per_em == 0) return 0.0f;
    return pixel_height / (float)font->units_per_em;
}

PAS_TT_DEF int
pas_tt_get_glyph_bitmap_box(const pas_tt_font_t *font, int glyph_index,
                            float scale_x, float scale_y,
                            int *x0, int *y0, int *x1, int *y1)
{
    int gx0 = 0, gy0 = 0, gx1 = 0, gy1 = 0;

    if (!font) return 0;
    pas_tt_get_glyph_box(font, glyph_index, &gx0, &gy0, &gx1, &gy1);
    if (gx0 == gx1 && gy0 == gy1) {
        if (x0) *x0 = 0;
        if (y0) *y0 = 0;
        if (x1) *x1 = 0;
        if (y1) *y1 = 0;
        return 0;
    }

    if (x0) *x0 = (int)(gx0 * scale_x);
    if (y0) *y0 = (int)(gy0 * scale_y);
    if (x1) *x1 = (int)(gx1 * scale_x + 0.999f);
    if (y1) *y1 = (int)(gy1 * scale_y + 0.999f);
    return 1;
}

#endif /* PAS_TRUETYPE_IMPLEMENTATION */

#endif /* PAS_TRUETYPE_H */


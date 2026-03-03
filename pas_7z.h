/*
    pas_7z.h - single-header 7z reader (stb-style, minimal)

    - No malloc: user-provided buffers; archive read from memory.
    - Lists files and extracts only when the folder uses Copy (no compression) codec.
    - Supports only non-packed (raw) header; packed/encoded header returns PAS_7Z_E_UNSUPPORTED.
    - File names: UTF-16LE in 7z, converted to UTF-8 in the name buffer.

    Usage:
        In ONE translation unit:
            #define PAS_7Z_IMPLEMENTATION
            #include "pas_7z.h"

        In others:
            #include "pas_7z.h"
*/

#ifndef PAS_7Z_H
#define PAS_7Z_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PAS_7Z_OK            0
#define PAS_7Z_E_INVALID    -1
#define PAS_7Z_E_NOT_FOUND  -2
#define PAS_7Z_E_COMPRESSED -3
#define PAS_7Z_E_UNSUPPORTED -4
#define PAS_7Z_E_NOSPACE    -5
#define PAS_7Z_E_RANGE      -6

typedef int pas_7z_status;

typedef struct pas_7z pas_7z_t;
typedef struct pas_7z_file pas_7z_file_t;

#ifndef PAS_7Z_MAX_NAME
#define PAS_7Z_MAX_NAME 512
#endif
#ifndef PAS_7Z_MAX_FILES
#define PAS_7Z_MAX_FILES 2048
#endif

struct pas_7z {
    const uint8_t *data;
    size_t         size;
    uint64_t       pack_pos;   /* offset of pack streams (after sig header) */
    int            num_files;
    int            has_compressed; /* non-zero if any folder is not Copy */
};

struct pas_7z_file {
    const char *name;
    uint64_t    size;
    uint64_t    data_offset;   /* into archive (only valid if Copy) */
    int         is_dir;
    int         is_compressed; /* non-zero if in non-Copy folder */
};

/* Open 7z from memory. data/size must remain valid. */
pas_7z_t *pas_7z_open(const void *data, size_t size, pas_7z_status *status);

/* Find file by name (case-sensitive, UTF-8). Returns NULL if not found. */
pas_7z_file_t *pas_7z_find(pas_7z_t *arch, const char *name);

const char *pas_7z_name(pas_7z_file_t *file);
uint64_t    pas_7z_size(pas_7z_file_t *file);
int         pas_7z_is_compressed(pas_7z_file_t *file);
int         pas_7z_is_dir(pas_7z_file_t *file);

/* Extract to buffer. Only for non-dir, Copy (uncompressed) entries. */
size_t pas_7z_extract(pas_7z_file_t *file, void *buffer, size_t buffer_size, pas_7z_status *status);

/* List all files. callback(name_utf8, size, is_dir, user). Returns 0 on success. */
int pas_7z_list(pas_7z_t *arch, void (*callback)(const char *name, uint64_t size, int is_dir, void *user), void *user);

#ifdef __cplusplus
}
#endif

/* ========== Implementation ========== */

#ifdef PAS_7Z_IMPLEMENTATION

#include <string.h>

static const unsigned char pas_7z_sig[6] = { '7', 'z', 0xBC, 0xAF, 0x27, 0x1C };
#define PAS_7Z_SIG_SIZE 6
#define PAS_7Z_START_HEADER_SIZE 32

static pas_7z_t pas_7z__handle;
static pas_7z_file_t pas_7z__files[PAS_7Z_MAX_FILES];
static char pas_7z__name_buf[PAS_7Z_MAX_NAME];
static char pas_7z__names[PAS_7Z_MAX_FILES][PAS_7Z_MAX_NAME];

/* 7z NUMBER: variable-length 1-9 byte encoding */
static int pas_7z__read_number(const uint8_t *p, const uint8_t *end, uint64_t *out, size_t *len) {
    if (!p || p >= end || !out || !len) return 0;
    unsigned b = *p++;
    *len = 1;
    if (b < 0x80) { *out = b; return 1; }
    if (b < 0xC0) { if (p >= end) return 0; *out = ((uint64_t)(b & 0x3Fu) << 8) | *p++; *len = 2; return 1; }
    if (b < 0xE0) { if (p + 2 > end) return 0; *out = ((uint64_t)(b & 0x1Fu) << 16) | (uint64_t)p[0] | ((uint64_t)p[1] << 8); *len = 3; return 1; }
    if (b < 0xF0) { if (p + 3 > end) return 0; *out = ((uint64_t)(b & 0x0Fu) << 24) | (uint64_t)p[0] | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16); *len = 4; return 1; }
    if (b < 0xF8) { if (p + 4 > end) return 0; *out = ((uint64_t)(b & 0x07u) << 32) | (uint64_t)p[0] | ((uint64_t)p[1]<<8) | ((uint64_t)p[2]<<16) | ((uint64_t)p[3]<<24); *len = 5; return 1; }
    if (b < 0xFC) { if (p + 5 > end) return 0; *out = ((uint64_t)(b & 0x03u) << 40) | (uint64_t)p[0] | ((uint64_t)p[1]<<8) | ((uint64_t)p[2]<<16) | ((uint64_t)p[3]<<24) | ((uint64_t)p[4]<<32); *len = 6; return 1; }
    if (b < 0xFE) { if (p + 6 > end) return 0; *out = (uint64_t)p[0] | ((uint64_t)p[1]<<8) | ((uint64_t)p[2]<<16) | ((uint64_t)p[3]<<24) | ((uint64_t)p[4]<<32) | ((uint64_t)p[5]<<40); *len = 7; return 1; }
    if (b == 0xFE) { if (p + 7 > end) return 0; *out = (uint64_t)p[0] | ((uint64_t)p[1]<<8) | ((uint64_t)p[2]<<16) | ((uint64_t)p[3]<<24) | ((uint64_t)p[4]<<32) | ((uint64_t)p[5]<<40) | ((uint64_t)p[6]<<48); *len = 8; return 1; }
    if (p + 8 > end) return 0; *out = (uint64_t)p[0] | ((uint64_t)p[1]<<8) | ((uint64_t)p[2]<<16) | ((uint64_t)p[3]<<24) | ((uint64_t)p[4]<<32) | ((uint64_t)p[5]<<40) | ((uint64_t)p[6]<<48) | ((uint64_t)p[7]<<56); *len = 9; return 1;
}

static uint64_t pas_7z__read_u64_le(const uint8_t *p) {
    return (uint64_t)p[0] | ((uint64_t)p[1]<<8) | ((uint64_t)p[2]<<16) | ((uint64_t)p[3]<<24) |
           ((uint64_t)p[4]<<32) | ((uint64_t)p[5]<<40) | ((uint64_t)p[6]<<48) | ((uint64_t)p[7]<<56);
}

/* UTF-16LE to UTF-8 into pas_7z__name_buf, return 1 on success */
static int pas_7z__utf16le_to_utf8(const uint8_t *src, size_t src_units, size_t *used) {
    char *dst = pas_7z__name_buf;
    size_t cap = sizeof(pas_7z__name_buf) - 1;
    size_t i = 0;
    size_t j = 0;
    *used = 0;
    while (i + 2 <= src_units && j < cap) {
        uint32_t cp = (uint32_t)src[i] | ((uint32_t)src[i+1] << 8);
        i += 2;
        if (cp == 0) break;
        if (cp < 0x80) {
            dst[j++] = (char)cp;
        } else if (cp < 0x800) {
            if (j + 2 > cap) return 0;
            dst[j++] = (char)(0xC0 | (cp >> 6));
            dst[j++] = (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0xD800 || cp > 0xDFFF) {
            if (j + 3 > cap) return 0;
            dst[j++] = (char)(0xE0 | (cp >> 12));
            dst[j++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            dst[j++] = (char)(0x80 | (cp & 0x3F));
        } else {
            if (i + 2 > src_units) return 0;
            uint32_t lo = (uint32_t)src[i] | ((uint32_t)src[i+1] << 8);
            i += 2;
            if (cp < 0xDC00 || lo < 0xDC00 || lo > 0xDFFF) return 0;
            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
            if (j + 4 > cap) return 0;
            dst[j++] = (char)(0xF0 | (cp >> 18));
            dst[j++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            dst[j++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            dst[j++] = (char)(0x80 | (cp & 0x3F));
        }
    }
    dst[j] = '\0';
    *used = i;
    return 1;
}

/* Skip optional property block (read id, if not 0 read size and skip) */
static const uint8_t *pas_7z__skip_property(const uint8_t *p, const uint8_t *end) {
    if (p >= end) return NULL;
    if (*p == 0) return p + 1;
    p++;
    uint64_t sz;
    size_t nlen;
    if (!pas_7z__read_number(p, end, &sz, &nlen)) return NULL;
    p += nlen;
    if ((uint64_t)(end - p) < sz) return NULL;
    return p + (size_t)sz;
}

/* Parse PackInfo; fill pack_pos, num_pack, pack_sizes[]. Returns pointer after block or NULL. */
static const uint8_t *pas_7z__parse_pack_info(const uint8_t *p, const uint8_t *end,
    uint64_t *pack_pos, uint64_t *num_pack, uint64_t *pack_sizes, int max_pack)
{
    if (p >= end || *p != 0x06) return NULL;
    p++;
    size_t nlen;
    if (!pas_7z__read_number(p, end, pack_pos, &nlen)) return NULL;
    p += nlen;
    if (!pas_7z__read_number(p, end, num_pack, &nlen)) return NULL;
    p += nlen;
    if (*num_pack > (uint64_t)max_pack) return NULL;
    if (p >= end || *p != 0x09) return NULL;
    p++;
    for (uint64_t i = 0; i < *num_pack; i++) {
        if (!pas_7z__read_number(p, end, &pack_sizes[i], &nlen)) return NULL;
        p += nlen;
    }
    if (p < end && *p == 0x0A) {
        p++;
        uint64_t all_def;
        if (!pas_7z__read_number(p, end, &all_def, &nlen)) return NULL;
        p += nlen;
        if (all_def == 0) {
            int bits = 0;
            for (uint64_t i = 0; i < *num_pack; i++) {
                if (bits == 0) { if (p >= end) return NULL; all_def = *p++; bits = 8; }
                bits--;
            }
        }
        for (uint64_t i = 0; i < *num_pack; i++) p += 4;
    }
    if (p >= end || *p != 0) return NULL;
    p++;
    return p;
}

/* Check folder is single Copy codec (id 0x00). Returns 1 if Copy-only. */
static int pas_7z__folder_is_copy(const uint8_t *p, const uint8_t *end) {
    uint64_t num_coders;
    size_t nlen;
    if (!pas_7z__read_number(p, end, &num_coders, &nlen)) return 0;
    p += nlen;
    if (num_coders != 1) return 0;
    if (p >= end) return 0;
    unsigned char flags = *p++;
    unsigned id_size = flags & 0x0Fu;
    if (id_size != 1 || p + 1 > end) return 0;
    if (*p != 0x00) return 0;
    return 1;
}

/* Parse UnPackInfo (Folders, CodersUnPackSize, optional CRC). Fill folders_copy[] and unpack_sizes[]. */
static const uint8_t *pas_7z__parse_unpack_info(const uint8_t *p, const uint8_t *end,
    uint64_t *num_folders, int *folders_copy, uint64_t *unpack_sizes, int max_folders, int max_streams)
{
    if (p >= end || *p != 0x07) return NULL;
    p++;
    if (p >= end || *p != 0x0B) return NULL;
    p++;
    size_t nlen;
    if (!pas_7z__read_number(p, end, num_folders, &nlen)) return NULL;
    p += nlen;
    if (*num_folders > (uint64_t)max_folders) return NULL;
    if (p >= end) return NULL;
    unsigned char external = *p++;
    if (external != 0) return NULL;
    const uint8_t *folder_start = p;
    for (uint64_t f = 0; f < *num_folders; f++) {
        folders_copy[f] = pas_7z__folder_is_copy(p, end);
        uint64_t num_coders;
        if (!pas_7z__read_number(p, end, &num_coders, &nlen)) return NULL;
        p += nlen;
        for (uint64_t c = 0; c < num_coders; c++) {
            if (p >= end) return NULL;
            unsigned fl = *p++;
            unsigned id_sz = fl & 0x0Fu;
            if (p + id_sz > end) return NULL;
            p += id_sz;
            if (fl & 0x10) { uint64_t attr_sz; if (!pas_7z__read_number(p, end, &attr_sz, &nlen)) return NULL; p += nlen + (size_t)attr_sz; }
            if (fl & 0x08) { uint64_t skip; pas_7z__read_number(p, end, &skip, &nlen); p += nlen; pas_7z__read_number(p, end, &skip, &nlen); p += nlen; }
        }
        uint64_t num_bind = (num_coders > 0 ? num_coders - 1 : 0);
        uint64_t skip;
        for (uint64_t i = 0; i < num_bind; i++) { pas_7z__read_number(p, end, &skip, &nlen); p += nlen; pas_7z__read_number(p, end, &skip, &nlen); p += nlen; }
        if (num_coders > 0) {
            uint64_t num_in_total = num_coders, num_packed = 1;
            if (num_packed > 1) for (uint64_t i = 0; i < num_packed; i++) { pas_7z__read_number(p, end, &skip, &nlen); p += nlen; }
        }
    }
    if (p >= end || *p != 0x0C) return NULL;
    p++;
    int stream_idx = 0;
    for (uint64_t f = 0; f < *num_folders; f++) {
        uint64_t num_out = 1;
        for (int i = 0; i < 1; i++) {
            if (stream_idx >= max_streams) return NULL;
            if (!pas_7z__read_number(p, end, &unpack_sizes[stream_idx], &nlen)) return NULL;
            p += nlen;
            stream_idx++;
        }
    }
    if (p < end && *p == 0x0A) { uint64_t skip; p++; pas_7z__read_number(p, end, &skip, &nlen); p += nlen; for (uint64_t f = 0; f < *num_folders; f++) p += 4; }
    if (p >= end || *p != 0) return NULL;
    p++;
    return p;
}

/* Parse SubStreamsInfo: fill num_unpack_in_folder[], then unpack_sizes[]. */
static const uint8_t *pas_7z__parse_substreams(const uint8_t *p, const uint8_t *end,
    uint64_t num_folders, uint64_t *num_unpack_in_folder, uint64_t *unpack_sizes, int max_streams)
{
    if (p >= end || *p != 0x08) return NULL;
    p++;
    size_t nlen;
    uint64_t total_streams = 0;
    if (p >= end || *p != 0x0D) return NULL;
    p++;
    for (uint64_t f = 0; f < num_folders; f++) {
        if (!pas_7z__read_number(p, end, &num_unpack_in_folder[f], &nlen)) return NULL;
        p += nlen;
        total_streams += num_unpack_in_folder[f];
    }
    if (p < end && *p == 0x09) {
        p++;
        for (uint64_t i = 0; i < total_streams; i++) {
            if (i >= (uint64_t)max_streams) return NULL;
            if (!pas_7z__read_number(p, end, &unpack_sizes[i], &nlen)) return NULL;
            p += nlen;
        }
    }
    while (p < end && *p != 0) p = pas_7z__skip_property(p, end);
    if (p >= end || *p != 0) return NULL;
    p++;
    return p;
}

pas_7z_t *pas_7z_open(const void *data, size_t size, pas_7z_status *status) {
    if (status) *status = PAS_7Z_E_INVALID;
    if (!data || size < PAS_7Z_START_HEADER_SIZE) return NULL;
    const uint8_t *d = (const uint8_t *)data;
    if (memcmp(d, pas_7z_sig, PAS_7Z_SIG_SIZE) != 0) return NULL;

    uint64_t next_off = pas_7z__read_u64_le(d + 12);
    uint64_t next_sz = pas_7z__read_u64_le(d + 20);
    if ((uint64_t)size < 32 + next_off + next_sz) return NULL;

    const uint8_t *header = d + 32 + (size_t)next_off;
    if (header[0] == 0x17) {
        if (status) *status = PAS_7Z_E_UNSUPPORTED;
        return NULL;
    }
    if (header[0] != 0x01) return NULL;

    pas_7z__handle.data = d;
    pas_7z__handle.size = size;
    pas_7z__handle.pack_pos = 0;
    pas_7z__handle.num_files = 0;
    pas_7z__handle.has_compressed = 0;

    const uint8_t *p = header + 1;
    const uint8_t *end = header + next_sz;
    uint64_t pack_pos = 0, num_pack = 0;
    uint64_t pack_sizes[256];
    uint64_t num_folders = 0;
    int folders_copy[256];
    uint64_t unpack_sizes[4096];
    uint64_t num_unpack_in_folder[256];

    while (p < end && *p != 0x00) {
        if (*p == 0x02) { p = pas_7z__skip_property(p, end); continue; }
        if (*p == 0x03) { p = pas_7z__skip_property(p, end); continue; }
        if (*p == 0x04) {
            p++;
            if (p >= end || *p != 0x06) break;
            p = pas_7z__parse_pack_info(p, end, &pack_pos, &num_pack, pack_sizes, 256);
            if (!p) return NULL;
            pas_7z__handle.pack_pos = 32 + pack_pos;
            if (p < end && *p == 0x07) {
                p = pas_7z__parse_unpack_info(p, end, &num_folders, folders_copy, unpack_sizes, 256, 4096);
                if (!p) return NULL;
            }
            if (p < end && *p == 0x08) {
                p = pas_7z__parse_substreams(p, end, num_folders, num_unpack_in_folder, unpack_sizes, 4096);
                if (!p) return NULL;
            }
            while (p < end && *p != 0x00) p = pas_7z__skip_property(p, end);
            if (p < end) p++;
            continue;
        }
        if (*p == 0x05) break;
        p = pas_7z__skip_property(p, end);
    }

    if (p >= end || *p != 0x05) return NULL;
    p++;
    uint64_t num_files;
    size_t nlen;
    if (!pas_7z__read_number(p, end, &num_files, &nlen)) return NULL;
    p += nlen;
    if (num_files > PAS_7Z_MAX_FILES) { if (status) *status = PAS_7Z_E_NOSPACE; return NULL; }

    int empty_stream[PAS_7Z_MAX_FILES];
    int empty_file[PAS_7Z_MAX_FILES];
    memset(empty_stream, 0, sizeof(empty_stream));
    memset(empty_file, 0, sizeof(empty_file));

    while (p < end && *p != 0x00) {
        if (*p == 0x0E) {
            p++;
            int bits = 0;
            unsigned char byte = 0;
            for (uint64_t i = 0; i < num_files; i++) {
                if (bits == 0) { if (p >= end) return NULL; byte = *p++; bits = 8; }
                empty_stream[i] = (byte >> 7) & 1;
                byte <<= 1;
                bits--;
            }
            continue;
        }
        if (*p == 0x0F) {
            p++;
            int bits = 0;
            unsigned char byte = 0;
            uint64_t empty_idx = 0;
            for (uint64_t i = 0; i < num_files; i++) {
                if (empty_stream[i]) {
                    if (bits == 0) { if (p >= end) return NULL; byte = *p++; bits = 8; }
                    empty_file[i] = (byte >> 7) & 1;
                    byte <<= 1;
                    bits--;
                }
            }
            continue;
        }
        if (*p == 0x11) {
            p++;
            if (p >= end) return NULL;
            if (*p != 0) return NULL;
            p++;
            uint64_t stream_idx = 0;
            uint64_t folder_stream_off = 0;
            uint64_t folder_pack_off = 32 + pack_pos;
            int cur_folder = 0;
            uint64_t stream_in_folder = 0;
            for (uint64_t i = 0; i < num_files; i++) {
                if ((int)i >= PAS_7Z_MAX_FILES) break;
                pas_7z__files[i].size = 0;
                pas_7z__files[i].data_offset = 0;
                pas_7z__files[i].is_compressed = 1;
                pas_7z__files[i].is_dir = (empty_stream[i] && empty_file[i]);
                if (!empty_stream[i]) {
                    if (cur_folder < (int)num_folders && folders_copy[cur_folder] &&
                        stream_idx < 4096u) {
                        pas_7z__files[i].size = unpack_sizes[stream_idx];
                        pas_7z__files[i].data_offset = folder_pack_off + folder_stream_off;
                        pas_7z__files[i].is_compressed = 0;
                    } else if (cur_folder < (int)num_folders) {
                        pas_7z__handle.has_compressed = 1;
                    }
                    folder_stream_off += unpack_sizes[stream_idx];
                    stream_idx++;
                    stream_in_folder++;
                    if (cur_folder < (int)num_folders && stream_in_folder >= num_unpack_in_folder[cur_folder]) {
                        folder_pack_off += (cur_folder < 256 ? pack_sizes[cur_folder] : 0);
                        cur_folder++;
                        folder_stream_off = 0;
                        stream_in_folder = 0;
                    }
                }
                const uint8_t *name_start = p;
                while (p + 2 <= end) {
                    uint16_t w = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
                    p += 2;
                    if (w == 0) break;
                }
                size_t name_len = (size_t)(p - name_start - 2);
                if (name_len > 0 && pas_7z__utf16le_to_utf8(name_start, name_len, &name_len)) {
                    size_t copy = strlen(pas_7z__name_buf);
                    if (copy >= PAS_7Z_MAX_NAME - 1) copy = PAS_7Z_MAX_NAME - 2;
                    memcpy(pas_7z__names[i], pas_7z__name_buf, copy + 1);
                    pas_7z__files[i].name = pas_7z__names[i];
                } else {
                    pas_7z__names[i][0] = '\0';
                    pas_7z__files[i].name = pas_7z__names[i];
                }
            }
            pas_7z__handle.num_files = (int)num_files;
            if (status) *status = PAS_7Z_OK;
            return &pas_7z__handle;
        }
        p = pas_7z__skip_property(p, end);
    }

    if (status) *status = PAS_7Z_E_INVALID;
    return NULL;
}

pas_7z_file_t *pas_7z_find(pas_7z_t *arch, const char *name) {
    if (!arch || !name) return NULL;
    for (int i = 0; i < arch->num_files; i++) {
        if (strcmp(pas_7z__files[i].name, name) == 0)
            return &pas_7z__files[i];
    }
    return NULL;
}

const char *pas_7z_name(pas_7z_file_t *f) { return f ? f->name : NULL; }
uint64_t pas_7z_size(pas_7z_file_t *f) { return f ? f->size : 0; }
int pas_7z_is_compressed(pas_7z_file_t *f) { return f ? f->is_compressed : 0; }
int pas_7z_is_dir(pas_7z_file_t *f) { return f ? f->is_dir : 0; }

size_t pas_7z_extract(pas_7z_file_t *file, void *buffer, size_t buffer_size, pas_7z_status *status) {
    if (status) *status = PAS_7Z_E_INVALID;
    if (!file || !buffer) return 0;
    if (file->is_dir) return 0;
    if (file->is_compressed) { if (status) *status = PAS_7Z_E_COMPRESSED; return 0; }
    if (file->size > (uint64_t)SIZE_MAX) { if (status) *status = PAS_7Z_E_RANGE; return 0; }
    if (buffer_size < (size_t)file->size) { if (status) *status = PAS_7Z_E_NOSPACE; return 0; }
    const uint8_t *d = pas_7z__handle.data;
    size_t sz = pas_7z__handle.size;
    if ((uint64_t)file->data_offset + file->size > (uint64_t)sz) return 0;
    memcpy(buffer, d + file->data_offset, (size_t)file->size);
    if (status) *status = PAS_7Z_OK;
    return (size_t)file->size;
}

int pas_7z_list(pas_7z_t *arch, void (*callback)(const char *name, uint64_t size, int is_dir, void *user), void *user) {
    if (!arch || !callback) return -1;
    for (int i = 0; i < arch->num_files; i++) {
        callback(pas_7z__files[i].name, pas_7z__files[i].size, pas_7z__files[i].is_dir, user);
    }
    return 0;
}

#endif
#endif

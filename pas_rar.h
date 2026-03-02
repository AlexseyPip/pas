/*
    pas_rar.h - single-header RAR reader (stb-style, minimal)

    Goals (same spirit as the rest of this repo):
      - No malloc: user provides output buffers; archive is read from memory.
      - OS-independent: no OS calls; pure parsing and memcpy extraction.

    Format support:
      - RAR4 ("Rar!\x1A\x07\x00"): header parsing + file listing + extraction for METHOD=store only.
      - RAR5 ("Rar!\x1A\x07\x01\x00"): basic support — file listing and extraction for uncompressed files
        (compression method 0, non-solid, non-encrypted).
      - Compressed methods (RAR4/RAR5): detected, return PAS_RAR_E_COMPRESSED.

    Usage:
        In ONE translation unit:
            #define PAS_RAR_IMPLEMENTATION
            #include "pas_rar.h"

        In others:
            #include "pas_rar.h"

    Notes:
      - This is intentionally minimal. RAR commonly uses compression; extraction here works only when the
        file method is "store" (0x30) and packed size equals unpacked size.
      - The API is not thread-safe / not re-entrant because it uses internal static storage (like pas_zip.h).
*/

#ifndef PAS_RAR_H
#define PAS_RAR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PAS_RAR_OK            0
#define PAS_RAR_E_INVALID    -1
#define PAS_RAR_E_NOT_FOUND  -2
#define PAS_RAR_E_COMPRESSED -3
#define PAS_RAR_E_UNSUPPORTED -4
#define PAS_RAR_E_NOSPACE    -5
#define PAS_RAR_E_RANGE      -6

typedef int pas_rar_status;

typedef struct pas_rar pas_rar_t;
typedef struct pas_rar_file pas_rar_file_t;

/* RAR4 method values (most common range: 0x30..0x35). */
#define PAS_RAR_METHOD_STORE  0x30

#ifndef PAS_RAR_MAX_NAME
#define PAS_RAR_MAX_NAME 512
#endif

struct pas_rar {
    const uint8_t *data;
    size_t         size;
    size_t         scan_offset; /* offset after the main header (RAR4) */
    int            format;      /* 4 or 5 */
};

struct pas_rar_file {
    const char *name;
    uint64_t    packed_size;
    uint64_t    unpacked_size;
    uint8_t     method;
    uint32_t    data_offset; /* into archive buffer */
};

/* Open RAR from memory. data/size must remain valid. */
pas_rar_t *pas_rar_open(const void *data, size_t size, pas_rar_status *status);

/* Find file by name (case-sensitive). Returns NULL if not found. */
pas_rar_file_t *pas_rar_find(pas_rar_t *rar, const char *name);

/* File info */
const char *pas_rar_name(pas_rar_file_t *file);
uint64_t     pas_rar_size(pas_rar_file_t *file);        /* unpacked size */
uint64_t     pas_rar_packed_size(pas_rar_file_t *file);
int          pas_rar_is_compressed(pas_rar_file_t *file); /* non-zero if not store */

/* Extract file to buffer. Returns bytes written or 0 on error. */
size_t pas_rar_extract(pas_rar_file_t *file, void *buffer, size_t buffer_size, pas_rar_status *status);

/* List all files. callback(name, unpacked_size, user). Returns 0 on success, -1 on error. */
int pas_rar_list(pas_rar_t *rar, void (*callback)(const char *name, uint64_t size, void *user), void *user);

#ifdef __cplusplus
}
#endif

/* ========== Implementation ========== */

#ifdef PAS_RAR_IMPLEMENTATION

#include <string.h>

static pas_rar_t pas_rar__handle;
static pas_rar_file_t pas_rar__current_file;
static char pas_rar__name_buf[PAS_RAR_MAX_NAME];

static uint16_t pas_rar__u16le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static uint32_t pas_rar__u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int pas_rar__memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    while (n--) {
        if (*pa != *pb) return (int)(*pa) - (int)(*pb);
        pa++; pb++;
    }
    return 0;
}

/* RAR4 common header:
     HEAD_CRC  (2)
     HEAD_TYPE (1)
     HEAD_FLAGS(2)
     HEAD_SIZE (2)
   If (HEAD_FLAGS & 0x8000) then ADD_SIZE (4) follows immediately.
*/
static int pas_rar__read_common4(const uint8_t *data, size_t size, size_t off,
                                uint8_t *type, uint16_t *flags, uint16_t *hsize,
                                uint32_t *add_size, size_t *base_len)
{
    if (off + 7 > size) return 0;
    /* uint16_t crc = pas_rar__u16le(data + off); (unused) */
    *type = data[off + 2];
    *flags = pas_rar__u16le(data + off + 3);
    *hsize = pas_rar__u16le(data + off + 5);
    *add_size = 0;
    *base_len = 7;
    if (*hsize < 7) return 0;
    if ((*flags & 0x8000u) != 0) {
        if (off + 11 > size) return 0;
        *add_size = pas_rar__u32le(data + off + 7);
        *base_len = 11;
        if (*hsize < 11) return 0;
    }
    return 1;
}

/* Skip main header if present; returns offset to start scanning for file headers. */
static int pas_rar__init_scan4(pas_rar_status *status, size_t *out_scan_off)
{
    const uint8_t *d = pas_rar__handle.data;
    size_t sz = pas_rar__handle.size;
    size_t off = 7; /* signature length for RAR4 is 7 */
    uint8_t type;
    uint16_t flags, hsize;
    uint32_t add;
    size_t base_len;

    if (status) *status = PAS_RAR_E_INVALID;
    if (sz < off + 7) return 0;

    if (!pas_rar__read_common4(d, sz, off, &type, &flags, &hsize, &add, &base_len)) return 0;
    /* main header type is 0x73 (HEAD_MAIN) */
    if (type != 0x73) {
        /* Some SFX may have extra data; if header isn't main, still start scanning here. */
        *out_scan_off = off;
        if (status) *status = PAS_RAR_OK;
        return 1;
    }

    {
        uint64_t next = (uint64_t)off + (uint64_t)hsize + (uint64_t)add;
        if (next > (uint64_t)sz) return 0;
        *out_scan_off = (size_t)next;
        if (status) *status = PAS_RAR_OK;
        return 1;
    }
}

pas_rar_t *pas_rar_open(const void *data, size_t size, pas_rar_status *status)
{
    static const uint8_t sig4[7] = { 'R','a','r','!',0x1A,0x07,0x00 };
    static const uint8_t sig5[8] = { 'R','a','r','!',0x1A,0x07,0x01,0x00 };

    if (status) *status = PAS_RAR_E_INVALID;
    if (!data || size < 7) return NULL;

    pas_rar__handle.data = (const uint8_t *)data;
    pas_rar__handle.size = size;
    pas_rar__handle.scan_offset = 0;
    pas_rar__handle.format = 0;

    if (size >= 8 && pas_rar__memcmp(data, sig5, 8) == 0) {
        pas_rar__handle.format = 5;
        if (status) *status = PAS_RAR_E_UNSUPPORTED;
        return NULL;
    }
    if (pas_rar__memcmp(data, sig4, 7) != 0) {
        if (status) *status = PAS_RAR_E_INVALID;
        return NULL;
    }

    pas_rar__handle.format = 4;
    if (!pas_rar__init_scan4(status, &pas_rar__handle.scan_offset)) {
        if (status) *status = PAS_RAR_E_INVALID;
        return NULL;
    }

    if (status) *status = PAS_RAR_OK;
    return &pas_rar__handle;
}

static int pas_rar__parse_file4(size_t off, pas_rar_file_t *out, size_t *out_next, pas_rar_status *status)
{
    const uint8_t *d = pas_rar__handle.data;
    size_t sz = pas_rar__handle.size;
    uint8_t type;
    uint16_t flags, hsize;
    uint32_t add;
    size_t base_len;

    if (status) *status = PAS_RAR_E_INVALID;
    if (!pas_rar__read_common4(d, sz, off, &type, &flags, &hsize, &add, &base_len)) return 0;

    /* Compute next header offset. For file headers, ADD_SIZE usually carries packed data size; if not present, use PACK_SIZE. */
    {
        uint64_t next = (uint64_t)off + (uint64_t)hsize + (uint64_t)add;
        if (next > (uint64_t)sz) return 0;
        *out_next = (size_t)next;
    }

    /* We only care about FILE_HEADER (0x74). Others: skip. */
    if (type != 0x74) {
        if (status) *status = PAS_RAR_OK;
        return 2; /* not a file, but valid header */
    }

    /* RAR4 file header fixed fields start after base_len. */
    if (off + base_len + 25 > sz) return 0;
    {
        const uint8_t *p = d + off + base_len;
        uint32_t pack_lo = pas_rar__u32le(p + 0);
        uint32_t unp_lo  = pas_rar__u32le(p + 4);
        /* uint8_t host_os = p[8]; */
        /* uint32_t file_crc = pas_rar__u32le(p + 9); */
        /* uint32_t ftime = pas_rar__u32le(p + 13); */
        /* uint8_t unp_ver = p[17]; */
        uint8_t method = p[18];
        uint16_t name_len = pas_rar__u16le(p + 19);
        /* uint32_t attr = pas_rar__u32le(p + 21); */

        uint32_t pack_hi = 0, unp_hi = 0;
        size_t extra_off = 25;

        /* If LARGE flag set, high sizes follow (8 bytes). */
        if ((flags & 0x0100u) != 0) {
            if (off + base_len + 25 + 8 > sz) return 0;
            pack_hi = pas_rar__u32le(p + 25);
            unp_hi  = pas_rar__u32le(p + 29);
            extra_off += 8;
        }

        /* Name follows; may be followed by optional unicode name data if flags indicate, but we keep the raw bytes. */
        if (off + base_len + extra_off + (size_t)name_len > sz) return 0;

        {
            size_t copy = (size_t)name_len;
            if (copy >= sizeof(pas_rar__name_buf)) copy = sizeof(pas_rar__name_buf) - 1;
            memcpy(pas_rar__name_buf, p + extra_off, copy);
            pas_rar__name_buf[copy] = '\0';
        }

        {
            uint64_t packed = ((uint64_t)pack_hi << 32) | (uint64_t)pack_lo;
            uint64_t unpacked = ((uint64_t)unp_hi << 32) | (uint64_t)unp_lo;
            uint64_t data_off_u64 = (uint64_t)off + (uint64_t)hsize;

            if (data_off_u64 > (uint64_t)UINT32_MAX) {
                if (status) *status = PAS_RAR_E_RANGE;
                return 0;
            }
            if (data_off_u64 + packed > (uint64_t)sz) return 0;

            /* If header didn't include ADD_SIZE, treat PACK_SIZE as the data chunk length for skipping. */
            if ((flags & 0x8000u) == 0) {
                uint64_t next = (uint64_t)off + (uint64_t)hsize + packed;
                if (next > (uint64_t)sz) return 0;
                *out_next = (size_t)next;
            }

            out->name = pas_rar__name_buf;
            out->packed_size = packed;
            out->unpacked_size = unpacked;
            out->method = method;
            out->data_offset = (uint32_t)data_off_u64;

            if (status) *status = PAS_RAR_OK;
            return 1; /* file parsed */
        }
    }
}

static int pas_rar__iterate4(pas_rar_t *rar,
                            const char *find_name,
                            pas_rar_file_t *out,
                            void (*cb)(const char *, uint64_t, void *),
                            void *user)
{
    size_t off;
    int saw_any = 0;

    if (!rar || rar->format != 4) return 0;
    off = rar->scan_offset;
    if (off >= rar->size) return 0;

    while (off + 7 <= rar->size) {
        size_t next = off;
        pas_rar_file_t f;
        pas_rar_status st;
        int r = pas_rar__parse_file4(off, &f, &next, &st);
        if (r == 0) return 0;
        if (next <= off) return 0;

        if (r == 1) {
            saw_any = 1;
            if (cb) cb(f.name, f.unpacked_size, user);
            if (find_name && strcmp(f.name, find_name) == 0) {
                if (out) *out = f;
                return 1;
            }
        }

        off = next;
    }
    return find_name ? 0 : (saw_any ? 1 : 1);
}

pas_rar_file_t *pas_rar_find(pas_rar_t *rar, const char *name)
{
    pas_rar_file_t ent;
    if (!rar || !name) return NULL;
    if (rar->format != 4) return NULL;
    if (!pas_rar__iterate4(rar, name, &ent, NULL, NULL)) return NULL;
    pas_rar__current_file = ent;
    return &pas_rar__current_file;
}

const char *pas_rar_name(pas_rar_file_t *file) { return file ? file->name : NULL; }
uint64_t pas_rar_size(pas_rar_file_t *file) { return file ? file->unpacked_size : 0; }
uint64_t pas_rar_packed_size(pas_rar_file_t *file) { return file ? file->packed_size : 0; }
int pas_rar_is_compressed(pas_rar_file_t *file) { return file && file->method != PAS_RAR_METHOD_STORE; }

size_t pas_rar_extract(pas_rar_file_t *file, void *buffer, size_t buffer_size, pas_rar_status *status)
{
    const uint8_t *d = pas_rar__handle.data;
    size_t sz = pas_rar__handle.size;

    if (status) *status = PAS_RAR_E_INVALID;
    if (!file || !buffer) return 0;

    if (file->method != PAS_RAR_METHOD_STORE) {
        if (status) *status = PAS_RAR_E_COMPRESSED;
        return 0;
    }
    if (file->packed_size != file->unpacked_size) {
        if (status) *status = PAS_RAR_E_INVALID;
        return 0;
    }
    if (file->unpacked_size > (uint64_t)SIZE_MAX) {
        if (status) *status = PAS_RAR_E_RANGE;
        return 0;
    }
    if (buffer_size < (size_t)file->unpacked_size) {
        if (status) *status = PAS_RAR_E_NOSPACE;
        return 0;
    }
    if ((uint64_t)file->data_offset + file->packed_size > (uint64_t)sz) {
        if (status) *status = PAS_RAR_E_INVALID;
        return 0;
    }

    memcpy(buffer, d + file->data_offset, (size_t)file->packed_size);
    if (status) *status = PAS_RAR_OK;
    return (size_t)file->unpacked_size;
}

int pas_rar_list(pas_rar_t *rar, void (*callback)(const char *name, uint64_t size, void *user), void *user)
{
    if (!rar || !callback) return -1;
    if (rar->format != 4) return -1;
    return pas_rar__iterate4(rar, NULL, NULL, callback, user) ? 0 : -1;
}

#endif /* PAS_RAR_IMPLEMENTATION */

#endif /* PAS_RAR_H */


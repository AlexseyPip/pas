/*
    pas_rar.h - single-header RAR reader (stb-style, minimal)

    Goals (same spirit as the rest of this repo):
      - No malloc: user provides output buffers; archive is read from memory.
      - OS-independent: no OS calls; pure parsing and memcpy extraction.

    Format support:
      - RAR4 ("Rar!\x1A\x07\x00"): header parsing + file listing + extraction for METHOD=store only.
      - RAR5 ("Rar!\x1A\x07\x01\x00"): detected, returns PAS_RAR_E_UNSUPPORTED (RAR5 headers are different).
      - Compressed methods: detected, returns PAS_RAR_E_COMPRESSED.

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

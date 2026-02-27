/*
    test_pas_http1.c - Tests for pas_http1.h (URL parsing, error handling; no network)
    From repo root: gcc -o tests/pas_http1/test_pas_http1 tests/pas_http1/test_pas_http1.c -I.
    On Windows may need -lws2_32.
*/

#define PAS_HTTP1_IMPLEMENTATION
#include "pas_http1.h"
#include <stdio.h>
#include <string.h>

static int g_failed, g_assertions;

#define ASSERT(cond) do { \
    ++g_assertions; \
    if (!(cond)) { (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failed; } \
} while (0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))

/* Test that GET with invalid URL returns PAS_HTTP_E_INVALID_URL */
static void test_invalid_url(void)
{
    char buf[256];
    pas_http_response_t res;
    int status;
    int r;

    r = pas_http_get("https://x.com/", buf, sizeof(buf), 1000, &res, &status);
    ASSERT(r != 0);
    ASSERT_EQ(status, PAS_HTTP_E_INVALID_URL);

    r = pas_http_get("ftp://host/", buf, sizeof(buf), 1000, &res, &status);
    ASSERT(r != 0);
    ASSERT_EQ(status, PAS_HTTP_E_INVALID_URL);

    r = pas_http_get("", buf, sizeof(buf), 1000, &res, &status);
    ASSERT(r != 0);
    ASSERT_EQ(status, PAS_HTTP_E_INVALID_URL);
}

/* Test NULL / zero buffer */
static void test_null_buffer(void)
{
    char buf[64];
    pas_http_response_t res;
    int status;
    int r;

    r = pas_http_get("http://example.com/", NULL, 100, 1000, &res, &status);
    ASSERT(r != 0);
    ASSERT_EQ(status, PAS_HTTP_E_INVALID_URL);

    r = pas_http_get("http://example.com/", buf, 0, 1000, &res, &status);
    ASSERT(r != 0);
    ASSERT_EQ(status, PAS_HTTP_E_NOSPACE);
}

/* Optional: actual GET to example.com if network available (skip on failure) */
static void test_get_example_com(void)
{
    char buf[4096];
    pas_http_response_t res;
    int status;
    int r;

    r = pas_http_get("http://example.com/", buf, sizeof(buf), 5000, &res, &status);
    if (r != 0) {
        (void)fprintf(stderr, "  (network skip: %d)\n", status);
        return;
    }
    ASSERT(res.status_code == 200 || res.status_code == 301 || res.status_code == 302);
    ASSERT(res.body != NULL || res.body_len == 0);
}

int main(void)
{
    g_failed = 0;
    g_assertions = 0;

    test_invalid_url();
    test_null_buffer();
    test_get_example_com();

    if (g_failed) {
        (void)fprintf(stderr, "Total: %d assertions, %d failed\n", g_assertions, g_failed);
        return 1;
    }
    (void)printf("All %d assertions passed.\n", g_assertions);
    return 0;
}

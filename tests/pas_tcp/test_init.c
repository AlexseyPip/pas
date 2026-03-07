/*
    test_init.c - Basic pas_tcp test.
    gcc -o test_init tests/pas_tcp/test_init.c -I. -DPAS_TCP_IMPLEMENTATION -lws2_32
*/
#ifndef PAS_TCP_IMPLEMENTATION
#define PAS_TCP_IMPLEMENTATION
#endif
#include "pas_tcp.h"
#include <stdio.h>

static int failed, assertions;
#define ASSERT(c) do { ++assertions; if (!(c)) { fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); ++failed; } } while(0)

int main(void) {
    pas_tcp_socket_t sock;
    failed = assertions = 0;
    ASSERT(pas_tcp_init(&sock) == 0);
    pas_tcp_close(&sock);
    if (failed) { fprintf(stderr, "%d/%d failed\n", failed, assertions); return 1; }
    printf("All %d assertions passed.\n", assertions);
    return 0;
}

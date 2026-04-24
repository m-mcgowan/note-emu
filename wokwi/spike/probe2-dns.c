#include "wokwi-api.h"
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>

void chip_init(void) {
    printf("[probe-2] Attempting DNS resolution...\n");
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *result = NULL;
    int err = getaddrinfo("softcard.blues.com", "443", &hints, &result);
    if (err != 0) {
        printf("[probe-2] FAIL: getaddrinfo returned %d\n", err);
        return;
    }
    printf("[probe-2] PASS: DNS resolved\n");
    freeaddrinfo(result);
}

#include "wokwi-api.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

void chip_init(void) {
    printf("[probe-3] Attempting TCP connection...\n");

    struct addrinfo hints = {0}, *result = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo("softcard.blues.com", "80", &hints, &result) != 0) {
        printf("[probe-3] FAIL: DNS\n");
        return;
    }

    int fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (fd < 0) {
        printf("[probe-3] FAIL: socket() returned %d\n", fd);
        freeaddrinfo(result);
        return;
    }

    if (connect(fd, result->ai_addr, result->ai_addrlen) != 0) {
        printf("[probe-3] FAIL: connect()\n");
        close(fd);
        freeaddrinfo(result);
        return;
    }

    printf("[probe-3] PASS: TCP connected\n");

    // Try a minimal HTTP request
    const char *req = "GET / HTTP/1.0\r\nHost: softcard.blues.com\r\n\r\n";
    send(fd, req, strlen(req), 0);

    char buf[256] = {0};
    int n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        printf("[probe-3] PASS: received %d bytes: %.40s...\n", n, buf);
    } else {
        printf("[probe-3] FAIL: recv returned %d\n", n);
    }

    close(fd);
    freeaddrinfo(result);
}

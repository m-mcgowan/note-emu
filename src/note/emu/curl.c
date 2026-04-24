// note-emu: libcurl HTTP backend implementation

// Required for clock_gettime / CLOCK_MONOTONIC under strict C standards on
// glibc (macOS libc exposes these unconditionally).
#define _POSIX_C_SOURCE 200809L

#include "curl.h"

#if !defined(ARDUINO) && !defined(ESP_PLATFORM)

#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ═══════════════════════════════════════════════════════════════════════
// Response accumulator for curl write callback
// ═══════════════════════════════════════════════════════════════════════

typedef struct {
    uint8_t *buf;
    size_t   buf_size;
    size_t   len;
} resp_ctx_t;

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    resp_ctx_t *ctx = (resp_ctx_t *)userdata;
    size_t bytes = size * nmemb;
    size_t space = ctx->buf_size - ctx->len;
    size_t n = bytes < space ? bytes : space;
    if (n > 0) {
        memcpy(ctx->buf + ctx->len, ptr, n);
        ctx->len += n;
    }
    return bytes;  // Always claim we consumed everything
}

// ═══════════════════════════════════════════════════════════════════════
// HTTP POST via libcurl
// ═══════════════════════════════════════════════════════════════════════

int note_emu_curl_post(
    const char *url,
    const char *const *headers,
    const uint8_t *body,
    size_t body_len,
    uint8_t *response_buf,
    size_t response_buf_size,
    size_t *response_len,
    int *http_status,
    void *ctx
) {
    (void)ctx;

    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // Set headers
    struct curl_slist *hdr_list = NULL;
    for (const char *const *h = headers; h && *h; h++) {
        hdr_list = curl_slist_append(hdr_list, *h);
    }
    if (hdr_list) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr_list);
    }

    // Use GET when no body provided (e.g. /v1/billing-accounts),
    // POST otherwise (softcard /v1/read and /v1/write).
    if (body && body_len > 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body_len);
    } else if (body) {
        // body is non-NULL but len is 0 — empty POST
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
    }
    // else: body is NULL — default GET

    // Response handling
    resp_ctx_t resp = {
        .buf = response_buf,
        .buf_size = response_buf_size,
        .len = 0,
    };
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);

    // Timeouts — 60s total to accommodate long-poll reads
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);

    int ret = -1;
    if (res == CURLE_OK) {
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        *http_status = (int)status;
        *response_len = resp.len;
        ret = 0;
    } else {
        *http_status = 0;
        *response_len = 0;
    }

    curl_slist_free_all(hdr_list);
    curl_easy_cleanup(curl);
    return ret;
}

// ═══════════════════════════════════════════════════════════════════════
// POSIX millisecond clock
// ═══════════════════════════════════════════════════════════════════════

uint32_t note_emu_posix_millis(void *ctx) {
    (void)ctx;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// ═══════════════════════════════════════════════════════════════════════
// Global init/cleanup
// ═══════════════════════════════════════════════════════════════════════

int note_emu_curl_init(void) {
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    return (res == CURLE_OK) ? 0 : -1;
}

void note_emu_curl_cleanup(void) {
    curl_global_cleanup();
}

#endif // !ARDUINO && !ESP_PLATFORM

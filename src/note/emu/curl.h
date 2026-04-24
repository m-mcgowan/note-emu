// note-emu: libcurl HTTP backend
//
// Provides a ready-made note_emu_http_post_fn implementation using libcurl,
// plus a POSIX millis() implementation. Use these when building native
// (non-embedded) applications.
//
// Usage:
//   note_emu_config_t config = {
//       .http_post = note_emu_curl_post,
//       .millis    = note_emu_posix_millis,
//       .api_token = getenv("NOTEHUB_PAT"),
//   };

#pragma once

// Only available on platforms with libcurl (not Arduino/embedded)
#if !defined(ARDUINO) && !defined(ESP_PLATFORM)

#include "emu.h"

#ifdef __cplusplus
extern "C" {
#endif

// HTTP POST implementation using libcurl.
// Thread-safe (each call creates its own CURL easy handle).
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
);

// Monotonic millisecond clock using clock_gettime (POSIX).
uint32_t note_emu_posix_millis(void *ctx);

// Initialize libcurl globally. Call once at startup.
// Returns 0 on success.
int note_emu_curl_init(void);

// Clean up libcurl globals. Call once at shutdown.
void note_emu_curl_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // !ARDUINO && !ESP_PLATFORM

// note-emu: Virtual Notecard transport over HTTP — core implementation

#include "emu.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// ═══════════════════════════════════════════════════════════════════════
// Internal state
// ═══════════════════════════════════════════════════════════════════════

struct note_emu {
    note_emu_config_t config;

    // Read buffer — holds bytes received from /v1/read that haven't
    // been consumed by serial_receive() yet.
    uint8_t read_buf[NOTE_EMU_READ_BUF_SIZE];
    size_t  read_pos;   // next byte to return
    size_t  read_len;   // total valid bytes in buffer

    // Transmit accumulator — serial_transmit() appends here;
    // data is flushed to /v1/write on flush=true or when the
    // buffer contains a newline.
    uint8_t tx_buf[NOTE_EMU_READ_BUF_SIZE];
    size_t  tx_len;

    // Authentication state
    char   session_cookie[256];
    bool   authenticated;

    // URL buffers (built once at create time)
    char   read_url[128];
    char   write_url[128];

    // Resolved UID (when auto-fetched from API token)
    char   resolved_uid[64];

    // Pre-built headers (rebuilt after authentication)
    char   uid_header[128];
    char   auth_header[280];  // "Authorization: Bearer ..." or "Cookie: ..."

    // Serial transport state — tracks whether a request was written
    // so serial_available() only polls the network when expecting a response.
    bool   awaiting_response;
};

// Global instance for note-c hook functions
static note_emu_t *s_global = NULL;

// ═══════════════════════════════════════════════════════════════════════
// Error strings
// ═══════════════════════════════════════════════════════════════════════

const char *note_emu_strerror(note_emu_err_t err) {
    switch (err) {
    case NOTE_EMU_OK:                 return "success";
    case NOTE_EMU_ERR_INVALID_CONFIG: return "invalid configuration (missing required fields)";
    case NOTE_EMU_ERR_ALLOC:          return "memory allocation failed";
    case NOTE_EMU_ERR_HTTP:           return "HTTP request failed";
    case NOTE_EMU_ERR_AUTH:           return "authentication failed";
    case NOTE_EMU_ERR_RESOLVE_UID:    return "could not resolve account UID from PAT";
    case NOTE_EMU_ERR_PARSE:          return "could not parse server response";
    case NOTE_EMU_ERR_INVALID_ARG:    return "invalid argument";
    default:                          return "unknown error";
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Logging
// ═══════════════════════════════════════════════════════════════════════

// Default log function — printf on native, available for override
static void default_log_fn(const char *msg, void *ctx) {
    (void)ctx;
#ifdef ARDUINO
    // On Arduino, Serial may not be available during early init.
    // The Arduino wrapper (NoteEmuArduino) sets a proper log callback.
    // Fall through to printf which maps to Serial on ESP32.
#endif
    printf("note-emu: %s\n", msg);
}

static void emu_log(const note_emu_config_t *config, const char *fmt, ...) {
    if (config->disable_logging) return;

    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    note_emu_log_fn fn = config->log_fn ? config->log_fn : default_log_fn;
    fn(buf, config->ctx);
}

// ═══════════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════════

static const char *service_url(const note_emu_config_t *c) {
    return c->service_url ? c->service_url : NOTE_EMU_DEFAULT_URL;
}

static void build_urls(note_emu_t *emu) {
    const char *base = service_url(&emu->config);
    snprintf(emu->read_url,  sizeof(emu->read_url),  "%s/v1/read",  base);
    snprintf(emu->write_url, sizeof(emu->write_url), "%s/v1/write", base);
}

static void build_headers(note_emu_t *emu) {
    snprintf(emu->uid_header, sizeof(emu->uid_header),
             "X-User-UID: %s", emu->config.user_uid);

    // Prefer API token, fall back to session cookie from login
    if (emu->config.api_token) {
        snprintf(emu->auth_header, sizeof(emu->auth_header),
                 "Authorization: Bearer %s", emu->config.api_token);
    } else if (emu->session_cookie[0]) {
        snprintf(emu->auth_header, sizeof(emu->auth_header),
                 "Cookie: %s", emu->session_cookie);
    } else {
        emu->auth_header[0] = '\0';
    }
}

// Build NULL-terminated header array for http_post.
// Returns pointer to static storage (not thread-safe).
static const char *const *request_headers(note_emu_t *emu) {
    static const char *hdrs[4];
    int i = 0;
    hdrs[i++] = emu->uid_header;
    if (emu->auth_header[0])
        hdrs[i++] = emu->auth_header;
    hdrs[i] = NULL;
    return hdrs;
}

// ═══════════════════════════════════════════════════════════════════════
// UID resolution — fetch account UID from Notehub API using bearer token
// ═══════════════════════════════════════════════════════════════════════

static note_emu_err_t note_emu_resolve_uid(note_emu_t *emu) {
    const char *hdrs[] = {NULL, NULL};
    char auth[280];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", emu->config.api_token);
    hdrs[0] = auth;

    uint8_t resp[1024];
    size_t resp_len = 0;
    int http_status = 0;

    emu_log(&emu->config, "resolving account UID from PAT via billing-accounts API");

    uint32_t t0 = emu->config.millis(emu->config.ctx);
    int rc = emu->config.http_post(
        "https://api.notefile.net/v1/billing-accounts", hdrs,
        NULL, 0,
        resp, sizeof(resp), &resp_len,
        &http_status, emu->config.ctx);
    uint32_t elapsed = emu->config.millis(emu->config.ctx) - t0;
    emu_log(&emu->config, "billing-accounts -> rc=%d http=%d [%u ms]",
            rc, http_status, (unsigned)elapsed);

    if (rc != 0) {
        emu_log(&emu->config, "billing-accounts request failed (transport error %d)", rc);
        return NOTE_EMU_ERR_HTTP;
    }

    if (http_status == 401 || http_status == 403) {
        emu_log(&emu->config, "billing-accounts returned %d — PAT may be invalid or expired", http_status);
        return NOTE_EMU_ERR_AUTH;
    }

    if (http_status != 200) {
        emu_log(&emu->config, "billing-accounts returned HTTP %d", http_status);
        return NOTE_EMU_ERR_HTTP;
    }

    if (resp_len == 0) {
        emu_log(&emu->config, "billing-accounts returned empty response");
        return NOTE_EMU_ERR_PARSE;
    }

    // Minimal JSON parse — find first "uid":"..." value.
    const char *p = (const char *)resp;
    const char *end = p + resp_len;
    const char *needle = "\"uid\":\"";
    const char *found = NULL;
    for (const char *s = p; s < end - 7; s++) {
        if (memcmp(s, needle, 7) == 0) {
            found = s + 7;
            break;
        }
    }
    if (!found) {
        emu_log(&emu->config, "could not find \"uid\" in billing-accounts response");
        return NOTE_EMU_ERR_PARSE;
    }

    const char *close = memchr(found, '"', (size_t)(end - found));
    if (!close) {
        emu_log(&emu->config, "malformed \"uid\" value in billing-accounts response");
        return NOTE_EMU_ERR_PARSE;
    }

    size_t uid_len = (size_t)(close - found);
    if (uid_len >= sizeof(emu->resolved_uid)) {
        emu_log(&emu->config, "UID too long (%zu bytes)", uid_len);
        return NOTE_EMU_ERR_PARSE;
    }

    memcpy(emu->resolved_uid, found, uid_len);
    emu->resolved_uid[uid_len] = '\0';
    emu->config.user_uid = emu->resolved_uid;

    emu_log(&emu->config, "resolved account UID: %s", emu->resolved_uid);

    return NOTE_EMU_OK;
}

// ═══════════════════════════════════════════════════════════════════════
// Create / destroy
// ═══════════════════════════════════════════════════════════════════════

note_emu_err_t note_emu_create(const note_emu_config_t *config, note_emu_t **emu_out) {
    if (emu_out) *emu_out = NULL;

    if (!config || !emu_out) {
        return NOTE_EMU_ERR_INVALID_ARG;
    }

    if (!config->http_post || !config->millis) {
        emu_log(config, "missing required config: http_post=%s millis=%s",
                config->http_post ? "set" : "null",
                config->millis ? "set" : "null");
        return NOTE_EMU_ERR_INVALID_CONFIG;
    }

    if (!config->user_uid && !config->api_token) {
        emu_log(config, "must provide either api_token or user_uid");
        return NOTE_EMU_ERR_INVALID_CONFIG;
    }

    note_emu_t *emu = (note_emu_t *)calloc(1, sizeof(note_emu_t));
    if (!emu) {
        emu_log(config, "failed to allocate %zu bytes", sizeof(note_emu_t));
        return NOTE_EMU_ERR_ALLOC;
    }

    emu->config = *config;
    build_urls(emu);

    emu_log(&emu->config, "connecting to %s", service_url(&emu->config));

    // If no UID provided, resolve it from the API token
    if (!emu->config.user_uid && emu->config.api_token) {
        note_emu_err_t err = note_emu_resolve_uid(emu);
        if (err != NOTE_EMU_OK) {
            free(emu);
            return err;
        }
    }

    build_headers(emu);

    emu_log(&emu->config, "ready (uid=%s)", emu->config.user_uid);

    *emu_out = emu;
    return NOTE_EMU_OK;
}

void note_emu_destroy(note_emu_t *emu) {
    if (!emu) return;
    if (s_global == emu) s_global = NULL;
    free(emu);
}

// ═══════════════════════════════════════════════════════════════════════
// Authentication
// ═══════════════════════════════════════════════════════════════════════

note_emu_err_t note_emu_authenticate(note_emu_t *emu) {
    if (!emu) return NOTE_EMU_ERR_INVALID_ARG;
    if (!emu->config.username || !emu->config.password) {
        emu->authenticated = true;
        return NOTE_EMU_OK;  // No credentials — skip auth
    }

    emu_log(&emu->config, "authenticating as %s", emu->config.username);

    char auth_url[128];
    snprintf(auth_url, sizeof(auth_url),
             "https://api.notefile.net/auth/login");

    char body[256];
    int body_len = snprintf(body, sizeof(body),
        "{\"username\":\"%s\",\"password\":\"%s\"}",
        emu->config.username, emu->config.password);

    const char *hdrs[] = {"Content-Type: application/json", NULL};
    uint8_t resp[512];
    size_t resp_len = 0;
    int http_status = 0;

    int rc = emu->config.http_post(
        auth_url, hdrs,
        (const uint8_t *)body, (size_t)body_len,
        resp, sizeof(resp), &resp_len,
        &http_status, emu->config.ctx);

    if (rc != 0) {
        emu_log(&emu->config, "auth request failed (transport error %d)", rc);
        return NOTE_EMU_ERR_HTTP;
    }

    if (http_status != 200) {
        emu_log(&emu->config, "auth returned HTTP %d", http_status);
        return NOTE_EMU_ERR_AUTH;
    }

    emu->authenticated = true;
    build_headers(emu);

    emu_log(&emu->config, "authenticated successfully");
    return NOTE_EMU_OK;
}

// ═══════════════════════════════════════════════════════════════════════
// Direct read/write
// ═══════════════════════════════════════════════════════════════════════

int note_emu_write(note_emu_t *emu, const uint8_t *data, size_t len) {
    if (!emu || !data || len == 0) return NOTE_EMU_ERR_INVALID_ARG;

    if (!emu->authenticated) {
        note_emu_err_t err = note_emu_authenticate(emu);
        if (err != NOTE_EMU_OK) return err;
    }

    uint8_t resp[64];
    size_t resp_len = 0;
    int http_status = 0;

    uint32_t t0 = emu->config.millis(emu->config.ctx);
    int rc = emu->config.http_post(
        emu->write_url, request_headers(emu),
        data, len,
        resp, sizeof(resp), &resp_len,
        &http_status, emu->config.ctx);
    uint32_t elapsed = emu->config.millis(emu->config.ctx) - t0;

    emu_log(&emu->config, "POST /v1/write (%zu bytes) -> rc=%d http=%d [%u ms]",
            len, rc, http_status, (unsigned)elapsed);
    if (rc != 0) {
        emu_log(&emu->config, "write failed (transport error %d)", rc);
        return NOTE_EMU_ERR_HTTP;
    }

    // Handle auth failure — re-authenticate and retry once
    if (http_status == 401) {
        emu_log(&emu->config, "write returned 401, re-authenticating");
        emu->authenticated = false;
        note_emu_err_t err = note_emu_authenticate(emu);
        if (err != NOTE_EMU_OK) return err;

        rc = emu->config.http_post(
            emu->write_url, request_headers(emu),
            data, len,
            resp, sizeof(resp), &resp_len,
            &http_status, emu->config.ctx);
        if (rc != 0) return NOTE_EMU_ERR_HTTP;
    }

    if (http_status < 200 || http_status >= 300) {
        emu_log(&emu->config, "write returned HTTP %d", http_status);
        return NOTE_EMU_ERR_HTTP;
    }

    return NOTE_EMU_OK;
}

int note_emu_read(note_emu_t *emu, uint8_t *buf, size_t max_len) {
    if (!emu || !buf || max_len == 0) return NOTE_EMU_ERR_INVALID_ARG;

    // Return buffered data first
    if (emu->read_pos < emu->read_len) {
        size_t avail = emu->read_len - emu->read_pos;
        size_t n = avail < max_len ? avail : max_len;
        memcpy(buf, emu->read_buf + emu->read_pos, n);
        emu->read_pos += n;
        return (int)n;
    }

    if (!emu->authenticated) {
        note_emu_err_t err = note_emu_authenticate(emu);
        if (err != NOTE_EMU_OK) return err;
    }

    // Fetch more data from softcard
    size_t resp_len = 0;
    int http_status = 0;

    // Empty POST (body="" not NULL) — NULL body signals GET to some backends
    static const uint8_t empty = 0;
    uint32_t t0 = emu->config.millis(emu->config.ctx);
    int rc = emu->config.http_post(
        emu->read_url, request_headers(emu),
        &empty, 0,
        emu->read_buf, sizeof(emu->read_buf), &resp_len,
        &http_status, emu->config.ctx);
    uint32_t elapsed = emu->config.millis(emu->config.ctx) - t0;

    emu_log(&emu->config, "POST /v1/read -> rc=%d http=%d bytes=%zu [%u ms]",
            rc, http_status, resp_len, (unsigned)elapsed);
    if (rc != 0) return NOTE_EMU_ERR_HTTP;

    if (http_status == 401) {
        emu->authenticated = false;
        note_emu_err_t err = note_emu_authenticate(emu);
        if (err != NOTE_EMU_OK) return err;

        rc = emu->config.http_post(
            emu->read_url, request_headers(emu),
            &empty, 0,
            emu->read_buf, sizeof(emu->read_buf), &resp_len,
            &http_status, emu->config.ctx);
        if (rc != 0) return NOTE_EMU_ERR_HTTP;
    }

    if (resp_len == 0) return 0;

    emu->read_pos = 0;
    emu->read_len = resp_len;

    size_t n = resp_len < max_len ? resp_len : max_len;
    memcpy(buf, emu->read_buf, n);
    emu->read_pos = n;
    return (int)n;
}

// ═══════════════════════════════════════════════════════════════════════
// note-c serial hooks
// ═══════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════
// Instance-based serial protocol helpers
//
// All serial semantics (TX accumulation, local handshake, RX buffering,
// awaiting-response gate) live here. The note-c global hooks and the
// note-cpp note::emu::SerialHal class are both thin wrappers around these.
// ═══════════════════════════════════════════════════════════════════════

void note_emu_proto_reset(note_emu_t *emu) {
    if (!emu) return;
    emu->tx_len = 0;
    emu->read_pos = 0;
    emu->read_len = 0;
    emu->awaiting_response = false;
}

int note_emu_proto_transmit(note_emu_t *emu,
                            const uint8_t *data, size_t len) {
    if (!emu || !data) return NOTE_EMU_ERR_INVALID_ARG;

    for (size_t i = 0; i < len; i++) {
        if (emu->tx_len >= sizeof(emu->tx_buf))
            return NOTE_EMU_ERR_ALLOC;  // tx buffer overflow
        emu->tx_buf[emu->tx_len++] = data[i];

        // Flush on newline — one request per line per the Notecard protocol.
        if (data[i] == '\n') {
            // Bare \n is the reset handshake probe — answer \r\n locally
            // without hitting the network.
            if (emu->tx_len == 1) {
                emu->read_buf[0] = '\r';
                emu->read_buf[1] = '\n';
                emu->read_pos = 0;
                emu->read_len = 2;
                emu->tx_len = 0;
                continue;
            }
            int rc = note_emu_write(emu, emu->tx_buf, emu->tx_len);
            emu->tx_len = 0;
            if (rc != 0) return rc;
            emu->awaiting_response = true;
        }
    }
    return NOTE_EMU_OK;
}

bool note_emu_proto_available(note_emu_t *emu) {
    if (!emu) return false;

    // Serve from local buffer first.
    if (emu->read_pos < emu->read_len) return true;

    // Don't poll the network unless a response is actually pending.
    // NotecardSerial::reset() drains aggressively; without this gate
    // each drain attempt would be a 30s HTTP round-trip.
    if (!emu->awaiting_response) return false;

    int n = note_emu_read(emu, emu->read_buf, sizeof(emu->read_buf));
    if (n > 0) {
        emu->read_pos = 0;
        emu->read_len = (size_t)n;
        emu->awaiting_response = false;
        return true;
    }
    return false;
}

size_t note_emu_proto_receive(note_emu_t *emu, uint8_t *buf, size_t max_len) {
    if (!emu || !buf || max_len == 0) return 0;
    if (emu->read_pos >= emu->read_len) return 0;
    size_t avail = emu->read_len - emu->read_pos;
    size_t n = avail < max_len ? avail : max_len;
    memcpy(buf, emu->read_buf + emu->read_pos, n);
    emu->read_pos += n;
    return n;
}

// ═══════════════════════════════════════════════════════════════════════
// note-c serial hooks — thin wrappers around the instance-based helpers.
// ═══════════════════════════════════════════════════════════════════════

void note_emu_set_global(note_emu_t *emu) {
    s_global = emu;
}

bool note_emu_serial_reset(void) {
    if (!s_global) return false;
    note_emu_proto_reset(s_global);
    return true;
}

void note_emu_serial_transmit(uint8_t *buf, size_t len, bool flush) {
    (void)flush;  // note-c semantics: always flush at end-of-line
    if (!s_global) return;
    note_emu_proto_transmit(s_global, buf, len);
}

bool note_emu_serial_available(void) {
    return note_emu_proto_available(s_global);
}

char note_emu_serial_receive(void) {
    uint8_t byte;
    if (note_emu_proto_receive(s_global, &byte, 1) == 1)
        return (char)byte;
    return '\0';
}

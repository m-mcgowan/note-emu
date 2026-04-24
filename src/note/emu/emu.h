// note-emu: Virtual Notecard transport over HTTP
//
// Connects to Blues' softcard service (softcard.blues.com) to emulate a
// physical Notecard. Platform-agnostic core with a pluggable HTTP backend.
//
// Two integration paths:
//   1. note-c:   Install as serial hook callbacks via NoteSetFnSerial()
//   2. note-cpp: Implement SerialHal for note::transport::NotecardSerial
//
// The library does NOT depend on note-c or note-cpp — it provides the
// glue layer that either library can consume.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════
// Error codes
// ═══════════════════════════════════════════════════════════════════════

typedef enum {
    NOTE_EMU_OK               =  0,
    NOTE_EMU_ERR_INVALID_CONFIG = -1,   // Missing required config fields
    NOTE_EMU_ERR_ALLOC        = -2,     // Memory allocation failed
    NOTE_EMU_ERR_HTTP         = -3,     // HTTP request failed (transport error)
    NOTE_EMU_ERR_AUTH         = -4,     // Authentication failed (401 or bad credentials)
    NOTE_EMU_ERR_RESOLVE_UID  = -5,     // Could not resolve account UID from PAT
    NOTE_EMU_ERR_PARSE        = -6,     // Could not parse server response
    NOTE_EMU_ERR_INVALID_ARG  = -7,     // Invalid argument to function
} note_emu_err_t;

// Return a human-readable string for an error code.
const char *note_emu_strerror(note_emu_err_t err);

// ═══════════════════════════════════════════════════════════════════════
// Logging
// ═══════════════════════════════════════════════════════════════════════

// Log callback — receives formatted messages. If not set, note-emu logs
// to printf (native) or Serial (Arduino) by default. Set to NULL to
// suppress all logging.
typedef void (*note_emu_log_fn)(const char *msg, void *ctx);

// ═══════════════════════════════════════════════════════════════════════
// Platform HTTP abstraction
// ═══════════════════════════════════════════════════════════════════════

// The platform must provide an HTTP POST implementation. note-emu calls
// this for every softcard read/write. The implementation owns TLS,
// connection pooling, cookie storage, etc.
//
// Returns 0 on success, negative on error. On success, response_buf is
// filled with up to response_buf_size bytes and *response_len is set.
// HTTP status is written to *http_status.

typedef int (*note_emu_http_post_fn)(
    const char *url,
    const char *const *headers,   // NULL-terminated array of "Key: Value" strings
    const uint8_t *body,
    size_t body_len,
    uint8_t *response_buf,
    size_t response_buf_size,
    size_t *response_len,
    int *http_status,
    void *ctx
);

// Monotonic millisecond clock.
typedef uint32_t (*note_emu_millis_fn)(void *ctx);

// ═══════════════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════════════

#define NOTE_EMU_DEFAULT_URL "https://softcard.blues.com"
#define NOTE_EMU_READ_TIMEOUT_MS 30000
#define NOTE_EMU_READ_BUF_SIZE 1024

typedef struct {
    // Required
    note_emu_http_post_fn http_post;
    note_emu_millis_fn millis;
    void *ctx;                      // Passed to http_post and millis

    // Authentication — provide api_token (preferred) or username+password
    const char *api_token;          // Notehub PAT — also used to resolve user_uid
    const char *username;           // Notehub email (fallback if no token)
    const char *password;           // Notehub password

    // Optional — auto-resolved from api_token via GET /v1/billing-accounts
    const char *user_uid;           // Notehub account UID (X-User-UID header)

    // Optional — override defaults
    const char *service_url;        // Default: NOTE_EMU_DEFAULT_URL
    uint32_t read_timeout_ms;       // Default: NOTE_EMU_READ_TIMEOUT_MS

    // Optional — logging. Default: logs to printf/Serial.
    // Set log_fn to NULL to suppress logging.
    note_emu_log_fn log_fn;         // Log callback (NULL = default platform logging)
    bool disable_logging;           // Set true to suppress all logging
} note_emu_config_t;

// ═══════════════════════════════════════════════════════════════════════
// Instance
// ═══════════════════════════════════════════════════════════════════════

typedef struct note_emu note_emu_t;

// Create a note-emu instance. Returns NOTE_EMU_OK on success with *emu_out
// set to the new instance. On error, *emu_out is NULL and the return value
// indicates the failure reason.
note_emu_err_t note_emu_create(const note_emu_config_t *config, note_emu_t **emu_out);
void           note_emu_destroy(note_emu_t *emu);

// Authenticate to Notehub (if username/password configured).
// Called automatically on first use, or manually for early error detection.
// Returns NOTE_EMU_OK on success or if no credentials configured.
note_emu_err_t note_emu_authenticate(note_emu_t *emu);

// ═══════════════════════════════════════════════════════════════════════
// note-c serial hook interface
//
// After note_emu_create(), install these as note-c serial hooks:
//
//   note_emu_t *emu;
//   note_emu_create(&config, &emu);
//   NoteSetFnSerial(
//       note_emu_serial_reset,
//       note_emu_serial_transmit,
//       note_emu_serial_available,
//       note_emu_serial_receive
//   );
//
// Because note-c hooks are bare function pointers (no context),
// a single global instance is used. Call note_emu_set_global() first.
// ═══════════════════════════════════════════════════════════════════════

void note_emu_set_global(note_emu_t *emu);

bool note_emu_serial_reset(void);
void note_emu_serial_transmit(uint8_t *buf, size_t len, bool flush);
bool note_emu_serial_available(void);
char note_emu_serial_receive(void);

// ═══════════════════════════════════════════════════════════════════════
// Direct read/write (for note-cpp SerialHal or custom integrations)
// ═══════════════════════════════════════════════════════════════════════

// Write data to the softcard. Returns 0 on success.
int note_emu_write(note_emu_t *emu, const uint8_t *data, size_t len);

// Non-blocking read from the softcard. Returns bytes read (0 if none
// available yet). Negative on error.
int note_emu_read(note_emu_t *emu, uint8_t *buf, size_t max_len);

// ═══════════════════════════════════════════════════════════════════════
// Serial protocol helpers (instance-based — no global needed)
//
// These implement the Notecard serial protocol semantics on top of
// note_emu_write/read: batch accumulation until newline, local handshake
// response (\n → \r\n), and idle-poll suppression via an awaiting-response
// flag. Use these from any HAL-style adapter (e.g. note::emu::SerialHal)
// instead of reimplementing the same logic.
//
// The note-c hooks above are thin wrappers around these that use a global
// instance.
// ═══════════════════════════════════════════════════════════════════════

// Reset the serial protocol state (flush tx, clear rx, clear awaiting).
void note_emu_proto_reset(note_emu_t *emu);

// Feed bytes into the TX accumulator. On seeing '\n', flushes — sending
// the buffer to softcard, or answering a bare '\n' handshake locally.
// Returns 0 on success, negative on flush failure.
int note_emu_proto_transmit(note_emu_t *emu,
                            const uint8_t *data, size_t len);

// Return true if at least one byte is available to receive() without
// blocking (either buffered locally or a polled HTTP call returned data).
// Only polls the network when a response is actually pending — idle calls
// (e.g. during reset drain loops) return false immediately.
bool note_emu_proto_available(note_emu_t *emu);

// Copy up to max_len bytes from the receive buffer. Returns bytes copied.
// Pairs with note_emu_proto_available(): call available() to decide
// whether to poll, then receive() to consume buffered bytes.
size_t note_emu_proto_receive(note_emu_t *emu, uint8_t *buf, size_t max_len);

#ifdef __cplusplus
}
#endif

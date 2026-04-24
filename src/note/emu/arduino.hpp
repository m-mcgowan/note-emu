// note::emu::Arduino — Arduino integration for note-emu
//
// Provides a high-level wrapper for using note-emu on Arduino platforms
// with WiFi. Handles HTTP transport, connection reuse, and integration
// with both note-c and note-cpp.
//
// Usage (note-c):
//   #include <note/emu/arduino.hpp>
//   #include <Notecard.h>
//
//   note::emu::Arduino softcard(NOTEHUB_PAT);
//   softcard.begin(wifiClient);
//   softcard.installNoteC();
//   // use note-c normally: NoteRequestResponse(NoteNewRequest("card.version"))
//
// Usage (note-cpp):
//   #include <note/emu/arduino.hpp>
//   #include <note/arduino.hpp>
//
//   note::emu::Arduino softcard(NOTEHUB_PAT);
//   softcard.begin(wifiClient);
//   note::arduino::Notecard nc;
//   nc.begin(softcard.serialHal());

#pragma once

#ifdef ARDUINO

#include <Print.h>
#include <NetworkClient.h>
#include <HTTPClient.h>
#include "emu.h"

namespace note::emu {

class Arduino {
public:
    Arduino(const char *api_token);

    // Connect to softcard using the provided network client.
    // Typically a WiFiClientSecure with setInsecure() called.
    // Returns NOTE_EMU_OK on success.
    note_emu_err_t begin(NetworkClient &client);

    // Get the underlying note_emu_t instance. Use this with
    // note_emu_set_global() for note-c integration, or to create
    // a note::emu::SerialHal for note-cpp integration.
    note_emu_t *instance() { return _emu; }

    // Set the output stream for log messages. Default: Serial.
    void setDebugOutput(Print &output);
    void disableLogging();

    // Access the underlying config (e.g. to set service_url before begin).
    note_emu_config_t &config() { return _config; }

private:
    static int httpPost(
        const char *url, const char *const *headers,
        const uint8_t *body, size_t body_len,
        uint8_t *response_buf, size_t response_buf_size,
        size_t *response_len, int *http_status, void *ctx
    );
    static uint32_t getMillis(void *ctx);
    static void logToPrint(const char *msg, void *ctx);

    NetworkClient *_client;
    Print *_log_output;
    note_emu_config_t _config;
    note_emu_t *_emu;
    HTTPClient _http;
    bool _http_initialized;
};

}  // namespace note::emu

#endif // ARDUINO

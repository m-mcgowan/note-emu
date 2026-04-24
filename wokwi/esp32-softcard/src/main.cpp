// Wokwi ESP32 + note-emu integration
//
// Uses the note-c API with a virtual Notecard via the Blues softcard
// service over Wokwi's simulated WiFi. No physical Notecard needed.
//
// After setup, enters an interactive serial command loop: type JSON
// Notecard requests and receive real softcard responses.

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Notecard.h>

#include <note/emu/arduino.hpp>
#include "secrets.h"

// ── Globals ─────────────────────────────────────────────────────────

WiFiClientSecure wifiClient;
note::emu::Arduino softcard(NOTEHUB_PAT);

// ── Setup ───────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Wait for any character — gives time for serial monitor to connect
    uint32_t last_prompt = 0;
    while (Serial.read() == -1) {
        if (millis() - last_prompt > 1000) {
            Serial.println("press enter to start...");
            last_prompt = millis();
        }
    }

    Serial.println("note-emu benchmark");

    // WiFi (Wokwi provides an open network)
    Serial.print("WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf(" connected: %s", WiFi.localIP().toString().c_str());
    Serial.println();

    wifiClient.setInsecure();

    // Connect to softcard
    note_emu_err_t err = softcard.begin(wifiClient);
    if (err != NOTE_EMU_OK) {
        Serial.printf("softcard.begin failed: %s", note_emu_strerror(err));
        Serial.println();
        return;
    }

    // Install note-c serial hooks. NoteSetFnDefault is required when
    // using NoteSetFnSerial directly (notecard.begin() does this
    // automatically, but we're using the raw note-c API here).
    NoteSetFnDefault(malloc, free, delay, millis);
    note_emu_set_global(softcard.instance());
    NoteSetFnSerial(
        note_emu_serial_reset,
        note_emu_serial_transmit,
        note_emu_serial_available,
        note_emu_serial_receive
    );

    // Run 5 iterations for a profiling comparison against the note-cpp version.
    constexpr int ITERATIONS = 5;
    for (int i = 1; i <= ITERATIONS; i++) {
        Serial.printf("=== iteration %d/%d ===", i, ITERATIONS);
        Serial.println();
        uint32_t t0 = millis();
        J *rsp = NoteRequestResponse(NoteNewRequest("card.version"));
        uint32_t total = millis() - t0;
        if (rsp) {
            Serial.printf("PROFILE req=card.version total=%lu", total);
            Serial.println();
            Serial.printf("  version = %s", JGetString(rsp, "version"));
            Serial.println();
            NoteDeleteResponse(rsp);
        } else {
            Serial.println("  FAILED: no response");
        }
    }

    Serial.println("READY");
}

// ── Loop (interactive serial command) ───────────────────────────────

static char line_buf[1024];
static size_t line_pos = 0;

void loop() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (line_pos > 0) {
                line_buf[line_pos] = '\0';
                J *req = JParse(line_buf);
                if (!req) {
                    Serial.println("ERR: invalid JSON");
                } else {
                    J *rsp = NoteRequestResponse(req);
                    if (rsp) {
                        char *json = JPrintUnformatted(rsp);
                        if (json) {
                            Serial.printf("RSP: %s", json);
                            Serial.println();
                            JFree(json);
                        }
                        NoteDeleteResponse(rsp);
                    } else {
                        Serial.println("ERR: no response");
                    }
                }
                line_pos = 0;
            }
        } else if (line_pos < sizeof(line_buf) - 1) {
            line_buf[line_pos++] = c;
        }
    }
}

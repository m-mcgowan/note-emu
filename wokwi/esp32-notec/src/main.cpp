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

#include <note-emu.h>
#include "secrets.h"

// ── Globals ─────────────────────────────────────────────────────────

WiFiClientSecure wifiClient;
note::emu::Arduino softcard(NOTEHUB_PAT);

// ── Setup ───────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(1500);  // give the serial monitor a moment to attach

    Serial.println("note-emu on Wokwi (note-c)");

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

    // Install note-c serial hooks in one call.
    softcard.installNoteC();

    // Demo: hub.set + card.version, with the raw JSON traced.
    {
        J *req = NoteNewRequest("hub.set");
        JAddStringToObject(req, "product", "com.example.you:notec-demo");
        JAddStringToObject(req, "mode",    "continuous");
        if (char *s = JPrintUnformatted(req)) { Serial.printf("  > %s\n", s); JFree(s); }
        J *rsp = NoteRequestResponse(req);
        if (char *s = JPrintUnformatted(rsp)) { Serial.printf("  < %s\n", s); JFree(s); }
        Serial.printf("hub.set: %s\n",
                      (rsp && !JGetString(rsp, "err")[0]) ? "OK" : "FAIL");
        NoteDeleteResponse(rsp);
    }
    {
        J *req = NoteNewRequest("card.version");
        if (char *s = JPrintUnformatted(req)) { Serial.printf("  > %s\n", s); JFree(s); }
        J *rsp = NoteRequestResponse(req);
        if (char *s = JPrintUnformatted(rsp)) { Serial.printf("  < %s\n", s); JFree(s); }
        Serial.printf("card.version: %s\n", rsp ? JGetString(rsp, "version") : "FAIL");
        NoteDeleteResponse(rsp);
    }

    Serial.println("READY");
    Serial.println("Type a Notecard request as JSON and press Enter, e.g. {\"req\":\"card.temp\"}");
    Serial.print("> ");
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
                    if (char *s = JPrintUnformatted(req)) { Serial.printf("  > %s\n", s); JFree(s); }
                    J *rsp = NoteRequestResponse(req);
                    if (rsp) {
                        if (char *s = JPrintUnformatted(rsp)) { Serial.printf("  < %s\n", s); JFree(s); }
                        NoteDeleteResponse(rsp);
                    } else {
                        Serial.println("ERR: no response");
                    }
                }
                line_pos = 0;
                Serial.print("> ");
            }
        } else if (line_pos < sizeof(line_buf) - 1) {
            line_buf[line_pos++] = c;
        }
    }
}

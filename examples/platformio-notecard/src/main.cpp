// note-emu + note-arduino integration example
//
// Uses the standard Notecard Arduino API with a virtual Notecard
// via the Blues softcard service. No physical Notecard needed.
//
// After setup, enters a serial command loop: send JSON Notecard
// requests over USB serial and receive responses. This enables
// interactive use and automated integration testing.
//
// Protocol:
//   Input:  raw Notecard JSON, one request per line
//   Output: "RSP: <json>" for responses, "ERR: <msg>" for errors
//   Marker: "READY" printed when the command loop starts

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Notecard.h>

#include <note/emu/arduino.hpp>
#include "NoteSerial_Softcard.h"
#include "secrets.h"

// ── Globals ──────────────────────────────────────────────────────────

Notecard notecard;
WiFiClientSecure wifiClient;
note::emu::Arduino softcard(NOTEHUB_PAT);

// Serial line buffer for command input
static char line_buf[1024];
static size_t line_pos = 0;

// ── Command handler ─────────────────────────────────────────────────

static void handle_command(const char *line) {
    // Parse the line as JSON
    J *req = JParse(line);
    if (!req) {
        Serial.println("ERR: invalid JSON");
        return;
    }

    // Forward to the Notecard via note-arduino
    J *rsp = notecard.requestAndResponse(req);
    if (rsp) {
        char *json = JPrintUnformatted(rsp);
        if (json) {
            Serial.printf("RSP: %s\n", json);
            JFree(json);
        } else {
            Serial.println("ERR: failed to serialize response");
        }
        notecard.deleteResponse(rsp);
    } else {
        Serial.println("ERR: no response from Notecard");
    }
}

// ── Setup ───────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("note-emu + note-arduino example");

    // Connect WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf(" connected: %s\n", WiFi.localIP().toString().c_str());

    // Allow connections to softcard.blues.com without a root CA
    wifiClient.setInsecure();

    // Connect to softcard (resolves account UID from PAT)
    note_emu_err_t err = softcard.begin(wifiClient);
    if (err != NOTE_EMU_OK) {
        Serial.printf("ERROR: softcard.begin failed: %s\n", note_emu_strerror(err));
        return;
    }

    // Bridge note-emu to note-arduino via NoteSerial
    static NoteSerial_Softcard softcardSerial(softcard.instance());
    notecard.setDebugOutputStream(Serial);
    notecard.begin(&softcardSerial);

    // Use the standard Notecard API — exactly as with a physical Notecard
    {
        J *req = notecard.newRequest("hub.set");
        JAddStringToObject(req, "product", NOTEHUB_PRODUCT);
        JAddStringToObject(req, "mode", "periodic");
        if (!notecard.sendRequest(req)) {
            Serial.println("hub.set failed");
        }
    }

    // Query version to verify connectivity
    {
        J *rsp = notecard.requestAndResponse(notecard.newRequest("card.version"));
        if (rsp) {
            const char *version = JGetString(rsp, "version");
            const char *device = JGetString(rsp, "device");
            Serial.printf("Softcard version: %s\n", version);
            Serial.printf("Device: %s\n", device);
            notecard.deleteResponse(rsp);
        }
    }

    Serial.println("READY");
}

// ── Loop ────────────────────────────────────────────────────────────

void loop() {
    // Read serial input and process complete lines
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (line_pos > 0) {
                line_buf[line_pos] = '\0';
                handle_command(line_buf);
                line_pos = 0;
            }
        } else if (line_pos < sizeof(line_buf) - 1) {
            line_buf[line_pos++] = c;
        }
    }
}

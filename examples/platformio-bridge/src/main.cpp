// note-emu + note-c + note-cpp coexistence example.
//
// Both APIs work against the same virtual Notecard in one sketch. This
// is note-cpp's "bridge mode" (see note-cpp's migration guide) applied
// to a note-emu softcard transport.
//
// Setup order:
//   1. note::emu::Arduino softcard(PAT)         — HTTP client
//   2. softcard.begin(wifiClient)               — softcard session
//   3. softcard.installNoteC()                  — note-c owns transport
//   4. installNoteCppBridge(softcard)           — note-cpp on top

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// readme:coexistence-includes
// Disable note-cpp's blanket `using namespace note;` — otherwise
// note-arduino's global `Notecard` collides with note-cpp's
// `Notecard` alias for `note::arduino::Notecard`. See note-cpp's
// examples/arduino/note-arduino-bridge/src/main.cpp for context.
#define NOTE_USING_NAMESPACE 0
#include <Notecard.h>                    // note-arduino (brings in note-c + cJSON)
#include <note-cpp.h>                    // note-cpp typed API
#include <note-emu.h>                    // note-emu (auto-pulls note_cpp.hpp bridge base)
#include <note/emu/note_cpp_bridge.hpp>  // note-cpp bridge on top of note-c
// readme:end

#include "secrets.h"

// ── Globals ─────────────────────────────────────────────────────────

WiFiClientSecure wifiClient;
note::emu::Arduino softcard(NOTEHUB_PAT);

// ── Setup ───────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("note-emu bridge example (note-c + note-cpp)");

    // WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());
    wifiClient.setInsecure();

    // Connect to softcard
    if (auto err = softcard.begin(wifiClient); err != NOTE_EMU_OK) {
        Serial.printf("softcard.begin failed: %s\n", note_emu_strerror(err));
        return;
    }

    // readme:coexistence-install
    // 1. note-c owns the transport (installs global serial hooks
    //    pointing at note-emu's virtual Notecard).
    softcard.installNoteC();

    // 2. note-cpp bridges on top of note-c. Returns a Notecard
    //    whose typed calls route through NoteRequestResponseJSON().
    auto &nc = note::emu::installNoteCppBridge(softcard);
    note::Api api(nc);
    // readme:end

    // ── Both APIs now work against the same virtual Notecard ────────

    // readme:coexistence-usage
    // note-c: raw JSON API
    J *req = NoteNewRequest("hub.set");
    JAddStringToObject(req, "product", "com.example.you:bridge-demo");
    JAddStringToObject(req, "mode", "continuous");
    J *rsp = NoteRequestResponse(req);
    Serial.printf("hub.set (note-c): %s\n",
                  (rsp && !JGetString(rsp, "err")[0]) ? "OK" : "FAIL");
    NoteDeleteResponse(rsp);

    // note-cpp: typed API
    auto v = api.card.version().execute();
    if (v) {
        Serial.print("card.version (note-cpp): ");
        Serial.println(v.version);
    } else {
        Serial.print("card.version FAILED: ");
        Serial.println(v.error());
    }
    // readme:end

    Serial.println("READY");
}

void loop() {
    delay(10000);
}

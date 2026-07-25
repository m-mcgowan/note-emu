// Wokwi ESP32 + note-emu + note-cpp integration
//
// Uses the type-safe note-cpp API with a virtual Notecard via the Blues
// softcard service over Wokwi's simulated WiFi.
//
// Transport chain (streaming path — recommended):
//   note::emu::SerialHal          (HTTP to softcard)
//     → note::link::SerialFramer  (Hal)
//       → note::Protocol           (ITransact)
//         → note::Notecard                (streaming, no backend needed)

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// Direct include of <note-cpp.h> is what triggers Arduino's library
// auto-detection to add note-cpp to the include path (Arduino #5441 —
// a bare ".hpp" include isn't detected). Once note-cpp is visible,
// <note-emu.h> auto-pulls the note-emu ↔ note-cpp bridge.
#include <note-cpp.h>
#include <note-emu.h>

#include "secrets.h"

// ── Globals ─────────────────────────────────────────────────────────

WiFiClientSecure wifiClient;
note::emu::Arduino softcard(NOTEHUB_PAT);
static note::Notecard *nc_ptr = nullptr;   // set in setup(), used by loop()

// ── Setup ───────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(1500);  // give the serial monitor a moment to attach
    Serial.println("note-emu on Wokwi (note-cpp)");

    // WiFi
    Serial.print("WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASS, 6);  // channel 6 — skip the scan that hangs in the Wokwi web sim
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf(" connected: %s", WiFi.localIP().toString().c_str());
    Serial.println();

    wifiClient.setInsecure();

    // 1. Connect to softcard
    if (auto err = softcard.begin(wifiClient); err != NOTE_EMU_OK) {
        Serial.printf("softcard.begin failed: %s", note_emu_strerror(err));
        Serial.println();
        return;
    }

    // 2. Wire up the streaming transport stack in one call.
    auto &nc = note::emu::installNoteCpp(softcard);
    nc_ptr = &nc;
    note::Api api(nc);

    // Trace raw JSON requests/responses via DebugListener::on_wire.
    static note::DebugListener listener;
    listener.on_wire = [](const note::WireEvent &e, void *) {
        Serial.print(e.direction == note::WireDirection::Send ? "  > " : "  < ");
        Serial.write(reinterpret_cast<const uint8_t *>(e.json.data()), e.json.size());
        Serial.println();
    };
    nc.set_debug(listener);

    // Demo: hub.set + card.version, via the typed API (JSON traced by on_wire).
    if (auto r = api.hub.set().product("com.example.you:notecpp-demo").mode("continuous").execute(); r) {
        Serial.println("hub.set: OK");
    } else {
        Serial.print("hub.set FAILED: ");
        Serial.println(r.error());
    }
    if (auto r = api.card.version().execute(); r) {
        Serial.print("card.version: ");
        Serial.println(r.version);
    } else {
        Serial.print("card.version FAILED: ");
        Serial.println(r.error());
    }

    Serial.println("READY");
    Serial.println("Type a Notecard request as JSON and press Enter, e.g. {\"req\":\"card.temp\"}");
    Serial.print("> ");
}

// ── Loop (interactive serial command) ───────────────────────────────

static char line_buf[1024];
static size_t line_pos = 0;

void loop() {
    if (!nc_ptr) return;

    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (line_pos > 0) {
                line_buf[line_pos] = '\0';
                // on_wire hook prints the JSON request and response.
                auto result = nc_ptr->transact(note::string_view(line_buf));
                if (!result) {
                    Serial.print("ERR: ");
                    Serial.println(result.error());
                }
                line_pos = 0;
                Serial.print("> ");
            }
        } else if (line_pos < sizeof(line_buf) - 1) {
            line_buf[line_pos++] = c;
        }
    }
}

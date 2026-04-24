// note-emu + note-cpp integration example
//
// Uses the type-safe note-cpp API with a virtual Notecard via the Blues
// softcard service. No physical Notecard needed.
//
// Transport chain (streaming — no JSON backend required):
//   note::emu::SerialHal → NotecardSerial → StreamingTransport → Notecard
//
// After setup, enters a serial command loop: send JSON Notecard requests
// over USB serial and receive responses.

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <note/emu/arduino.hpp>
#include <note/emu/serial_hal.hpp>

#include <note/notecard.hpp>
#include <note/api.hpp>
#include <note/body.hpp>
#include <note/streaming_transport.hpp>
#include <note/transport/serial.hpp>

#include "secrets.h"

// ── Sensor data struct ──────────────────────────────────────────────

struct Readings {
    float temperature;
    int16_t humidity;
    NOTE_FIELDS(temperature, humidity)
};

// ── Globals ─────────────────────────────────────────────────────────

WiFiClientSecure wifiClient;
note::emu::Arduino softcard(NOTEHUB_PAT);

static note::Notecard *nc_ptr = nullptr;

// ── Setup ───────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("note-emu + note-cpp example");

    // Connect WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf(" connected: %s\n", WiFi.localIP().toString().c_str());

    wifiClient.setInsecure();

    // 1. Connect to softcard
    note_emu_err_t err = softcard.begin(wifiClient);
    if (err != NOTE_EMU_OK) {
        Serial.printf("ERROR: softcard.begin failed: %s\n", note_emu_strerror(err));
        return;
    }

    // 2. Wire up the streaming transport stack
    static note::emu::SerialHal hal(*softcard.instance(), millis, delay);
    static note::transport::NotecardSerial<> serial_hal(hal);
    static note::StreamingTransport transport(serial_hal);
    static note::Notecard nc(transport);
    nc_ptr = &nc;

    // 3. Use the typed API
    note::Api api(nc);

    // Configure hub
    auto hub_result = api.hub.set()
        .product(PRODUCT_UID)
        .mode("periodic")
        .outbound(60)
        .execute();
    if (hub_result) {
        Serial.println("hub.set OK");
    } else {
        Serial.print("hub.set failed: ");
        Serial.println(hub_result.error());
    }

    // Query version
    auto ver = api.card.version().execute();
    if (ver) {
        Serial.print("version: ");
        Serial.println(ver.version);
        Serial.print("device:  ");
        Serial.println(ver.device);
    }

    // Send a typed note
    Readings r{.temperature = 22.5f, .humidity = 60};
    auto add = api.note.add()
        .file("sensors.qo")
        .body(r)
        .execute();
    if (add) {
        Serial.printf("note.add OK (total=%d)\n", (int)add.total);
    } else {
        Serial.print("note.add failed: ");
        Serial.println(add.error());
    }

    Serial.println("READY");
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
                auto result = nc_ptr->transact(note::string_view(line_buf));
                if (result) {
                    Serial.printf("RSP: %.*s\n",
                        (int)result->size(), result->data());
                } else {
                    Serial.print("ERR: ");
                    Serial.println(result.error());
                }
                line_pos = 0;
            }
        } else if (line_pos < sizeof(line_buf) - 1) {
            line_buf[line_pos++] = c;
        }
    }
}

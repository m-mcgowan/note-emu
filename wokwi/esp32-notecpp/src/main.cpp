// Wokwi ESP32 + note-emu + note-cpp integration
//
// Uses the type-safe note-cpp API with a virtual Notecard via the Blues
// softcard service over Wokwi's simulated WiFi.
//
// Transport chain (streaming path — recommended):
//   note::emu::SerialHal          (HTTP to softcard)
//     → note::transport::NotecardSerial  (TransportHal)
//       → note::StreamingTransport        (IStreamingTransport)
//         → note::Notecard                (streaming, no backend needed)

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <note/emu/arduino.hpp>
#include <note/emu/serial_hal.hpp>

#include <note/notecard.hpp>
#include <note/api.hpp>
#include <note/streaming_transport.hpp>
#include <note/transport/serial.hpp>
#include <note/debug.hpp>

#include "secrets.h"

// ── Globals ─────────────────────────────────────────────────────────

WiFiClientSecure wifiClient;
note::emu::Arduino softcard(NOTEHUB_PAT);

// ── Setup ───────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("note-emu + note-cpp on Wokwi");

    // WiFi
    Serial.print("WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
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

    // 2. Wire up the streaming transport stack
    static note::emu::SerialHal hal(*softcard.instance(), millis, delay);
    static note::transport::NotecardSerial<> serial_hal(hal);
    static note::StreamingTransport transport(serial_hal);
    static note::Notecard nc(transport);

    // ── Per-transaction profiling ──────────────────────────────────
    // Track phase start times and accumulate spans. On TransactionEnd,
    // print a one-line summary of where time was spent.
    struct Profile {
        uint32_t txn_start = 0;
        uint32_t reset_start = 0, reset_ms = 0;
        uint32_t tx_start = 0,    tx_ms = 0;
        uint32_t rx_start = 0,    rx_ms = 0;
        uint32_t parse_start = 0, parse_ms = 0;
    };
    static Profile prof;

    note::DebugListener listener;
    listener.on_timing = [](note::TimingEvent ev, note::string_view req, void *) {
        uint32_t now = millis();
        using E = note::TimingEvent;
        switch (ev) {
            case E::TransactionBegin:
                prof = {};
                prof.txn_start = now;
                break;
            case E::ResetBegin:     prof.reset_start = now; break;
            case E::ResetEnd:       prof.reset_ms += now - prof.reset_start; break;
            case E::TransmitBegin:  prof.tx_start = now; break;
            case E::TransmitEnd:    prof.tx_ms += now - prof.tx_start; break;
            case E::ReceiveBegin:   prof.rx_start = now; break;
            case E::ReceiveEnd:     prof.rx_ms += now - prof.rx_start; break;
            case E::ParseBegin:     prof.parse_start = now; break;
            case E::ParseEnd:       prof.parse_ms += now - prof.parse_start; break;
            case E::TransactionEnd: {
                uint32_t total = now - prof.txn_start;
                Serial.printf(
                    "PROFILE req=%.*s total=%lu reset=%lu tx=%lu rx=%lu parse=%lu",
                    (int)req.size(), req.data(),
                    total, prof.reset_ms, prof.tx_ms, prof.rx_ms, prof.parse_ms);
                Serial.println();
                break;
            }
            default: break;
        }
    };
    nc.set_debug(listener);

    // 3. Run multiple requests back-to-back to see how much of the
    //    first-request cost is one-time (TLS handshake) vs persistent.
    note::Api api(nc);
    constexpr int ITERATIONS = 5;
    for (int i = 1; i <= ITERATIONS; i++) {
        Serial.printf("=== iteration %d/%d ===", i, ITERATIONS);
        Serial.println();
        if (auto r = api.card.version().execute(); r) {
            Serial.print("  version = ");
            Serial.println(r.version);
        } else {
            Serial.print("  FAILED: ");
            Serial.println(r.error());
        }
    }

    Serial.println("READY");
}

void loop() {
    delay(10000);
}

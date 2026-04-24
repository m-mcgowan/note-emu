// note-emu: Arduino example using note-cpp
//
// Connects to WiFi and uses the softcard service as a virtual Notecard.
// Application code uses note-cpp's type-safe API — same as with a physical
// Notecard, but requests go to the softcard cloud simulator over HTTP.
//
// Dependencies: note-emu, note-cpp (install via Arduino Library Manager)
// Board: any ESP32 with WiFi (tested on ESP32-S3)

#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <note/emu/arduino.hpp>
#include <note/emu/serial_hal.hpp>

#include <note/notecard.hpp>
#include <note/api.hpp>
#include <note/streaming_transport.hpp>
#include <note/transport/serial.hpp>

// ── Configuration ──────────────────────────────────────────────────────

#ifndef WIFI_SSID
#define WIFI_SSID "your-ssid"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "your-password"
#endif
#ifndef NOTEHUB_PAT
#define NOTEHUB_PAT "your-notehub-api-token"
#endif
#ifndef NOTEHUB_PRODUCT
#define NOTEHUB_PRODUCT "com.example.softcard"
#endif

// ── Globals ────────────────────────────────────────────────────────────

WiFiClientSecure wifiClient;
note::emu::Arduino softcard(NOTEHUB_PAT);

// ── Setup ──────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("note-emu: Arduino + note-cpp example");

    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf(" connected: %s\n", WiFi.localIP().toString().c_str());

    // Allow connections to softcard.blues.com without a root CA
    wifiClient.setInsecure();

    // Connect to the softcard service (resolves account UID from PAT)
    note_emu_err_t err = softcard.begin(wifiClient);
    if (err != NOTE_EMU_OK) {
        Serial.printf("ERROR: %s\n", note_emu_strerror(err));
        while (1) delay(1000);
    }

    // Wire up the transport:
    //   SerialHal → NotecardSerial → StreamingTransport → Notecard
    // No JSON backend needed — streaming parses directly from the wire.
    static note::emu::SerialHal hal(*softcard.instance(), millis, delay);
    static note::transport::NotecardSerial<> serial_transport(hal);
    static note::StreamingTransport transport(serial_transport);
    static note::Notecard nc(transport);

    // Use the typed API — same as with a physical Notecard
    note::Api api(nc);

    auto ver = api.card.version().execute();
    if (ver) {
        Serial.print("Version: ");
        Serial.println(ver.version);
        Serial.print("Device:  ");
        Serial.println(ver.device);
    } else {
        Serial.print("card.version failed: ");
        Serial.println(ver.error());
    }

    Serial.println("Ready.");
}

// ── Loop ───────────────────────────────────────────────────────────────

void loop() {
    delay(60000);
}

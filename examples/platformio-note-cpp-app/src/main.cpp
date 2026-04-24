// note-emu + note-cpp-app integration example
//
// Uses note-cpp-app's application framework with a virtual Notecard via the
// Blues softcard service. Demonstrates ConnectionManager, NotePublisher,
// and EnvVar/EnvGroup for cloud-configurable sensor publishing.
//
// After setup, enters a serial command loop for integration testing.
//
// Protocol:
//   Input:  raw Notecard JSON, one request per line
//   Output: "RSP: <json>" for responses, "ERR: <msg>" for errors
//   Marker: "READY" printed when the command loop starts

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <note/emu/arduino.hpp>
#include <note/emu/serial_hal.hpp>

#include <note/notecard.hpp>
#include <note/body.hpp>
#include <note/transport/serial.hpp>

#include <note_app/channel.hpp>
#include <note_app/connection_manager.hpp>
#include <note_app/note_publisher.hpp>
#include <note_app/env_var.hpp>
#include <note_app/env_group.hpp>
#include <note_app/state_store.hpp>

#include "cjson_backend.hpp"
#include "secrets.h"

// ── Sensor data struct ──────────────────────────────────────────────

struct Readings {
    float temperature;
    int16_t humidity;
    NOTE_FIELDS(temperature, humidity)
};

// ── Environment variables (cloud-configurable) ──────────────────────

static note_app::EnvVar publish_interval("publish_interval", int32_t{60});
static note_app::EnvVar location("location", std::string("room-1"));

// ── Globals ─────────────────────────────────────────────────────────

WiFiClientSecure wifiClient;
note::emu::Arduino softcard(NOTEHUB_PAT);
noteemu::CjsonBackend backend;

static note::Notecard *nc_ptr = nullptr;

// Serial line buffer for command input
static char line_buf[1024];
static size_t line_pos = 0;

// ── Command handler ─────────────────────────────────────────────────

static void handle_command(const char *line) {
    auto result = nc_ptr->transact(note::string_view(line), 10000);
    if (result) {
        Serial.printf("RSP: %s\n", result->c_str());
    } else {
        Serial.printf("ERR: %.*s\n",
            (int)result.error().message.size(),
            result.error().message.data());
    }
}

// ── Setup ───────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("note-emu + note-cpp-app example");

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

    // 2. Transport chain: note-emu → SerialHal → NotecardSerial → Notecard
    static note::emu::SerialHal hal(*softcard.instance(), millis, delay);
    using Transport = note::transport::NotecardSerial<>;
    static Transport transport(hal);
    static Transport *tp = &transport;

    static note::Notecard nc(backend,
        [](note::string_view req, uint32_t t) {
            return (*tp)(req, t);
        });
    nc_ptr = &nc;

    // 3. note-cpp-app channel wrapping the Notecard
    static note_app::DirectChannel channel(nc);

    // 4. ConnectionManager — configure hub
    static note_app::NullStateStore store;
    static note_app::ConnectionManager conn(channel, store,
        std::function<void(uint32_t)>([](uint32_t ms) { delay(ms); }));

    auto hub_result = conn.configure({
        .product  = PRODUCT_UID,
        .mode     = "periodic",
        .outbound = 60,
    });
    if (hub_result) {
        Serial.printf("hub.set OK (%s, periodic, outbound=60)\n", PRODUCT_UID);
    } else {
        Serial.printf("hub.set failed: %.*s\n",
            (int)hub_result.error().message.size(),
            hub_result.error().message.data());
    }

    // Query version
    auto ver = nc.request("card.version");
    if (ver) {
        Serial.println("card.version:");
        auto device = (*ver)->get_string("device");
        auto version = (*ver)->get_string("version");
        Serial.printf("  device:  %.*s\n", (int)device.size(), device.data());
        Serial.printf("  version: %.*s\n", (int)version.size(), version.data());
    }

    // 5. EnvGroup — load cloud-configurable variables
    static note_app::EnvGroup env(channel);
    env.add(publish_interval).add(location);
    env.on_change([] {
        Serial.printf("Config updated: interval=%d, location=%s\n",
            (int)*publish_interval,
            std::string(*location).c_str());
    });
    env.on_error([](const note_app::EnvError &e) {
        Serial.printf("Env error: %s — %s\n",
            e.var_name.c_str(), e.message.c_str());
    });

    Serial.println("Loading environment variables...");
    env.load();
    Serial.printf("  publish_interval = %d\n", (int)*publish_interval);
    Serial.printf("  location         = %s\n", std::string(*location).c_str());

    // 6. NotePublisher — publish a reading (template auto-registered)
    static note_app::NotePublisher pub(channel);

    Readings r{.temperature = 22.5f, .humidity = 60};
    Serial.printf("Publishing reading: temperature=%.1f, humidity=%d\n",
        r.temperature, r.humidity);
    auto add_result = pub.publish("sensors.qo", r);
    if (add_result) {
        Serial.println("  note.add OK");
    } else {
        Serial.printf("  note.add failed: %.*s\n",
            (int)add_result.error().message.size(),
            add_result.error().message.data());
    }

    Serial.println("READY");
}

// ── Loop ────────────────────────────────────────────────────────────

void loop() {
    if (!nc_ptr) return;

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

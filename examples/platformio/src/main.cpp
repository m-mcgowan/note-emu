// note-emu PlatformIO example — identical to the Arduino note-c example.
// See examples/arduino/note_c_example/note_c_example.ino for the full
// commented version. This file exists so PlatformIO finds a src/main.cpp.

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Notecard.h>
#include <note/emu/emu.h>

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

static int arduino_http_post(
    const char *url, const char *const *headers,
    const uint8_t *body, size_t body_len,
    uint8_t *resp_buf, size_t resp_buf_size, size_t *resp_len,
    int *http_status, void *ctx
) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(30000);

    for (const char *const *h = headers; h && *h; h++) {
        const char *colon = strchr(*h, ':');
        if (!colon) continue;
        String key = String(*h).substring(0, colon - *h);
        String val = String(colon + 2);
        http.addHeader(key, val);
    }

    int status;
    if (body && body_len > 0) {
        http.addHeader("Content-Type", "application/octet-stream");
        status = http.POST(const_cast<uint8_t *>(body), body_len);
    } else {
        status = http.POST("");
    }

    *http_status = status;
    *resp_len = 0;

    if (status > 0) {
        String payload = http.getString();
        size_t n = payload.length();
        if (n > resp_buf_size) n = resp_buf_size;
        memcpy(resp_buf, payload.c_str(), n);
        *resp_len = n;
    }

    http.end();
    return (status > 0) ? 0 : -1;
}

static uint32_t arduino_millis(void *ctx) { return millis(); }

static note_emu_t *emu = NULL;

void setup() {
    Serial.begin(115200);
    delay(2000);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    Serial.printf("Connected: %s\n", WiFi.localIP().toString().c_str());

    note_emu_config_t config = {
        .http_post = arduino_http_post,
        .millis    = arduino_millis,
        .ctx       = NULL,
        .api_token = NOTEHUB_PAT,
    };

    note_emu_err_t err = note_emu_create(&config, &emu);
    if (err != NOTE_EMU_OK) {
        Serial.printf("ERROR: %s\n", note_emu_strerror(err));
        while (1) delay(1000);
    }
    note_emu_set_global(emu);
    NoteSetFnSerial(
        note_emu_serial_reset,
        note_emu_serial_transmit,
        note_emu_serial_available,
        note_emu_serial_receive
    );

    J *req = NoteNewRequest("hub.set");
    JAddStringToObject(req, "product", NOTEHUB_PRODUCT);
    JAddStringToObject(req, "mode", "periodic");
    NoteRequest(req);
}

void loop() {
    J *req = NoteNewRequest("card.temp");
    J *rsp = NoteRequestResponse(req);
    if (rsp) {
        double temp = JGetNumber(rsp, "value");
        Serial.printf("Softcard temp: %.2f C\n", temp);
        NoteDeleteResponse(rsp);
    }
    delay(10000);
}

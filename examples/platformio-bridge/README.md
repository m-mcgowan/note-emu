# platformio-bridge — note-c + note-cpp coexistence with note-emu

Demonstrates note-cpp's [bridge mode](https://github.com/m-mcgowan/note-cpp/blob/main/docs/platforms/host/migration-from-note-c.md#bridge-mode-incremental-migration) against a note-emu virtual Notecard: note-c owns the transport, note-cpp typed calls route through `NoteRequestResponseJSON()`. Both APIs work against the same virtual Notecard in one sketch.

Use this pattern when:

- You have existing note-c code and want to adopt note-cpp incrementally without rewriting the transport setup.
- You want the raw `NoteRequestResponse`-style API available alongside the typed `note::Api` in the same sketch.
- You're migrating a project between the two libraries and need both to work during the transition.

For pure note-cpp (streaming mode, no bridge, no note-c), see [`../platformio-notecpp/`](../platformio-notecpp/) instead.

## Prerequisites

1. ESP32-S3 board (tested with ESP32-S3-DevKitM-1)
2. PlatformIO installed
3. A [Notehub](https://notehub.io) account with a Personal Access Token

Both `note-arduino` (which pulls in note-c + cJSON) and `note-cpp` are resolved from `platformio.ini`'s `lib_deps` — no manual setup.

## Setup

Copy the secrets template and fill in your values:

```sh
cp src/secrets.h.example src/secrets.h
```

<!-- snippet:examples/platformio-bridge/src/secrets.h.example:4-6 -->
```cpp
#define WIFI_SSID "your-ssid"
#define WIFI_PASS "your-password"
#define NOTEHUB_PAT "your-notehub-api-token"
```

## Build and flash

```sh
pio run -t upload
pio device monitor
```

## What to look for in the output

Both API calls succeed in the same sketch, hitting the same virtual Notecard. Captured from a real Wokwi run (account UID redacted):

<!-- snippet:examples/platformio-bridge/sample-output.txt:1-19 -->
```text
note-emu bridge example (note-c + note-cpp)
....
Connected: 10.13.37.2
note-emu: connecting to https://softcard.blues.com
note-emu: resolving account UID from PAT via billing-accounts API
note-emu: billing-accounts -> rc=0 http=200 [396 ms]
note-emu: resolved account UID: 00000000-0000-0000-0000-000000000000
note-emu: ready (uid=00000000-0000-0000-0000-000000000000)
  > {"req":"hub.set","product":"com.example.you:bridge-demo","mode":"continuous"}
note-emu: POST /v1/write (101 bytes) -> rc=0 http=200 [457 ms]
note-emu: POST /v1/read -> rc=0 http=200 bytes=26 [62 ms]
  < {}
hub.set (note-c): OK
  > {"req":"card.version","id":1}
note-emu: POST /v1/write (31 bytes) -> rc=0 http=200 [132 ms]
note-emu: POST /v1/read -> rc=0 http=200 bytes=369 [72 ms]
  < {"id":1,"version":"notecard-11.1.1.1301","device":"dev:soft:00000000-0000-0000-0000-000000000000","name":"Blues Wireless Notecard","sku":"NOTE-SOFTCARD","board":"0.0","cell":true,"body":{"org":"Blues Wireless","product":"Notecard","target":"ux","version":"notecard-ux-11.1.1","ver_major":11,"ver_minor":1,"ver_patch":1,"ver_build":1301,"built":"Dec 8 2025 10:13:37"}}
card.version (note-cpp): notecard-11.1.1.1301
READY
```

## Running in Wokwi

The example ships with a `wokwi.toml` + `diagram.json`, so it also builds and runs under the Wokwi simulator (browser or VS Code extension):

```sh
pio run -e wokwi
# then either:
#   - open this folder in VS Code with the Wokwi extension
#   - or, headless: WOKWI_CLI_TOKEN=... wokwi-cli --expect-text "READY" .
```

## Switching to physical hardware

The same bridge pattern applies when moving from note-emu to a real Notecard. See [`docs/migrating-to-physical-notecard.md`](../../docs/migrating-to-physical-notecard.md) for the transport swap and note-cpp's [bridge-mode documentation](https://github.com/m-mcgowan/note-cpp/blob/main/docs/platforms/host/migration-from-note-c.md#bridge-mode-incremental-migration) for the underlying `NoteCTransport` design.

## Key files

| File | Purpose |
|------|---------|
| `src/main.cpp` | Sketch — installs note-c + note-cpp on the same softcard, exercises both APIs |
| `src/secrets.h.example` | WiFi + Notehub credentials template |
| `platformio.ini` | Build configuration (note-arduino + note-cpp + note-emu) |

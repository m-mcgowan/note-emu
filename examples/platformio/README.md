# PlatformIO Example (note-c)

PlatformIO version of the `arduino/note_c_example` sketch. Uses the note-c API with an inline HTTP transport. Same functionality, adapted for PlatformIO's build system.

For new projects, consider using the [platformio-notecard](../platformio-notecard/) example instead, which provides a cleaner integration via the type-safe `NoteEmuArduino` class and `NoteSerial_Softcard` bridge.

## Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or IDE plugin)
- An ESP32-S3 development board (or edit `platformio.ini` for your board)
- WiFi network access
- A Notehub Personal Access Token (PAT)

## Setup

Copy the secrets template and fill in your values:

```sh
cp src/secrets.h.example src/secrets.h
```

Edit `src/secrets.h`:

<!-- snippet:examples/platformio/src/secrets.h.example:4-7 -->
```cpp
#define WIFI_SSID "your-ssid"
#define WIFI_PASS "your-password"
#define NOTEHUB_PAT "your-notehub-api-token"
#define NOTEHUB_PRODUCT "com.example.softcard"
```

## Build and flash

```sh
pio run -t upload
```

## Monitor serial output

```sh
pio device monitor
```

## Expected output

```
Connected: 192.168.1.42
Softcard temp: 23.00 C
Softcard temp: 23.00 C
...
```

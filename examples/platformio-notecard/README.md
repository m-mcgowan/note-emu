# PlatformIO + note-arduino Example

Uses the standard **note-arduino** `Notecard` API with a virtual Notecard via the Blues softcard service. No physical Notecard needed — just WiFi and a Notehub account.

This example demonstrates the recommended integration pattern: `NoteEmuArduino` provides a type-safe HTTP backend (taking any `NetworkClient`), and `NoteSerial_Softcard` bridges note-emu to note-arduino's `NoteSerial` interface.

## Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or IDE plugin)
- An ESP32-S3 development board (or edit `platformio.ini` for your board)
- WiFi network access
- A Notehub Personal Access Token (PAT) — generate one at https://notehub.io under **Account Settings > API Tokens**
- A Notehub project with a product UID (e.g. `com.your-company:your-product`)

## Setup

1. Copy the secrets template and fill in your values:

   ```sh
   cp src/secrets.h.example src/secrets.h
   ```

2. Edit `src/secrets.h` with your credentials:

   ```c
   #define WIFI_SSID "your-ssid"
   #define WIFI_PASS "your-password"
   #define NOTEHUB_PAT "your-notehub-api-token"
   #define NOTEHUB_PRODUCT "com.your-company:your-product"
   ```

   `secrets.h` is gitignored and will not be committed.

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
note-emu + note-arduino example
WiFi.... connected: 192.168.1.42
Softcard version: notecard-softcard-1.0.0
Device: dev:soft:a8f1385c-...
Softcard temp: 23.00 C
Softcard temp: 23.00 C
...
```

On startup, the example:

1. Connects to WiFi
2. Creates a note-emu instance (resolves your account UID from the PAT)
3. Bridges note-emu to note-arduino via `NoteSerial_Softcard`
4. Sends `hub.set` to assign the softcard to your project
5. Queries `card.version` to verify connectivity
6. Reads `card.temp` every 15 seconds in the main loop

The softcard device will appear in your Notehub project's device list.

## Adapting for other boards

Edit `platformio.ini` to change the board and platform. The `NoteEmuArduino` class accepts any `NetworkClient` subclass, so it works with `WiFiClientSecure`, `EthernetClient`, or any other Arduino-compatible network client.

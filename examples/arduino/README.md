# Arduino IDE Examples

Arduino IDE sketches that use note-emu's low-level C serial HAL directly, without the `NoteEmuArduino` wrapper class.

For new projects, consider using the [platformio-notecard](../platformio-notecard/) example instead, which provides a cleaner integration via the type-safe `NoteEmuArduino` class.

## Prerequisites

- Arduino IDE with ESP32 board support
- The following libraries installed via Library Manager:
  - **Blues Wireless Notecard** (note-arduino)
  - **note-emu** (install from this repository)
- WiFi network access
- A Notehub Personal Access Token (PAT)

## Examples

### note_c_example

Uses the **note-c** API (`NoteNewRequest`, `NoteRequest`, `NoteRequestResponse`). The HTTP transport is wired up inline using Arduino's `HTTPClient`. On startup it sends `hub.set` and then reads `card.temp` every 10 seconds.

### note_cpp_example

Uses the **note-cpp** typed API. Sets up the transport via `noteemu::SoftcardSerialHal` and `note::transport::NotecardSerial`. This is a skeleton — note-cpp requires a JSON backend implementation for full functionality.

## Setup

1. Edit the `#define` values at the top of the sketch:

   ```c
   #define WIFI_SSID "your-ssid"
   #define WIFI_PASS "your-password"
   #define NOTEHUB_PAT "your-notehub-api-token"
   #define NOTEHUB_PRODUCT "com.your-company:your-product"
   ```

   Or pass them as build flags (`-DWIFI_SSID=\"...\"`) to keep credentials out of source.

2. Select your ESP32 board in the Arduino IDE.

3. Upload and open the Serial Monitor at 115200 baud.

## Expected output (note_c_example)

```
note-emu: Arduino + note-c example
Connecting to WiFi....
Connected: 192.168.1.42
note-c configured via softcard!
Virtual Notecard temperature: 23.00 C
Virtual Notecard temperature: 23.00 C
...
```

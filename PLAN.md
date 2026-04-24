# note-emu: Virtual Notecard over HTTP

## Goal

Use Blues' cloud-hosted virtual Notecard ("softcard") over HTTP instead of a physical Notecard over I2C/UART. This enables prototyping without hardware in two contexts:

1. **ESP32 over WiFi** — firmware talks to the softcard service directly, replacing the local Notecard serial transport with HTTP.
2. **Cloud dev environments** — Codespaces, Gitpod, or any online IDE can run note-c based code against a virtual Notecard with no USB, no drivers, no physical hardware at all. The softcard service is pure HTTP, so it works from anywhere with internet access.

## How the Blues Simulator Works

The in-browser terminal at dev.blues.io communicates with a cloud-hosted virtual Notecard via two HTTP endpoints. The protocol emulates a raw serial port over HTTP.

### Endpoints

| Endpoint | Method | Purpose |
|---|---|---|
| `https://softcard.blues.com/v1/write` | POST | Send bytes to the virtual Notecard (request body = raw data) |
| `https://softcard.blues.com/v1/read` | POST | Long-poll read bytes from the virtual Notecard (blocks until data available) |

### Authentication

Currently, the softcard service requires only a single header:

```
X-User-UID: <notehub-account-uid>
```

This is a routing key that identifies which virtual Notecard instance to talk to. It is the Notehub account UID (e.g. `user:abc123`), visible at `https://notehub.io/account-settings`. No session cookies or bearer tokens are validated today.

However, **the implementation should assume authentication will be required in future**. The Notehub API at `https://api.notefile.net/auth/login` already accepts username/password credentials and returns session tokens. The softcard transport should:
- Support authenticating to Notehub with provided credentials
- Store and send session cookies alongside `X-User-UID`
- Handle 401 responses with re-authentication

CORS restricts browser access to `https://dev.blues.io`, but direct HTTP requests from firmware are not subject to CORS.

### Protocol

The Notecard protocol is JSON-line based over a serial-like byte stream:

1. **Write a request**: POST the JSON request string (newline-terminated) to `/v1/write`
2. **Read the response**: POST to `/v1/read` — blocks until the virtual Notecard has output, returns the response bytes

The browser's terminal code adds an `id` field to each request for response correlation:

```json
{"req":"card.version","id":2500000001}
```

The Notecard echoes the `id` back in the response. However, the browser also uses a transaction lock (one request at a time), so the `id` is mainly a safety check. For a serial-port emulation, sequential request/response is sufficient — no need for multiplexing.

### Instance Lifecycle

Unknown: whether the softcard service auto-provisions an instance when a new UID connects, or whether the simulator must first be started from the browser. A 504 timeout was observed when using a random UID, suggesting the instance may need to be started from the browser UI first. Needs investigation.

## Implementation Approach

Replace note-c's serial transport with an HTTP transport that talks to the softcard service. This is a compile-time swap — the application code using note-c doesn't change.

### note-c Serial Transport

note-c uses a pluggable serial interface with these callbacks:
- `noteSerialReset()` — open/reset the port
- `noteSerialTransmit(data, len)` — write bytes
- `noteSerialAvailable()` — check if bytes are available to read
- `noteSerialReceive()` — read one byte

The softcard transport would implement these as:
- `noteSerialReset()` — no-op or re-establish HTTP client
- `noteSerialTransmit()` — POST to `/v1/write`
- `noteSerialAvailable()` / `noteSerialReceive()` — POST to `/v1/read`, buffer the response, return bytes from buffer

### ESP32 Firmware

Configuration:
- WiFi credentials (already standard for WiFi Notecard projects)
- Notehub account UID (build flag or `#define`)
- Notehub username/password (for authentication, build flags or runtime config)
- Softcard service URL (default `https://softcard.blues.com`)

Considerations:
- **TLS required**: ESP32 needs a TLS-capable HTTP client (ESP-IDF's `esp_http_client` or Arduino's `HTTPClient` with WiFiClientSecure).
- **Long-poll blocking**: `/v1/read` blocks until data is available. This maps naturally to note-c's serial read, which also blocks. May need a timeout to avoid indefinite hangs.
- **Latency**: Each Notecard transaction becomes two HTTPS round-trips instead of local I2C/UART. Acceptable for prototyping, not for production.
- **No physical Notecard dependencies**: No I2C/UART pin assignments, no Notecard power management, no antenna considerations.

### Wokwi Simulation

Wokwi (wokwi.com) simulates ESP32 with full peripheral and WiFi support. Using its Custom Chips C API, a virtual Notecard I2C peripheral can be built that proxies requests to the softcard backend. This gives firmware a fully transparent simulation — it talks I2C to what it thinks is a real Notecard.

Two possible architectures:

1. **Custom I2C chip → softcard over simulated WiFi**: The Wokwi custom chip (WASM) implements the Notecard I2C protocol. When the firmware does an I2C transaction, the chip forwards it via HTTP to `softcard.blues.com` over Wokwi's simulated WiFi. Firmware code is completely unmodified.

2. **note-c HTTP transport in firmware**: Skip the I2C emulation entirely. The firmware uses the softcard HTTP transport directly (as in the ESP32 approach above), and Wokwi just provides the simulated WiFi. Simpler, but the firmware knows it's not talking to a real Notecard.

Wokwi also has a VS Code extension for local simulation and can run in Codespaces.

### Cloud Dev Environments (Codespaces, Gitpod)

The same HTTP transport works from any environment — no ESP32 needed. Wokwi in Codespaces provides the full experience: simulated ESP32 + virtual Notecard, entirely in the browser.

Use cases:
- **CI/CD testing**: Run Notecard integration tests in GitHub Actions without hardware.
- **Interactive tutorials**: Codespace-based Blues workshops where every participant gets a virtual Notecard — no hardware kits to ship.
- **Development iteration**: Write and test Notehub routing, JSONata transforms, and cloud-side logic without waiting for physical device syncs.

## Open Questions

1. Does the softcard instance auto-provision on first connect, or must it be started from the browser?
2. What is the Notecard protocol's flow control — does the browser's `card.binary` check / chunk sizing matter for HTTP transport?
3. Can multiple clients connect to the same UID simultaneously (browser + ESP32)?
4. Is there a session timeout / idle expiry for softcard instances?

## Project Structure

```
note-emu/
├── PLAN.md
├── src/
│   ├── note_emu.h              # C API — config, create/destroy, serial hooks, direct read/write
│   ├── note_emu.c              # Core implementation — HTTP transport, TX/RX buffering, auth
│   └── note_emu_serial_hal.hpp # C++ adapter — implements note-cpp SerialHal
├── examples/
│   ├── arduino/
│   │   ├── note_c_example/     # Arduino sketch using note-c + NoteSetFnSerial()
│   │   └── note_cpp_example/   # Arduino sketch using note-cpp + SerialHal
│   └── platformio/
│       ├── platformio.ini      # ESP32-S3 PIO project
│       └── src/main.cpp        # PIO example using note-c
├── examples/native/
│   ├── main.c              # Native demo/test — libcurl backend, env var config
│   └── Makefile
└── tests/
    ├── test_softcard.py    # Python integration verifier — checks Notehub event stream
    └── run.sh              # Shell wrapper for env sourcing
```

### Integration paths

1. **note-c**: Call `note_emu_set_global(emu)` then `NoteSetFnSerial(note_emu_serial_reset, ...)`. Application code is unchanged.

2. **note-cpp**: Create `noteemu::SoftcardSerialHal` wrapping a `note_emu_t` instance. Pass to `note::transport::NotecardSerial`. Application code is unchanged.

Both paths require the platform to supply an HTTP POST function (Arduino `HTTPClient`, ESP-IDF `esp_http_client`, libcurl, etc.) and a millis clock.

## Testing

### Test 1: Basic (no project)

The native demo sends `card.version`, `card.temp`, `card.status` and verifies responses. No Notehub project needed — tests local Notecard queries only.

```bash
set -a; source ~/.notehub-blues-mat; set +a
make -C examples/native run
```

### Test 2: Project + note.add (end-to-end)

The native demo sends `hub.set` (assigns project), then `note.add` with a unique test marker, then Python verifies the event appears in Notehub's event stream via the REST API.

```bash
set -a; source ~/.notehub-blues-mat; set +a
export NOTEHUB_PRODUCT_UID="com.example.softcard"
python3 tests/test_softcard.py
```

The Python verifier:
1. Builds and runs the C demo in project mode
2. Parses machine-readable output for device UID and test_id
3. Polls `GET /v1/projects/{productUID}/events?files=test.qo` with bearer auth
4. Confirms the event body contains the matching test_id

Stdlib-only Python — no pip dependencies.

### Future: embedded-bridge integration

These tests will eventually be orchestrated by the test runner infrastructure in [embedded-bridge](https://github.com/m-mcgowan/embedded-bridge), which provides a unified framework for running integration tests across hardware and emulated targets.

## Protocol source

The softcard service is not publicly documented. Protocol details above were captured by inspecting the in-browser terminal at dev.blues.io.

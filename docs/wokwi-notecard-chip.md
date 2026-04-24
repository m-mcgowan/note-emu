# Wokwi Notecard Chip — Design Notes

## Goal

Replace the canned-response mock Notecard chip in Wokwi simulations with a
real chip that proxies to the Blues softcard simulator (`softcard.blues.com`).
This enables full end-to-end simulated testing: firmware runs on simulated AVR
or ESP32, talks UART/I2C to a custom chip, which forwards JSON requests to
softcard and returns real responses.

The mock chip (`note-cpp/examples/binary-size-comparison/chips/notecard-mock.c`)
has its place for fast, deterministic binary-size testing. This chip would serve
a different purpose: integration/acceptance testing against the real Notecard API.

## Current Mock Chip (reference)

- Compiled to WASM via `wokwi-cli chip compile`
- Exposes 4 pins: VCC, GND, TX, RX (UART at 9600 baud)
- Handles reset handshake (`\n` → `\r\n`)
- Pattern-matches JSON requests (`card.temp`, `note.template`, `note.get`) → canned responses
- Everything else → `{}\r\n`
- ~150 lines of C, uses only Wokwi UART API

## Architecture Options

### Option A: WASM chip with WASI networking

**Idea:** Compile `note_emu.c` + an HTTP backend directly into the WASM chip.
The chip receives JSON over UART from the simulated MCU, forwards it to
softcard via HTTP, and returns the response over UART.

**Open question:** Does the Wokwi WASM runtime support WASI sockets or
`wasi:http`? This hasn't been tested. WASI preview1 has `sock_*` syscalls;
preview2 has `wasi:sockets` and `wasi:http`. Support depends on the runtime.

**To test:**
```c
// Minimal probe — compile and run in Wokwi
#include <sys/socket.h>
#include <netdb.h>
// Try to resolve softcard.blues.com and open a TCP connection
```
Or simpler: try `#include <stdio.h>` with `fopen()` to see what WASI file I/O
is available, then escalate to sockets.

**If it works:** This is the simplest path. The chip would be:
- `note_emu.c` (core library, platform-agnostic)
- A thin HTTP backend using WASI sockets or `wasi:http`
- UART glue using the Wokwi chip API (same as mock chip)

**HTTP backend for WASI:** Would need a new `note_emu_wasi.c` backend since
neither libcurl nor Arduino HTTPClient are available in WASM. Could be a
minimal HTTP/1.1 client over raw TCP sockets (softcard only needs POST to
two endpoints: `/v1/write` and `/v1/read`).

### Option B: Docker sidecar + serial forwarding

**Idea:** Run note-emu as a native process inside the `wokwi-ci-server` Docker
container. The WASM chip communicates with the sidecar through some IPC
mechanism, and the sidecar handles HTTP to softcard.

**Architecture:**
```
Simulated MCU (AVR/ESP32)
    ↕ UART (Wokwi simulation)
WASM custom chip
    ↕ IPC (file, pipe, socket?)
note-emu sidecar (native process in Docker)
    ↕ HTTPS
softcard.blues.com
```

**IPC options:**
- **WASI file I/O:** Chip writes request to a file, sidecar watches it, writes
  response to another file. Polling-based, higher latency. Most likely to work.
- **WASI sockets (localhost):** Chip connects to `localhost:PORT` where sidecar
  listens. Clean but depends on socket support.
- **Shared memory:** Not available in standard WASI.

**Docker image:** Build a custom image based on `wokwi/wokwi-ci-server` that
also includes the note-emu native binary. Or use docker-compose with two
containers sharing a volume.

**Advantages:** Works even if WASM has no networking. note-emu is already built
and tested as a native binary.

**Disadvantages:** More moving parts. IPC latency. Custom Docker image to maintain.

### Option C: ESP32 + WiFi (no custom chip)

**Idea:** Use Wokwi's ESP32 WiFi simulation. The firmware itself uses note-emu's
HTTP transport to reach softcard directly — no custom Notecard chip needed.

**Architecture:**
```
Firmware (ESP32, WiFi enabled)
    ↕ note-emu HTTP transport
    ↕ Wokwi simulated WiFi
softcard.blues.com
```

**Advantages:** No custom chip complexity. Uses note-emu's existing Arduino
HTTP backend (`note_emu_arduino.cpp`). Already works on real ESP32 hardware.

**Disadvantages:** Only works on ESP32 (not AVR). Firmware must be note-emu
aware (can't test with standard UART/I2C Notecard code). Wokwi WiFi
simulation may have limitations.

**Good for:** Testing note-emu itself, testing note-cpp-app integration on ESP32.
Not suitable for AVR binary-size validation.

## Recommended Exploration Order

1. **Try Option A first** — probe WASI networking in a Wokwi WASM chip. If it
   works, this is the cleanest path with the least infrastructure.

2. **Fall back to Option B** if WASM networking is blocked. The Docker sidecar
   approach is proven architecture (just more plumbing).

3. **Option C in parallel** — independent of A/B, useful for ESP32 integration
   testing. Could be a quick win since note-emu already has the Arduino backend.

## Implementation Scope (for note-emu project)

The Wokwi Notecard chip should live in note-emu, not note-cpp, so it's
reusable across projects:

```
note-emu/
  wokwi/
    notecard-chip.c          # Wokwi custom chip: UART ↔ note-emu
    notecard-chip.chip.json  # Chip metadata
    Makefile                 # Build with wokwi-cli chip compile
    README.md                # Usage instructions
```

Projects (note-cpp, note-arduino, etc.) would reference the compiled
`notecard-chip.wasm` from note-emu, or note-emu could publish it as a release
artifact.

## note-emu API Surface (for chip integration)

The chip would use note-emu's core C API:

```c
note_emu_config_t config = {
    .http_post = wasi_http_post,  // or sidecar_ipc_post
    .millis = wasi_millis,
    .api_token = "...",           // from chip attribute or env var
};
note_emu_t *emu = note_emu_create(&config);

// In UART rx callback: accumulate bytes, on \n:
note_emu_write(emu, request_buf, request_len);

// Poll for response (in timer callback):
int n = note_emu_read(emu, response_buf, sizeof(response_buf));
if (n > 0) uart_write(dev, response_buf, n);
```

## AVR Binary Size Context (2026-04-09)

For reference, the current AVR binary comparison that uses the mock chip:

|            | Flash        | Static RAM   | Heap  | Total RAM    |
|------------|-------------|-------------|-------|-------------|
| note-c     | 25,076 (78%) | 729 (36%)   | ~371B | 1,100 (54%) |
| note-cpp   | 28,822 (89%) | 832 (41%)   | 0     | 832 (41%)   |

The mock chip is sufficient for binary-size testing (deterministic, fast, no
network dependency). The softcard-backed chip targets a different use case:
verifying that firmware correctly interacts with the full Notecard API.

## Spike Results (2026-04-09)

### WASI Networking: Blocked

Probed WASI capabilities in Wokwi custom chips (see `wokwi/spike/`):

- **Probe 1 (stdio): PASS** — printf/stderr work from chip_init()
- **Probe 2 (DNS): FAIL** — `netdb.h` not found. WASI Preview 1 has no socket API.
  Even with manual wasip2 compilation (headers exist), the Wokwi runtime only
  provides hardware-sim imports (pin, UART, I2C, SPI, timer). No networking.

**Root causes (three independent blockers):**
1. WASI Preview 1 was sealed without networking
2. Wokwi runtime exposes only hardware-simulation WASM imports
3. WiFi is implemented at ESP32 hardware register level — not accessible to chips

**Multi-MCU: Not supported.** Wokwi supports one MCU per simulation
([feature #186](https://github.com/wokwi/wokwi-features/issues/186), open since 2021).
This rules out a custom chip + ESP32 relay architecture.

### ESP32 + WiFi + Softcard: Working

Proved the full stack works (see `wokwi/esp32-softcard/`):

```
ESP32-S3 in Wokwi → WiFi (Wokwi-GUEST) → HTTPS → softcard.blues.com → HTTP 200
```

- ESP32-S3 serial output via UART0 works (not USB CDC — Wokwi captures UART0)
- WiFi connects to `Wokwi-GUEST` (open network, private gateway IP `10.13.37.2`)
- HTTPS to softcard.blues.com succeeds with `WiFiClientSecure` + `setInsecure()`
- Tested via VS Code extension (community/open-source license, no CI minute quota)

### Wokwi CI Server (Docker)

The `wokwi/wokwi-ci-server` Docker image runs simulations locally but **still
enforces cloud quota** via token validation. It does NOT provide unmetered
simulation. The VS Code extension uses a separate licensing model (license key,
not CI token) and appears unmetered for interactive use.

### Architecture Decision

**Option A (WASM + WASI networking): Dead.** Three independent blockers.

**Option B (Docker sidecar): Dead.** No IPC path — chips can't do file I/O or
sockets either. Multi-MCU not supported.

**Option C (ESP32 + WiFi): Viable and proven.** The ESP32 runs note-emu directly
with the Arduino HTTP backend. No custom chip needed for softcard integration.

This means:
- **For real softcard responses:** Run note-emu on ESP32 in Wokwi with WiFi.
  The firmware uses note-emu's NoteEmuArduino backend directly.
- **For Notecard protocol testing (UART/I2C):** The existing mock chip in
  note-cpp continues to serve this purpose with canned responses.
- **These are separate concerns** and don't need to be unified into one chip.

### Wokwi Chip Distribution

Wokwi has no chip registry. Custom chips are distributed as `.chip.wokwi`
bundles via GitHub releases, referenced by URL in `wokwi.toml`.

## Status

- [x] Probe WASI networking support in Wokwi WASM runtime — **blocked**
- [x] Prove ESP32 + WiFi + softcard works in Wokwi — **working**
- [x] Wire up NoteEmuArduino in the Wokwi example (real Notecard API call)
- [x] Test with note-cpp integration (streaming transport via note::emu::SerialHal)
- [x] Profiling + real hardware benchmark (Wokwi @240MHz ≈ real ESP32 ≈ 230ms/req)
- [ ] Set up as CI-runnable test (requires paid Wokwi plan or VS Code automation)

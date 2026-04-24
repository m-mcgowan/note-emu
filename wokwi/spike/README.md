# WASI Socket Probe — Results

**Date:** 2026-04-09
**Verdict:** WASI networking is **blocked** in Wokwi custom chips. Pivot to ESP32 + WiFi.
See `wokwi/esp32-softcard/` for the working alternative (ESP32 + WiFi + softcard confirmed).

## Probe Results

### Probe 1: WASI stdio — PASS

```
[chip-probe-chip] [probe-1] WASI stdio works
[chip-probe-chip] [probe-1] stderr works
```

- `printf` and `fprintf(stderr, ...)` work from `chip_init()`
- Output appears prefixed with `[chip-<name>]` in simulation output
- Compilation pipeline works: `wokwi-cli chip compile` bundles WASI-SDK v25.0
  (clang 19.1.5, wasm32-unknown-wasi target)

### Probe 2: DNS resolution — FAIL (compile time)

```
probe2-dns.c:3:10: fatal error: 'netdb.h' file not found
```

**Root cause:** `wokwi-cli chip compile` targets `wasm32-unknown-wasi` (WASI Preview 1).
WASI Preview 1 was sealed without networking support — no `netdb.h`, no socket
syscalls, no DNS.

**Further investigation:** WASI-SDK v25.0 *does* include networking headers under
the `wasm32-wasip2` sysroot (`netdb.h`, `sys/socket.h`). Manual compilation with
`--target=wasm32-wasip2` succeeds. However, this is moot because the **Wokwi
runtime does not provide networking imports**. The only host-provided WASM imports
are hardware simulation APIs:

- Pin I/O (`pinInit`, `pinRead`, `pinWrite`, `pinWatch`, etc.)
- UART (`uartInit`, `uartWrite`)
- I2C (`i2cInit`)
- SPI (`spiInit`, `spiStart`, `spiStop`)
- Timers (`timerInit`, `timerStart`)
- Framebuffer (`framebufferInit`, `bufferRead`, `bufferWrite`)
- Attributes (`attrInit`, `attrRead`)
- Time (`getSimNanos`)
- Debug (`debugPrint`)

No `sock_*`, `getaddrinfo`, or any network-related imports exist.

### Probes 3-4: Skipped

Blocked by Probe 2 failure. No path to TCP or TLS from a custom chip.

## Why Networking Can't Work

1. **WASI Preview 1 spec** has no socket API (sealed without it)
2. **Wokwi runtime** only exposes hardware-simulation imports to WASM chips
3. **Zero precedent** — no community examples or feature requests for chip networking
4. **ESP32 WiFi** is a separate mechanism: Wokwi simulates a full TCP/UDP stack
   for ESP32 firmware internally, but this is not exposed to custom chip WASM

## Viable Alternatives

Per the decision tree in the [spike design](../../docs/specs/2026-04-09-wasi-socket-probe-design.md):

### Option B: UART bridge (custom chip + MCU networking)

The custom chip communicates with the MCU firmware over UART/I2C (same as a real
Notecard). The MCU firmware (ESP32 with WiFi) makes the HTTP calls to
softcard.blues.com. The custom chip handles the Notecard serial protocol only.

**Pros:** Works within Wokwi's architecture. ESP32 WiFi networking is proven.
**Cons:** More complex — two codebases (chip WASM + ESP32 firmware).

### Option C: ESP32 firmware only (no custom chip)

Skip the custom chip entirely. Build an ESP32 firmware that emulates a Notecard
over I2C/UART, with note-emu handling the HTTP backend via ESP32 WiFi.

**Pros:** Simplest architecture. Everything in one firmware. note-emu already has
an Arduino/ESP backend.
**Cons:** The "Notecard" is an ESP32 running firmware, not a drop-in chip part.

### Chip registry note

Wokwi has no centralized chip registry. Custom chips are distributed as
`.chip.wokwi` bundles via GitHub releases, referenced by URL in `wokwi.toml`.

## Reproduction

```bash
cd wokwi/spike
./run-probe.sh probe1-stdio.c   # PASS
./run-probe.sh probe2-dns.c     # FAIL at compile time
```

Requires: Docker (wokwi-ci-server), wokwi-cli (~/.wokwi/bin/wokwi-cli),
Wokwi CLI token (note-cpp/.wokwi or local .wokwi file).

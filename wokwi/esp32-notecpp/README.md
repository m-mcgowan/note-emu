# esp32-notecpp — note-emu on Wokwi (note-cpp)

[![Simulate in Wokwi](https://img.shields.io/badge/Simulate-Wokwi-AAB42F?logo=espressif)](https://wokwi.com/projects/469739119860047873)

A simulated ESP32-S3 that talks to the **real** Blues softcard service over Wokwi's
virtual WiFi — no physical Notecard, no hardware at all. This is the **note-cpp** sibling
of [`esp32-notec/`](../esp32-notec/), using the type-safe C++23 note-cpp API on top of
note-emu's HTTP transport via `note::emu::SerialHal` and `note::link::SerialFramer`.

## Run it locally (the iteration loop)

```sh
cp src/secrets.h.example src/secrets.h          # then add your Notehub PAT
pio run -e wokwi                                # build the firmware (pulls note-cpp from GitHub)
```

Then either:

- **Wokwi VS Code extension** — open this folder, the extension reads `wokwi.toml`
  (which points at `.pio/build/wokwi/firmware.bin`) and runs the sim in-editor.
  Works in github.dev / Codespaces too.
- **`wokwi-cli`** — headless, scriptable:
  ```sh
  wokwi-cli --timeout 30000 --expect-text "READY" .
  ```
  Set `WOKWI_CLI_SERVER=ws://localhost:9177` and run a local Docker
  `wokwi-ci-server` to avoid burning cloud minutes.

On boot the firmware connects to WiFi, opens a softcard session, runs five `card.version`
round-trips as a latency profile via the typed `api.card.version()` call, then prints
`READY`.

> **WiFi can be slow or flaky in the browser.** The web simulator joins `Wokwi-GUEST`
> through Wokwi's free, shared public gateway, which is capacity-limited. If a run hangs
> on WiFi, restart the simulation. For reliable runs, use the VS Code Wokwi extension or
> real hardware.

> **First request is slow (~30 s).** softcard provisions your virtual Notecard instance on
> first contact; the initial `/v1/read` long-poll can exceed the 30 s read timeout and
> retry once. Subsequent requests are ~250 ms.

> **Tip:** `diagram.json` pins `"cpuFrequency": "240"`. Wokwi's default 8 MHz makes
> the TLS handshake ~30× slower.

## Example output

When the example runs, you should see output like this:

<!-- snippet:wokwi/esp32-notecpp/sample-output.txt:1-33 -->
```text
note-emu + note-cpp on Wokwi
WiFi...... connected: 10.13.37.2
note-emu: connecting to https://softcard.blues.com
note-emu: resolving account UID from PAT via billing-accounts API
note-emu: billing-accounts -> rc=0 http=200 [380 ms]
note-emu: resolved account UID: 00000000-0000-0000-0000-000000000000
note-emu: ready (uid=00000000-0000-0000-0000-000000000000)
=== iteration 1/5 ===
note-emu: POST /v1/write (53 bytes) -> rc=0 http=200 [456 ms]
note-emu: POST /v1/read -> rc=0 http=200 bytes=391 [62 ms]
PROFILE req=card.version total=1268 reset=0 tx=0 rx=0 parse=0
  version = notecard-11.1.1.1301
=== iteration 2/5 ===
note-emu: POST /v1/write (53 bytes) -> rc=0 http=200 [122 ms]
note-emu: POST /v1/read -> rc=0 http=200 bytes=391 [62 ms]
PROFILE req=card.version total=184 reset=0 tx=0 rx=0 parse=0
  version = notecard-11.1.1.1301
=== iteration 3/5 ===
note-emu: POST /v1/write (53 bytes) -> rc=0 http=200 [132 ms]
note-emu: POST /v1/read -> rc=0 http=200 bytes=391 [62 ms]
PROFILE req=card.version total=194 reset=0 tx=0 rx=0 parse=0
  version = notecard-11.1.1.1301
=== iteration 4/5 ===
note-emu: POST /v1/write (53 bytes) -> rc=0 http=200 [102 ms]
note-emu: POST /v1/read -> rc=0 http=200 bytes=391 [52 ms]
PROFILE req=card.version total=154 reset=0 tx=0 rx=0 parse=0
  version = notecard-11.1.1.1301
=== iteration 5/5 ===
note-emu: POST /v1/write (53 bytes) -> rc=0 http=200 [92 ms]
note-emu: POST /v1/read -> rc=0 http=200 bytes=391 [52 ms]
PROFILE req=card.version total=144 reset=0 tx=0 rx=0 parse=0
  version = notecard-11.1.1.1301
READY
```

## Publishing the badge

There's no "open this GitHub folder" URL — a one-click badge needs a project saved on
wokwi.com. Follow the same recipe as
[`esp32-notec/README.md`](../esp32-notec/README.md#publishing-the-badge):

1. Create a new ESP32-S3 project on wokwi.com.
2. Paste `src/main.cpp` as the sketch and replace `diagram.json` with this one.
3. Add a `secrets.h` (placeholder values — Wokwi projects are public, so don't commit a
   real Notehub PAT; users supply their own).
4. Add a `libraries.txt` listing `note-emu` and `note-cpp` (and any transitive deps).
   note-cpp is now published (v0.3.1+); confirm it resolves via Wokwi's library index.
5. Save, copy the `/projects/<id>` from the URL, and add a `Simulate in Wokwi` badge to
   the top of this README.

## Files

| File | Purpose |
|------|---------|
| `src/main.cpp` | Firmware — WiFi, softcard session, typed `api.card.version()` profiling loop |
| `src/secrets.h` | WiFi + Notehub PAT (gitignored) |
| `diagram.json` | Simulated board (ESP32-S3 @ 240 MHz, serial monitor) |
| `wokwi.toml` | Points the simulator at the built firmware |
| `platformio.ini` | `wokwi` build env (pulls note-cpp from GitHub via `lib_deps`) |

## See also

- [`wokwi/esp32-notec/`](../esp32-notec/) — same sketch using the note-c API (publicly runnable today)
- [`examples/platformio-notecpp/`](../../examples/platformio-notecpp/) — non-Wokwi sibling for real hardware
- [`docs/softcard-protocol.md`](../../docs/softcard-protocol.md) — softcard HTTP wire protocol details

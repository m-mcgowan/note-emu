[![Simulate in Wokwi](https://img.shields.io/badge/Simulate-Wokwi-AAB42F?logo=espressif)](https://wokwi.com/projects/465203727626487809)

# esp32-notec — note-emu on Wokwi (note-c)

A simulated ESP32-S3 that talks to the **real** Blues softcard service over Wokwi's
virtual WiFi — no physical Notecard, no hardware at all. The firmware uses the
note-c API with note-emu's HTTP transport underneath (`note::emu::Arduino`).

On boot it connects to WiFi, opens a softcard session, runs five `card.version`
round-trips as a latency profile, prints `READY`, then accepts JSON Notecard
requests typed into the serial monitor.

## Run it in your browser (one click)

1. Click the **Simulate in Wokwi** badge above.
2. In `secrets.h`, set `NOTEHUB_PAT` to your [Notehub Personal Access Token](https://notehub.io/account-settings).
   `WIFI_SSID`/`WIFI_PASS` are already `Wokwi-GUEST` / empty — Wokwi's open network has real internet egress, so TLS to `softcard.blues.com` works.
3. Press ▶. When you see `press enter to start...`, click the serial monitor and hit Enter.

The firmware connects, runs five `card.version` round-trips as a latency profile, then
prints `READY` and drops into an interactive prompt. At the `> ` prompt, type a Notecard
request as JSON and press Enter — it's sent to the real softcard and the response prints back:

```
> {"req":"card.temp"}
RSP: {"value":23.5,"calibration":-3.0}
> {"req":"card.version"}
RSP: {"version":"notecard-11.1.1.1301",...}
> {"req":"note.add","file":"data.qo","body":{"temp":22.5},"sync":true}
RSP: {"total":1}
```

Unparseable input prints `ERR: invalid JSON`.

> **WiFi can be slow or flaky in the browser.** The web simulator joins `Wokwi-GUEST`
> through Wokwi's free, shared **public gateway**, which is capacity-limited — association
> can take a while, and sometimes a run simply won't connect (the `WiFi...` dots never
> finish). This is a Wokwi free-plan limitation, not a note-emu issue: a bare WiFi-only
> sketch shows the same behavior. If a run hangs on WiFi, just restart the simulation. For
> reliable runs, use the **VS Code Wokwi extension** (see below) or real hardware.

> **First request is slow (~30 s).** softcard provisions your virtual Notecard instance on
> first contact; the initial `/v1/read` long-poll can exceed the 30 s read timeout and
> retry once. Subsequent requests are ~250 ms, and the instance stays warm between runs.

> **Tip:** `diagram.json` pins `"cpuFrequency": "240"`. Wokwi's default 8 MHz makes
> the TLS handshake ~30× slower.

## Run it locally (the iteration loop)

The in-repo files here are the source of truth; the hosted Wokwi project is a
published copy of them. Iterate locally, then re-publish when it's solid.

```sh
cp src/secrets.h.example src/secrets.h     # then add your Notehub PAT
pio run -e wokwi                            # build the firmware
```

Then either:

- **Wokwi VS Code extension** — open this folder, the extension reads `wokwi.toml`
  (which points at `.pio/build/wokwi/firmware.bin`) and runs the sim in-editor.
  Works in github.dev / Codespaces too.
- **`wokwi-cli`** — headless, scriptable, good for a fast assert loop:
  ```sh
  wokwi-cli --timeout 30000 --expect-text "READY" .
  ```
  Set `WOKWI_CLI_SERVER=ws://localhost:9177` and run a local Docker
  `wokwi-ci-server` to avoid burning cloud minutes.

## Publishing the badge

There's no "open this GitHub folder" URL — a one-click badge needs a project saved
on wokwi.com. To mint/refresh it:

1. Create a new ESP32-S3 project on wokwi.com.
2. Paste `src/main.cpp` as the sketch and replace `diagram.json` with this one.
3. Add a `secrets.h` (placeholder values — Wokwi projects are public, so don't commit a
   real Notehub PAT; users supply their own).
4. Add a `libraries.txt`:
   ```
   Blues Wireless Notecard
   note-emu
   ```
   **`note-emu` only resolves once it's published to the Arduino Library Manager.**
   Wokwi's `libraries.txt` accepts Library-Manager names and Wokwi-hosted uploads
   (`note-emu@wokwi:<id>`, a paid Wokwi Club feature) — *not* GitHub URLs. Until note-emu
   is on the registry, either upload it via the Arduino-library "+" → "Upload a Library"
   button, or vendor its `note/emu/*` sources into the project directly.
5. Save, copy the `/projects/<id>` from the URL, and drop it into the badge link at
   the top of this file.

Keep the hosted copy in sync by re-pasting after meaningful changes to `main.cpp`
or `diagram.json`.

## Files

| File | Purpose |
|------|---------|
| `src/main.cpp` | Firmware — WiFi, softcard session, profiling loop, serial command interpreter |
| `src/secrets.h` | WiFi + Notehub PAT (gitignored) |
| `diagram.json` | Simulated board (ESP32-S3 @ 240 MHz, serial monitor) |
| `wokwi.toml` | Points the simulator at the built firmware |
| `platformio.ini` | `wokwi` build env |

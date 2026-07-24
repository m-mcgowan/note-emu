# Changelog

All notable changes to this project will be documented in this file.
Follows [Keep a Changelog](https://keepachangelog.com/) conventions.

## [0.3.2] - 2026-07-24

### Added
- `Arduino::installNoteC()` method wires note-c's global serial hooks (`NoteSetFnSerial`, `NoteSetFnDefault`) to note-emu in a single call. Replaces ~6 lines of boilerplate. Guarded by `__has_include(<Notecard.h>)` so pure-note-cpp builds without note-arduino remain valid.
- `note::emu::TransportStack` + `note::emu::installNoteCpp()` in `<note/emu/note_cpp.hpp>` — one-call setup for the note-cpp streaming pipeline. Replaces the SerialHal + SerialFramer + Protocol + Notecard chain.
- New `<note/emu/note_cpp_bridge.hpp>` — provides `note::emu::NoteCBridge` and `installNoteCppBridge()` wrapping note-cpp's [documented bridge-mode pattern](https://github.com/m-mcgowan/note-cpp/blob/main/docs/platforms/host/migration-from-note-c.md#bridge-mode-incremental-migration). Enables note-c and note-cpp APIs to coexist against the same virtual Notecard.
- New `examples/platformio-bridge/` — runnable coexistence sketch, built as part of CI. Top-level README snippets its code via `tools/inject-snippets.py` so docs stay in sync.
- `docs/migrating-to-physical-notecard.md` — new "Running note-c and note-cpp in the same sketch" section covers the coexistence pattern for both virtual and physical Notecards.

## [0.3.1] - 2026-07-22

### Changed
- `<note/emu/note_cpp.hpp>` now bundles the full note-cpp typed-API + streaming transport stack (`note.hpp`, `note/link/serial.hpp`, `note/protocol.hpp`, `note/debug.hpp`). Previously it only pulled in the note-emu ↔ note-cpp bridge, forcing sketches to include the transport headers explicitly. Additive change — existing sketches that included those headers directly still compile.
- `<note-emu.h>` now uses `__has_include(<note.hpp>)` to conditionally pull in the note-cpp bridge (via `<note/emu/note_cpp.hpp>`). Brings note-cpp sketches to include-parity with note-c sketches: `#include <note-cpp.h>` triggers note-cpp auto-detection, then `#include <note-emu.h>` picks up the bridge automatically. See `wokwi/esp32-notecpp/src/main.cpp` for the new shorter form. Sketches that don't use note-cpp are unaffected.

## [0.3.0] - 2026-07-16

### Added
- Umbrella headers `<note/emu/note_c.h>` and `<note/emu/note_cpp.hpp>` as the canonical include points for each library integration. The underlying `<note/emu/emu.h>` and `<note/emu/serial_hal.hpp>` remain and still work — the umbrellas make the intent ("I'm using note-c" / "I'm using note-cpp") explicit at the include site.
- `wokwi/esp32-notecpp/` example (Wokwi project + README + captured `sample-output.txt`) — the note-cpp sibling of `wokwi/esp32-notec/`, now that note-cpp is a published dependency.
- Scheduled endpoint canary (`.github/workflows/endpoints.yml`) exercising softcard + Notehub daily against a real PAT; retries transient errors once and surfaces clearer diagnostics on failure.
- Live "Simulate in Wokwi" badge on the top-level `README.md`, pointing at the hosted `esp32-notec` project (note-c variant).
- Snippet-injection tooling (`tools/inject-snippets.py` + `.githooks/pre-commit` hook + `tools/verify-docs.sh`) that keeps example output blocks in READMEs in sync with captured firmware output.
- Real captured sample outputs at `wokwi/esp32-notec/sample-output.txt` and `wokwi/esp32-notecpp/sample-output.txt`, wired into their READMEs via snippet markers.
- Optional `hub.set` in the note-c Wokwi example when `NOTEHUB_PRODUCT` is defined (binds the virtual Notecard to a Notehub project).
- `docs/softcard-protocol.md` accuracy fixes: `X-User-UID` documented as UUID (was `user:abc123…`) and billing-accounts response shape tightened to match `tests/test_endpoints.py`.
- `.envrc` added to `.gitignore` (blocks accidental commit of local secrets like `WOKWI_CLI_TOKEN`).

### Changed
- **Credentials now come from a required `secrets.h`.** Every example that used inline `#ifndef WIFI_SSID … #define …` placeholder fallbacks now requires the user to `cp src/secrets.h.example src/secrets.h` (or the Arduino sketch-dir equivalent). A missing secrets file becomes a compile error instead of a silent binary with placeholder credentials. Applies to `examples/platformio/`, `examples/arduino/note_c_example/`, `examples/arduino/note_cpp_example/`.
- **note-cpp examples pull note-cpp from git-ref**, not a local checkout. `examples/platformio-notecpp/platformio.ini` and `wokwi/esp32-notecpp/platformio.ini` now use `note-cpp=https://github.com/m-mcgowan/note-cpp.git#v0.3.3` (note-cpp went public 2026-07-08). External users no longer need to set `NOTE_CPP_PATH`; existing local-iteration workflow is documented as an override in the ini comments.
- `src/note/emu/emu.h` header comment reframed: the core (`note_emu_t`, `note_emu_proto_*`) is note-emu's own design, with note-c and note-cpp compatibility layers riding on top. The old wording described the library as "library-agnostic glue either library can consume," which understated the deliberate note-c hook-signature match.
- `wokwi/esp32-softcard/` renamed to `wokwi/esp32-notec/` for consistency with the note-c/note-cpp naming split.
- Top-level `README.md` substantially refreshed: new Overview + "Should I use this in Production?" (why the physical Notecard beats software) + In-browser vs VS Code Wokwi paths + Example output section wired to captured sample. Headers section now lists umbrella headers as canonical.
- CI (`.github/workflows/ci.yml`) drops the private-repo `Checkout note-cpp` step and its `continue-on-error` shim now that note-cpp is public; `ci.sh` drops the `NOTE_CPP_PATH` guard around note-cpp example builds.

### Removed
- `examples/platformio-note-cpp-app/` — depended on `note-cpp-app`, which is not published on GitHub. Removed until it lands; can be restored from git history when the dependency ships.
- Broken `../../PLAN.md` link in `examples/platformio-notecpp/README.md`; retargeted to `docs/softcard-protocol.md`.

### Fixed
- `wokwi/esp32-notecpp/` brought to parity with `esp32-notec` (same secrets flow, same `platformio.ini` structure, same Wokwi `cpuFrequency: 240` workaround for TLS-handshake speed).

## [0.2.1] - 2026-05-27

### Fixed
- `<note-emu.h>` now pulls in the C++ Arduino wrapper (`note::emu::Arduino`), not just the C core. In 0.2.0 the umbrella included only `note/emu/emu.h`, so a sketch using `note::emu::Arduino` via `<note-emu.h>` failed to compile. Arduino sketches (incl. Wokwi) should include `<note-emu.h>` — Arduino's library auto-detection only adds a library to the include path from a `.h` include, so a bare `<note/emu/arduino.hpp>` isn't found ([Arduino #5441](https://github.com/arduino/Arduino/issues/5441)).

### Removed
- Stale backward-compat shim headers in `src/` root (`note_emu.h`, `note_emu_arduino.{h,cpp}`, `note_emu_curl.{h,c}`, `note_emu_serial_hal.hpp`, `note_emu.c`) left over from the namespace migration — note-emu had no prior release, so nothing depended on them.

## [0.2.0] - 2026-05-27

### Added
- `docs/softcard-protocol.md` documenting the softcard HTTP wire protocol (endpoints, headers, PAT→UID resolution, Notecard framing, and observed instance auto-provisioning / cold-start behavior)
- `wokwi/esp32-softcard/README.md` quickstart with a "Simulate in Wokwi" badge scaffold and run/iteration instructions
- Interactive `> ` prompt and usage hint after `READY` in the esp32-softcard Wokwi example so users know they can type JSON Notecard requests
- `LICENSE` file (MIT) and `src/note-emu.h` umbrella header — the library now passes `arduino-lint --library-manager submit` cleanly, ready for Arduino Library Manager submission

### Changed
- note-cpp examples now resolve the dependency from `NOTE_CPP_PATH` (and `NOTE_CPP_APP_PATH` for the note-cpp-app example) instead of a hardcoded `${HOME}/e/note-cpp` symlink path; `ci.sh` skips those builds when the variable is unset
- Tracked note-cpp transport-layer renames across examples (`note::transport::` → `note::link::`, `SerialFramer`, etc.)
- Native demo and unit-test builds compile under `-Wpedantic -Werror` with strict `-std=c11`/`-std=c++17` to catch portability issues locally
- `library.properties`: added required `maintainer` field; `includes` now points at the `note-emu.h` umbrella header
- CI: bumped `actions/checkout` v4→v6, `actions/cache` v4→v5, `actions/setup-python` v5→v6 off the deprecated Node 20 runtime

### Removed
- `PLAN.md` — superseded by the README and `docs/softcard-protocol.md`; durable protocol notes were extracted before removal
- Stale committed symlinks `examples/platformio/src/note_emu.{c,h}` (broken pre-namespace-migration paths; the example builds via its `note-emu` lib_dep)

### Fixed
- CI no longer aborts when the private note-cpp repo can't be checked out — the step is best-effort and the note-cpp examples skip gracefully

## [0.1.0] - 2026-04-23

### Added
- GitHub Actions CI workflow running `ci.sh` on push/PR (unit tests, native demo, PlatformIO builds, Arduino library compat-check)
- Wokwi ESP32 simulation examples for note-c (`wokwi/esp32-softcard`) and note-cpp (`wokwi/esp32-notecpp`) — evaluate Notecard in-browser without hardware
- note-cpp-app example (`platformio-note-cpp-app`) with ConnectionManager, NotePublisher, and EnvVar/EnvGroup for cloud-configurable environment variables
- note-cpp example (`platformio-notecpp`) with typed C++23 API and cJSON-based `JsonBackend`
- cJSON backend — first production `note::JsonBackend` implementation for note-cpp
- Status table and architecture diagram (mermaid) in README
- Serial command interpreter for firmware integration testing
- Firmware integration tests via USB serial (`test_firmware.py`)
- note-arduino integration with type-safe Arduino HTTP backend (`NoteEmuArduino`)
- Error codes (`note_emu_err_t`), logging callbacks, and `note_emu_strerror()`
- note-cpp SerialHal adapter (`SoftcardSerialHal`) for `SerialFramer` bridge
- Integration tests with end-to-end Notehub event verification (`test_softcard.py`)
- Shared Notehub API helpers (`notehub_api.py`)
- Core library: HTTP transport, TX/RX buffering, authentication state machine
- Platform backends: libcurl (native/desktop) and Arduino HTTPClient (ESP32)
- Native demo application with basic and project modes
- PlatformIO example for ESP32-S3 with note-arduino
- Per-directory READMEs for all examples

### Changed
- Renamed C++ namespace `note_emu` → `noteemu` to fix conflict with C struct tag `struct note_emu`

### Fixed
- Arduino HTTP backend: distinguish GET (NULL body) from empty POST for billing-accounts endpoint

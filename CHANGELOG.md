# Changelog

All notable changes to this project will be documented in this file.
Follows [Keep a Changelog](https://keepachangelog.com/) conventions.

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

# Changelog

All notable changes to this project will be documented in this file.
Follows [Keep a Changelog](https://keepachangelog.com/) conventions.

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
- note-cpp SerialHal adapter (`SoftcardSerialHal`) for `NotecardSerial` bridge
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

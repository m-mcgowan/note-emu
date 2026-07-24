# note-emu Examples

Start with **platformio-notecard** if you're new. See the [main README](../README.md) for setup instructions.

## PlatformIO (recommended)

### [platformio-notecard](platformio-notecard/) — note-c / note-arduino

The standard Blues Arduino API with a virtual Notecard. Uses `note::emu::Arduino` for HTTP transport and `NoteSerial_Softcard` to bridge to note-arduino's `NoteSerial` interface. **Start here.**

### [platformio-notecpp](platformio-notecpp/) — note-cpp (C++23)

Type-safe note-cpp API with the streaming transport. Uses `note::emu::SerialHal` to bridge to `note::link::SerialFramer`. Demonstrates `note::Api` with typed requests and responses.

### [platformio-bridge](platformio-bridge/) — note-c + note-cpp coexistence

Runs both APIs in one sketch against the same virtual Notecard, via note-cpp's [bridge mode](https://github.com/m-mcgowan/note-cpp/blob/main/docs/platforms/host/migration-from-note-c.md#bridge-mode-incremental-migration). Useful for incremental migration between the two libraries, or when legacy note-c code coexists with new typed-API code.

### [platformio](platformio/) — note-c raw API

PlatformIO version of the Arduino note-c sketch. Uses the note-emu C API directly with a hand-written HTTP callback.

## Arduino IDE

### [arduino/note_c_example](arduino/note_c_example/) — note-c

Arduino IDE sketch using the note-emu C API. Includes an inline HTTP backend — useful for understanding how note-emu works under the hood.

### [arduino/note_cpp_example](arduino/note_cpp_example/) — note-cpp

Arduino IDE sketch using the note-cpp streaming transport via `note::emu::Arduino` and `note::emu::SerialHal`.

## Native (no hardware)

### [native](native/) — desktop demo

Desktop demo using libcurl. Runs on macOS/Linux without embedded hardware. Exercises the softcard protocol directly.

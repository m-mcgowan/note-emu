// note-emu — public umbrella header (Arduino-style, like ArduinoJson.h).
//
// This is intentionally a ".h" carrying C++: Arduino's library auto-detection
// (Arduino IDE, Wokwi) only adds a library to the include path when it sees a
// ".h" include from it — a bare ".hpp" include is never detected, so the
// library's headers won't be found (Arduino issue #5441). So Arduino sketches
// should include <note-emu.h>, not <note/emu/arduino.hpp> directly.
//
// Pulls in the C core plus, on Arduino, the C++ wrapper note::emu::Arduino.
// arduino.hpp is #ifdef ARDUINO-guarded, so on native this is just the C core.
//
// The note/emu/ headers keep the codebase convention (.h = C, .hpp = C++);
// this umbrella is the deliberate Arduino-facing exception. For the note-cpp
// SerialHal adapter include <note/emu/serial_hal.hpp>; for the native libcurl
// backend include <note/emu/curl.h>.
#pragma once

#include "note/emu/emu.h"
#include "note/emu/arduino.hpp"

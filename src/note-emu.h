// note-emu — umbrella header.
//
// Convenience entry point matching the library name, used by the Arduino IDE's
// "Include Library" menu. Pulls in the platform-agnostic C core (the note-c
// serial-hook interface and direct read/write API).
//
// For the C++ Arduino wrapper (note::emu::Arduino) include <note/emu/arduino.hpp>;
// for the note-cpp SerialHal adapter include <note/emu/serial_hal.hpp>; for the
// native libcurl backend include <note/emu/curl.h>.
#pragma once

#include "note/emu/emu.h"

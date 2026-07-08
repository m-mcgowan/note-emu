// note-emu support for note-cpp.
//
// Include this header to use note-emu as a virtual Notecard transport for
// the note-cpp typed API. Provides note::emu::SerialHal which implements
// note::link::SerialHal for use with note::link::SerialFramer<>.
//
// This is the canonical include for note-cpp integration. The SerialHal
// adapter itself is declared in <note/emu/serial_hal.hpp>.

#pragma once

#include "serial_hal.hpp"

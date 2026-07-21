// note-emu support for note-cpp.
//
// Include this header to use note-emu as a virtual Notecard transport for
// the note-cpp typed API. It bundles the common note-cpp includes needed to
// build the streaming transport stack against note-emu, so a sketch can
// wire everything with a single include:
//
//   #include <note-emu.h>
//   #include <note/emu/note_cpp.hpp>
//
//   note::emu::Arduino softcard(NOTEHUB_PAT);
//   note::emu::SerialHal hal(*softcard.instance(), millis, delay);
//   note::link::SerialFramer<> framer(hal);
//   note::Protocol transport(framer);
//   note::Notecard nc(transport);
//   note::Api api(nc);
//
// If you need a smaller include surface, include only the specific note-cpp
// headers you use (e.g. <note/api.hpp>, <note/link/serial.hpp>) plus this
// header's bridge (see <note/emu/serial_hal.hpp>).

#pragma once

// note-emu bridge: note::emu::SerialHal (implements note-cpp's SerialHal).
#include "serial_hal.hpp"

// note-cpp typed API surface: note::Notecard, note::Api, note::body, etc.
#include <note.hpp>

// note-cpp transport stack (needed to compose the streaming pipeline above).
#include <note/link/serial.hpp>   // note::link::SerialFramer<>
#include <note/protocol.hpp>       // note::Protocol
#include <note/debug.hpp>          // note::DebugListener, note::TimingEvent

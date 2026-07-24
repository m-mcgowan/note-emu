// note-emu support for note-cpp — streaming mode.
//
// Include this header to use note-emu as a virtual Notecard transport for
// the note-cpp typed API via note-cpp's streaming pipeline:
//
//     SerialHal → SerialFramer → Protocol → Notecard(transport)
//
// No JSON backend needed. Fastest path. Recommended when the sketch uses
// note-cpp exclusively.
//
// For coexistence with note-c in the same sketch, see
// <note/emu/note_cpp_bridge.hpp> — that provides note-cpp's documented
// "bridge mode" where note-c owns the transport and note-cpp typed calls
// route through NoteRequestResponseJSON().
//
// Usage:
//
//   #include <note-cpp.h>
//   #include <note-emu.h>
//
//   note::emu::Arduino softcard(NOTEHUB_PAT);
//   softcard.begin(wifiClient);
//   auto &nc = note::emu::installNoteCpp(softcard);
//   note::Api api(nc);
//   api.card.version().execute();

#pragma once

// note-emu bridge: note::emu::SerialHal (implements note-cpp's SerialHal).
#include "serial_hal.hpp"

// note-cpp typed API surface: note::Notecard, note::Api, note::body, etc.
#include <note.hpp>

// note-cpp streaming transport stack.
#include <note/link/serial.hpp>   // note::link::SerialFramer<>
#include <note/protocol.hpp>       // note::Protocol
#include <note/debug.hpp>          // note::DebugListener, note::TimingEvent

#ifdef ARDUINO
#include "arduino.hpp"             // note::emu::Arduino (for installNoteCpp)
#endif

namespace note::emu {

// Owns the note-cpp streaming transport chain built on top of a note-emu
// softcard transport: SerialHal → SerialFramer → Protocol → Notecard.
// Constructed after Arduino::begin() succeeds; must outlive any use of
// the contained Notecard.
struct TransportStack {
    SerialHal                  hal;
    note::link::SerialFramer<> framer;
    note::Protocol             transport;
    note::Notecard             notecard;

#ifdef ARDUINO
    explicit TransportStack(
        Arduino &softcard,
        SerialHal::MillisFn millis_fn = _default_millis,
        SerialHal::DelayFn  delay_fn  = _default_delay)
      : hal(*softcard.instance(), millis_fn, delay_fn),
        framer(hal),
        transport(framer),
        notecard(transport) {}

private:
    static uint32_t _default_millis() { return ::millis(); }
    static void     _default_delay(uint32_t ms) { ::delay(ms); }
#endif
};

#ifdef ARDUINO
// Install as note-cpp streaming transport. Returns a reference to the
// constructed Notecard, ready to use with note::Api or directly.
// Must be called after Arduino::begin() succeeds.
//
// Storage for the transport stack is function-static — intended for the
// common one-softcard-per-sketch case. If you need multiple softcard
// instances or explicit ownership, construct a TransportStack yourself.
inline note::Notecard &installNoteCpp(Arduino &softcard) {
    static TransportStack stack(softcard);
    return stack.notecard;
}
#endif

}  // namespace note::emu

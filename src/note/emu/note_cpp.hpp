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

// Convenience alias so users don't need to write template syntax.
// `note::emu::Notecard` is the note-cpp NotecardApi (typed API surface +
// wrapped Notecard). Same shape as `note::arduino::Notecard` for physical
// hardware — mirrors that so sketches read the same either way.
using Notecard = note::NotecardApi<>;

// Owns the note-cpp streaming transport chain built on top of a note-emu
// softcard transport: SerialHal → SerialFramer → Protocol → NotecardApi.
// Constructed after Arduino::begin() succeeds; must outlive any use of
// the contained NotecardApi.
//
// Exposes a note::NotecardApi (rather than a raw note::Notecard), so
// `stack.notecard.card.version().execute()` works directly — mirroring
// the note::arduino::Notecard shape that physical hardware uses.
struct TransportStack {
    SerialHal                  hal;
    note::link::SerialFramer<> framer;
    note::Protocol             transport;
    Notecard                   notecard;

#ifdef ARDUINO
    explicit TransportStack(
        Arduino &softcard,
        SerialHal::MillisFn millis_fn = _default_millis,
        SerialHal::DelayFn  delay_fn  = _default_delay)
      : hal(*softcard.instance(), millis_fn, delay_fn),
        framer(hal),
        transport(framer)
    {
        notecard.begin(transport);
    }

private:
    static uint32_t _default_millis() { return ::millis(); }
    static void     _default_delay(uint32_t ms) { ::delay(ms); }
#endif
};

#ifdef ARDUINO
// Install as note-cpp streaming transport. Returns a reference to a
// note::NotecardApi — the typed API surface is directly accessible
// (`nc.card.version().execute()`, `nc.hub.set()…`) with no wrapping
// `note::Api` needed. Same call shape as physical `note::arduino::Notecard`.
//
// Must be called after Arduino::begin() succeeds. Storage is
// function-static — intended for the common one-softcard-per-sketch
// case. For multiple softcards or explicit ownership, construct a
// TransportStack yourself.
inline Notecard &installNoteCpp(Arduino &softcard) {
    static TransportStack stack(softcard);
    return stack.notecard;
}
#endif

}  // namespace note::emu

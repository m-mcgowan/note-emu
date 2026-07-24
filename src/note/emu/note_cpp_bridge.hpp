// note-emu support for note-cpp — bridge mode (coexistence with note-c).
//
// Wraps note-cpp's documented "bridge" pattern (see
// https://github.com/m-mcgowan/note-cpp/blob/main/docs/platforms/host/migration-from-note-c.md
// § "Bridge mode (incremental migration)") for use with note-emu.
//
// In bridge mode, note-c OWNS the transport and note-cpp typed calls
// route through NoteRequestResponseJSON(). Both APIs then work against
// the same virtual Notecard in one sketch — useful for incremental
// migration from note-c to note-cpp, or when a project has legacy note-c
// code you want to keep alongside new typed-API code.
//
// Requires cJSON (bundled with note-arduino). The CjsonBackend header
// pulls it in.
//
// Usage:
//
//   #include <Notecard.h>                     // brings in note-c + cJSON
//   #include <note-cpp.h>
//   #include <note-emu.h>
//   #include <note/emu/note_cpp_bridge.hpp>
//
//   note::emu::Arduino softcard(NOTEHUB_PAT);
//   softcard.begin(wifiClient);
//   softcard.installNoteC();                       // note-c owns transport
//   auto &nc = note::emu::installNoteCppBridge(softcard);
//   note::Api api(nc);
//
//   // Both APIs work against the same virtual Notecard:
//   NoteRequestResponse(NoteNewRequest("card.version"));  // note-c
//   api.card.version().execute();                         // note-cpp
//
// Requests do not overlap by natural single-threaded sketch flow. If
// you use RTOS tasks, coordinate access as you would with a physical
// Notecard on a shared bus.

#pragma once

#ifdef ARDUINO

#include "arduino.hpp"                 // note::emu::Arduino
#include "note_cpp.hpp"                // note-cpp typed API includes
#include <note/transact.hpp>           // note::ITransact
#include <note/backends/cjson.hpp>     // note::backends::CjsonBackend
#include <string>
#include <cstring>

namespace note::emu {

// Bridge transport for note-cpp on top of note-c. Implements
// note::ITransact by delegating each typed request to note-c's
// NoteRequestResponseJSON(). Mirrors the pattern from note-cpp's docs.
class NoteCBridge : public note::ITransact {
public:
    using note::ITransact::transact;
    using note::ITransact::send;

    note::Result<note::string_view> transact(note::string_view req,
                                             note::span<char> buf,
                                             uint32_t /*timeout_ms*/) override {
        scratch_.assign(req.data(), req.size());
        char *rsp = ::NoteRequestResponseJSON(scratch_.c_str());
        if (rsp == nullptr) {
            return note::make_error(note::Error::ResponseLost, NOTE_ERR("no response"));
        }
        size_t rsp_len = std::strlen(rsp);
        if (rsp_len >= buf.size()) {
            std::free(rsp);
            return note::make_error(note::Error::Overflow, NOTE_ERR("response exceeds buffer"));
        }
        std::memcpy(buf.data(), rsp, rsp_len);
        std::free(rsp);
        return note::string_view(buf.data(), rsp_len);
    }

    note::Result<void> send(note::string_view req) override {
        scratch_.assign(req.data(), req.size());
        char *rsp = ::NoteRequestResponseJSON(scratch_.c_str());
        if (rsp != nullptr) std::free(rsp);
        return {};
    }

    void reset() override {}
    void abort() override {}

    // No-op HAL — bridge mode never touches raw bytes.
    struct NoopHal : note::Hal {
        bool transmit(const uint8_t *, size_t) override { return true; }
        note::Result<size_t> read(uint8_t *, size_t, uint32_t) override {
            return note::Result<size_t>{size_t{0}};
        }
        bool reset() override { return true; }
        bool write_line_terminator() override { return true; }
        uint32_t millis() override { return ::millis(); }
        void delay(uint32_t ms) override { ::delay(ms); }
    };

    note::Hal &hal() override { return hal_; }

private:
    std::string scratch_;
    NoopHal     hal_;
};

// Owns the note-cpp bridge chain: NoteCBridge + CjsonBackend + Notecard.
// Softcard must already have `installNoteC()` called on it (or the caller
// must have wired note-c's serial hooks by other means) before use.
struct BridgeStack {
    note::backends::CjsonBackend backend;
    NoteCBridge                  bridge;
    note::Notecard               notecard;

    BridgeStack() : notecard(backend, bridge) {}
};

// Install note-cpp as a bridge on top of note-c. Returns a reference to
// the ready-to-use Notecard.
//
// Note: this expects note-c's transport (NoteSetFnSerial + friends) to
// already be wired to `softcard`. Auto-installs by calling
// `softcard.installNoteC()`; call is idempotent, so if you already
// installed note-c explicitly the extra call is harmless.
inline note::Notecard &installNoteCppBridge(Arduino &softcard) {
    softcard.installNoteC();
    static BridgeStack stack;
    return stack.notecard;
}

}  // namespace note::emu

#endif // ARDUINO

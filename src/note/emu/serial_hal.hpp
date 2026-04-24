// note::emu::SerialHal — note-cpp SerialHal adapter for note-emu
//
// Thin wrapper around the instance-based serial protocol helpers in
// note_emu.c (note_emu_proto_*). The same logic serves the note-c serial
// hooks, so behaviour is consistent across both integration paths:
//
//   - Writes are batched until a newline, then flushed as one HTTP POST.
//   - A bare "\n" (reset handshake) is answered locally with "\r\n".
//   - receive() only polls the network when a response is actually pending.
//
// Usage:
//   note::emu::Arduino softcard(NOTEHUB_PAT);
//   softcard.begin(wifiClient);
//   note::emu::SerialHal hal(*softcard.instance(), millis, delay);
//   note::transport::NotecardSerial<> transport(hal);

#pragma once

#include "emu.h"

#include <note/transport/serial.hpp>

namespace note::emu {

class SerialHal : public note::transport::SerialHal {
public:
    using MillisFn = uint32_t (*)();
    using DelayFn  = void (*)(uint32_t);

    SerialHal(note_emu_t &emu, MillisFn millis_fn, DelayFn delay_fn)
        : emu_(emu), millis_fn_(millis_fn), delay_fn_(delay_fn) {}

    bool transmit(const uint8_t *data, size_t len) override {
        return note_emu_proto_transmit(&emu_, data, len) == NOTE_EMU_OK;
    }

    size_t receive(uint8_t *buf, size_t max_len) override {
        // Trigger a network poll if data is expected, then drain the buffer.
        note_emu_proto_available(&emu_);
        return note_emu_proto_receive(&emu_, buf, max_len);
    }

    uint32_t millis() override { return millis_fn_(); }
    void delay(uint32_t ms) override { delay_fn_(ms); }

private:
    note_emu_t &emu_;
    MillisFn    millis_fn_;
    DelayFn     delay_fn_;
};

}  // namespace note::emu

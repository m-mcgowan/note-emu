// NoteSerial_Softcard — bridges note-emu to note-arduino's NoteSerial interface
//
// This glue class implements note-arduino's NoteSerial abstract interface
// using note-emu's HTTP transport to the Blues softcard service. It allows
// the standard Notecard Arduino API (notecard.begin, notecard.newRequest, etc.)
// to work with a virtual Notecard over HTTP instead of physical I2C/Serial.
//
// Usage:
//   note_emu_t *emu = note_emu_create(&config);
//   NoteSerial_Softcard softcardSerial(emu);
//   notecard.begin(&softcardSerial);

#pragma once

#include <NoteSerial.hpp>
#include <note/emu/emu.h>

class NoteSerial_Softcard final : public NoteSerial {
public:
    NoteSerial_Softcard(note_emu_t *emu);

    size_t available(void) override;
    char   receive(void) override;
    bool   reset(void) override;
    size_t transmit(uint8_t *buffer, size_t size, bool flush) override;

private:
    void refill(void);

    note_emu_t *_emu;

    // Read-ahead buffer — note_emu_read() returns a chunk, but
    // NoteSerial's receive() returns one byte at a time.
    static constexpr size_t BUF_SIZE = 512;
    uint8_t _buf[BUF_SIZE];
    size_t  _buf_len = 0;
    size_t  _buf_pos = 0;
};

// NoteSerial_Softcard — implementation

#include "NoteSerial_Softcard.h"

NoteSerial_Softcard::NoteSerial_Softcard(note_emu_t *emu)
    : _emu(emu)
{
}

void NoteSerial_Softcard::refill(void) {
    _buf_pos = 0;
    int n = note_emu_read(_emu, _buf, BUF_SIZE);
    _buf_len = (n > 0) ? (size_t)n : 0;
}

size_t NoteSerial_Softcard::available(void) {
    size_t remaining = _buf_len - _buf_pos;
    if (remaining > 0) return remaining;

    // Try to fetch more data from the softcard
    refill();
    return _buf_len - _buf_pos;
}

char NoteSerial_Softcard::receive(void) {
    if (_buf_pos >= _buf_len) {
        refill();
        if (_buf_pos >= _buf_len) return 0;
    }
    return (char)_buf[_buf_pos++];
}

bool NoteSerial_Softcard::reset(void) {
    _buf_len = 0;
    _buf_pos = 0;
    return true;
}

size_t NoteSerial_Softcard::transmit(uint8_t *buffer, size_t size, bool flush) {
    (void)flush;
    int rc = note_emu_write(_emu, buffer, size);
    return (rc == 0) ? size : 0;
}

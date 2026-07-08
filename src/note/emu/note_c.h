// note-emu support for note-c.
//
// Include this header to use note-emu as a virtual Notecard transport for
// the note-c API. Install the serial hooks via NoteSetFnSerial() / friends
// against the note_emu_t instance you create.
//
// This is the canonical include for note-c integration. The underlying
// transport API (note_emu_t, note_emu_*) is declared in <note/emu/emu.h>.

#pragma once

#include "emu.h"

# Migrating from note-emu to a physical Notecard

`note-emu` is a **transport-level** virtual Notecard: it plugs into `note-c` or
`note-cpp` at the same seam that a physical Notecard's serial/I²C driver plugs
into. That means when your Notecard arrives, only the transport wiring
changes — every line of application code keeps working as is.

This guide shows exactly what changes and what doesn't, for both `note-c` and
`note-cpp`.

## What doesn't change

- Your Notecard API calls (`hub.set`, `note.add`, `card.version`, `env.get`,
  and everything else)
- Your JSON request bodies and response parsing
- Your note templates and note bodies
- Your Notehub project, routes, event schema, and any Notehub-side code
- The `note-c` / `note-cpp` library you were already using

The only things that change are the transport-setup code near the top of
`setup()`, and the platform/hardware-side pieces (WiFi disappears, `Wire.h`
or `HardwareSerial` appears).

## A note on `note::emu::` in the examples below

Both migration examples use `note::emu::Arduino`, `note-emu`'s own C++
Arduino wrapper (declared in `<note-emu.h>` via `src/note/emu/arduino.hpp`).
It handles WiFi HTTP client wiring and softcard authentication for you.

Despite the `note::` prefix, this is unrelated to `note-cpp` code — it's a
convenience class provided by `note-emu` itself, and it works equally well
whether you're calling the `note-c` or `note-cpp` API on top. If you prefer to
avoid C++ constructs entirely, you can call the raw C API directly
(`note_emu_create()` + your own HTTP callback); see
[`examples/arduino/note_c_example/`](../examples/arduino/note_c_example/)
for that variant.

## note-c migration

### Before: note-c + note-emu

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Notecard.h>
#include <note-emu.h>
#include "secrets.h"

WiFiClientSecure wifiClient;
note::emu::Arduino softcard(NOTEHUB_PAT);   // note-emu's Arduino HTTP wrapper

void setup() {
    Serial.begin(115200);

    // WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) { delay(500); }
    wifiClient.setInsecure();

    // Connect to softcard and install note-c serial hooks
    softcard.begin(wifiClient);
    NoteSetFnDefault(malloc, free, delay, millis);
    note_emu_set_global(softcard.instance());
    NoteSetFnSerial(
        note_emu_serial_reset,
        note_emu_serial_transmit,
        note_emu_serial_available,
        note_emu_serial_receive
    );

    // Application code from here on is unchanged
    // NoteNewRequest("hub.set"), NoteRequestResponse(), etc.
    // ...
}
```

### After: note-c + physical Notecard (I²C)

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <Notecard.h>

Notecard notecard;

void setup() {
    Serial.begin(115200);

    notecard.begin();          // uses default I²C bus (Wire)

    // Application code from here on is unchanged
    // ...
}
```

### After: note-c + physical Notecard (Serial)

```cpp
#include <Arduino.h>
#include <Notecard.h>

Notecard notecard;

void setup() {
    Serial.begin(115200);

    notecard.begin(Serial1, 9600);  // pin the Notecard to Serial1

    // Application code from here on is unchanged
    // ...
}
```

## note-cpp migration

### Before: note-cpp + note-emu

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <note-cpp.h>
#include <note-emu.h>
#include "secrets.h"

WiFiClientSecure wifiClient;
note::emu::Arduino softcard(NOTEHUB_PAT);

void setup() {
    Serial.begin(115200);

    // WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) { delay(500); }
    wifiClient.setInsecure();

    // Connect to softcard and build the streaming transport pipeline
    softcard.begin(wifiClient);
    static note::emu::SerialHal hal(*softcard.instance(), millis, delay);
    static note::link::SerialFramer<> framer(hal);
    static note::Protocol transport(framer);
    static note::Notecard nc(transport);
    note::Api api(nc);

    // Application code from here on is unchanged
    // api.card.version().execute();
    // api.hub.set().product("com.example.app").mode("periodic").execute();
    // ...
}
```

### After: note-cpp + physical Notecard (I²C)

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <note-cpp.h>

note::Notecard nc;

void setup() {
    Serial.begin(115200);

    nc.begin(Wire);            // default I²C
    note::Api api(nc);

    // Application code from here on is unchanged
    // api.card.version().execute();
    // ...
}
```

### After: note-cpp + physical Notecard (Serial)

```cpp
#include <Arduino.h>
#include <note-cpp.h>

note::Notecard nc;

void setup() {
    Serial.begin(115200);

    nc.begin(Serial1, 9600);
    note::Api api(nc);

    // Application code from here on is unchanged
    // ...
}
```

## What each line of transport setup does

If you're wondering why the `note-emu` setup is longer than the physical
setup, it's because `note-emu` has more moving parts by nature — it has to
authenticate to the softcard cloud service, establish an HTTP session, and
present that session as a serial-like transport that `note-c` or `note-cpp`
recognises. A physical Notecard hides all of that behind an I²C or UART
line.

- `note::emu::Arduino softcard(NOTEHUB_PAT)` — creates the HTTP client
  that talks to softcard, authenticated with your Notehub PAT
- `softcard.begin(wifiClient)` — resolves your account UID from the PAT
  (one-time HTTP call) and establishes the softcard session
- `note::emu::SerialHal` (`note-cpp` only) — implements `note-cpp`'s
  `note::link::SerialHal` interface on top of `note-emu`'s transport
- `NoteSetFnSerial(...)` (`note-c` only) — installs the four callback
  functions `note-c` uses instead of a real UART/I²C driver
- `NoteSetFnDefault(...)` (`note-c` only) — required when using
  `NoteSetFnSerial` directly, since `notecard.begin()` normally installs
  these default functions itself

Once these are wired, the API surface is identical to a physical Notecard.

## Handling both configurations in one build

If you want a single codebase that builds for both `note-emu` and a physical
Notecard — for example, running the same firmware on a devkit in Wokwi and
on real hardware — a simple compile-time switch works well:

```cpp
#ifdef USE_NOTE_EMU
    // note-emu transport setup (as above)
#else
    // physical Notecard setup (as above)
#endif

// Application code below the switch is shared.
```

Point your CI or Wokwi build at `-DUSE_NOTE_EMU` and your hardware build
at nothing (or the inverse — whichever reads better). Everything below the
switch runs against both.

## Beyond transport: what the physical Notecard adds

The migration itself is trivial, but it's worth being aware of what your
firmware gains when you swap in real hardware:

- **Cellular / LoRa / satellite** connectivity that doesn't depend on WiFi
- **Low-power operation** — the physical Notecard sleeps at ~8 µA
- **Secure element** for device identity and TLS credentials
- **Field-hardened firmware** for unreliable-network scenarios
- **Independent flash storage** for the note store

Your application code doesn't have to change to take advantage of these —
they're inherent to the physical Notecard and its cellular/etc. modem. But
your operational planning should reflect them: for example, your test
strategy that ran fine against `note-emu` over reliable WiFi will look
different when the transport is a cellular modem with intermittent coverage.

## Where the boundaries actually are

For both `note-c` and `note-cpp`, the seam is designed to be swappable:

- **`note-c`**: the four `NoteSetFnSerial()` callbacks. A physical
  Notecard's driver installs its own implementations; `note-emu` installs
  the softcard-backed ones (`note_emu_serial_*`). Above these four
  callbacks, everything is identical.

- **`note-cpp`**: the `note::link::SerialHal` interface. A physical
  Notecard's setup installs an Arduino `Wire`- or `HardwareSerial`-backed
  implementation; `note-emu` installs `note::emu::SerialHal`, which forwards
  to the softcard HTTP transport. Above the SerialHal, everything is
  identical.

## See also

- Top-level [README](../README.md) — architecture diagram and Wokwi
  quickstart
- [`docs/softcard-protocol.md`](softcard-protocol.md) — the HTTP wire
  protocol `note-emu` speaks to softcard, if you're curious how it works
  underneath

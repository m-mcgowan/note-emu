# WASI Socket Probe — Spike Design

## Goal

Determine whether a Wokwi WASM custom chip can make outbound HTTP requests
to `softcard.blues.com`. This is the critical unknown blocking the cleanest
architecture for a Notecard emulator chip: everything in one WASM binary,
no sidecar processes.

## Context

note-emu is a C library that proxies Notecard JSON requests to the Blues
softcard simulator via HTTP. The existing Wokwi mock chip in note-cpp uses
canned responses. We want to replace that (for integration testing) with a
chip backed by the real softcard service.

Wokwi compiles custom chips to WASM using WASI-SDK (LLVM, wasm32 target).
The runtime is single-threaded and event-driven. Chips interact with the
simulation via the Wokwi chip API (UART, I2C, GPIO, timers). Whether the
runtime exposes WASI networking syscalls is unknown.

See `docs/wokwi-notecard-chip.md` for the full architecture options. This
spike evaluates Option A (WASM + WASI networking).

## Deliverable

A series of minimal probe chips in `wokwi/spike/` that progressively test
WASI capabilities. The outcome is a go/no-go answer with specific failure
data if networking is blocked.

## Runtime

All probes run against the local Docker CI server (`wokwi/wokwi-ci-server`)
to avoid burning Wokwi cloud minutes. Requires Docker installed locally.

```bash
# Start local simulation server (once)
docker run -d -p 9177:3000 --name wokwi-ci-server wokwi/wokwi-ci-server

# Run probes against it
export WOKWI_CLI_SERVER=ws://localhost:9177
~/.wokwi/bin/wokwi-cli --timeout 10000 .
```

Note: The `WOKWI_CLI_SERVER` environment variable is unverified — it was
suggested by an AI agent and needs to be confirmed. If it doesn't work,
check `wokwi-cli --help` for server override flags, or inspect the binary
for environment variable names. Worst case, use the cloud server for the
spike (budget: ~5 minutes of simulation time).

## Probes

### Probe 1: WASI stdio

**Question:** Can chip code write to stdout/stderr and have it appear in
the simulation output?

```c
#include "wokwi-api.h"
#include <stdio.h>

void chip_init(void) {
    printf("[probe-1] WASI stdio works\n");
    fprintf(stderr, "[probe-1] stderr works\n");
}
```

**Pass:** Text appears in wokwi-cli serial output or logs.
**Fail:** No output, link error, or runtime trap.

**Why this matters:** If basic WASI I/O doesn't work, nothing else will.
Also confirms the compilation and chip-loading pipeline works.

### Probe 2: DNS resolution

**Question:** Does the WASM runtime link and execute `getaddrinfo`?

```c
#include "wokwi-api.h"
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>

void chip_init(void) {
    printf("[probe-2] Attempting DNS resolution...\n");
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *result = NULL;
    int err = getaddrinfo("softcard.blues.com", "443", &hints, &result);
    if (err != 0) {
        printf("[probe-2] FAIL: getaddrinfo returned %d\n", err);
        return;
    }
    printf("[probe-2] PASS: DNS resolved\n");
    freeaddrinfo(result);
}
```

**Pass:** Prints "PASS: DNS resolved".
**Fail:** Link error (symbol not found), runtime trap, or non-zero return.

**Why this matters:** DNS resolution requires the runtime to provide
`getaddrinfo`. If this symbol isn't available at link time, WASI networking
is definitively blocked in this environment.

### Probe 3: TCP connection

**Question:** Can the chip open a TCP socket and exchange data?

```c
#include "wokwi-api.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

void chip_init(void) {
    printf("[probe-3] Attempting TCP connection...\n");

    struct addrinfo hints = {0}, *result = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo("softcard.blues.com", "80", &hints, &result) != 0) {
        printf("[probe-3] FAIL: DNS\n");
        return;
    }

    int fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (fd < 0) {
        printf("[probe-3] FAIL: socket() returned %d\n", fd);
        freeaddrinfo(result);
        return;
    }

    if (connect(fd, result->ai_addr, result->ai_addrlen) != 0) {
        printf("[probe-3] FAIL: connect()\n");
        close(fd);
        freeaddrinfo(result);
        return;
    }

    printf("[probe-3] PASS: TCP connected\n");

    // Try a minimal HTTP request
    const char *req = "GET / HTTP/1.0\r\nHost: softcard.blues.com\r\n\r\n";
    send(fd, req, strlen(req), 0);

    char buf[256] = {0};
    int n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        printf("[probe-3] PASS: received %d bytes: %.40s...\n", n, buf);
    } else {
        printf("[probe-3] FAIL: recv returned %d\n", n);
    }

    close(fd);
    freeaddrinfo(result);
}
```

**Pass:** Receives HTTP response bytes.
**Fail:** Socket creation or connect fails.

**Why this matters:** This confirms raw TCP works. Port 80 (not 443) to
avoid TLS complexity. Softcard may redirect to HTTPS, but any response
proves TCP is functional.

### Probe 4: TLS (conditional)

Only attempt if Probe 3 passes.

**Question:** Can the chip establish a TLS connection?

This is more complex. Options to try:
- WASI-SDK may bundle a TLS implementation
- mbedtls can be compiled to WASM (note-emu already uses it for ESP32)
- The runtime might provide a `wasi:http` interface that handles TLS

If raw sockets work but TLS is unavailable, we could potentially use an
HTTP-to-HTTPS proxy in the Docker environment (nginx/stunnel), keeping the
chip code simple.

**Pass:** HTTPS POST to `softcard.blues.com/v1/write` returns a response.
**Fail:** No TLS primitives available. Document what's missing.

## File Layout

```
note-emu/
  wokwi/
    spike/
      probe1-stdio.c
      probe2-dns.c
      probe3-tcp.c
      probe4-tls.c          # only if probe 3 passes
      notecard-chip.chip.json
      diagram.json           # minimal: just the custom chip, no MCU needed
      wokwi.toml
      run-probe.sh           # build + run one probe at a time
      README.md              # results log
```

The `diagram.json` needs a simulated MCU (Wokwi requires one). Use an
Arduino Uno as a minimal host — the chip init runs regardless of what
firmware the MCU has.

## Decision Tree

```
Probe 1 (stdio) fails
  → Wokwi chip API or compilation pipeline is broken. Debug before continuing.

Probe 2 (DNS) fails at link time
  → WASI-SDK doesn't provide networking headers/symbols for wasm32.
  → Verdict: WASI networking blocked. Pivot to Docker sidecar (Option B).

Probe 2 (DNS) fails at runtime
  → Symbols exist but runtime doesn't implement them.
  → Verdict: Same — pivot to Option B.

Probe 3 (TCP) fails
  → DNS works but sockets don't. Unusual but possible.
  → Verdict: Pivot to Option B.

Probe 3 (TCP) passes, Probe 4 (TLS) fails
  → Raw TCP works but no TLS.
  → Mitigation: HTTP proxy (stunnel/nginx) in Docker. Chip uses plain
    HTTP to localhost proxy, proxy handles TLS to softcard.
  → This is a viable architecture for CI (Docker-hosted proxy).
  → For VS Code: less clean, but could use a local proxy process.

All probes pass
  → Full path works. Design the real chip with note-emu core + WASI HTTP
    backend. Cleanest architecture.
```

## Success Criteria

The spike succeeds if we get a clear answer: either WASI networking works
(and we know how far — TCP only, or TCP+TLS), or it definitively doesn't
(with the specific failure point documented).

Both outcomes are valuable. "No" with data is better than "unknown".

## Time Budget

This should take 2-4 hours of implementation time. Each probe is ~20 lines
of C. The bulk of the work is setting up the build pipeline (wokwi-cli chip
compile) and the local Docker server.

If Probe 1 takes more than an hour to get working (compilation/toolchain
issues), that's a signal to timebox and document the blocker.

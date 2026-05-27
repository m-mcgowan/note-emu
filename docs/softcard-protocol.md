# Softcard HTTP Protocol

Notes on the wire protocol used by Blues' cloud-hosted virtual Notecard ("softcard"). The service is not publicly documented; this is what `note-emu` actually sends and what the in-browser terminal at `dev.blues.io` was observed doing.

## Endpoints

Base URL: `https://softcard.blues.com`

| Endpoint | Method | Purpose |
|---|---|---|
| `/v1/write` | POST | Send raw bytes to the virtual Notecard. Request body is the raw byte stream. |
| `/v1/read`  | POST | Long-poll read. Blocks until the virtual Notecard has output; response body is the raw byte stream. |

Both endpoints emulate a serial port — they do not parse JSON themselves. The Notecard's own JSON request/response framing rides on top.

## Headers

Every softcard request carries:

- `X-User-UID: <notehub-account-uid>` — routing key identifying which virtual Notecard instance to talk to. Format `user:abc123…`.
- `Authorization: Bearer <pat>` — Notehub Personal Access Token. `note-emu` always sends this when an `api_token` is configured (`src/note/emu/emu.c`, `build_headers`). When the username/password fallback is used instead, a `Cookie: <session>` header (from `auth/login`) replaces the bearer header.

## Resolving `X-User-UID` from a PAT

If the caller only has a PAT and doesn't know their account UID, `note-emu` resolves it before opening the softcard session:

```
GET https://api.notefile.net/v1/billing-accounts
Authorization: Bearer <pat>
```

The response is JSON; the first `"uid":"…"` value is taken as the account UID and used for `X-User-UID` thereafter.

## Notecard framing

The Notecard protocol is JSON-line over a byte stream — one request per line, one response per line, both terminated with `\n`.

1. Write a request: POST the JSON line to `/v1/write`.
2. Read the response: POST to `/v1/read`; blocks until the Notecard has output.

A bare `\n` is a reset handshake; the Notecard answers with `\r\n`. `note-emu` short-circuits this locally (`note_emu_proto_transmit` in `src/note/emu/emu.c`) rather than round-tripping to the server.

The browser terminal adds an `id` field to each request for response correlation:

```json
{"req":"card.version","id":2500000001}
```

The Notecard echoes the `id` back. Because the transport is sequential (one outstanding request at a time, like a real serial port), the `id` is a safety check rather than a multiplexing key. `note-emu` does not currently set or check it.

## Instance lifecycle

**Auto-provisioning (observed).** softcard spins up the virtual Notecard instance on
first contact — no need to start it from the browser UI first. The cold start is slow:
on the first `card.version` of a fresh session, the initial `/v1/read` long-poll produced
no data and the client hit its 30 s read timeout (`NOTE_EMU_READ_TIMEOUT_MS`), so note-c
resent the request; the retry returned in ~130 ms. Steady-state round-trips are then
~250 ms. A back-to-back second run had a fast first request (~1.5 s, no timeout), so the
instance stays warm for some window between sessions. (An earlier experiment with a
never-used UID saw a 504 instead of a client-side timeout — the exact cold-start failure
mode may vary.)

Practical implication: budget for one slow (~30 s) request at the start of a cold session,
or issue a throwaway warm-up request during setup so the application's first real
transaction is fast.

Still unverified:

- **Idle expiry.** No documented session timeout — the warm window above exists but its
  length isn't characterized.
- **Concurrent clients.** Behavior when both a browser and `note-emu` connect to the same UID is unverified.

## CORS

Browser access is restricted to `https://dev.blues.io` via CORS. Direct HTTP requests from firmware or a native HTTP client (libcurl, ESP-IDF, Arduino `HTTPClient`) are not subject to CORS and reach the API unimpeded.

## Source

These notes were captured by inspecting the in-browser terminal at `dev.blues.io` and by running `note-emu` against the live service. The protocol is not officially documented and is subject to change without notice.

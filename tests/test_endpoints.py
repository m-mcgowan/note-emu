#!/usr/bin/env python3
"""
Endpoint canary — verify the Notehub + softcard endpoints note-emu relies on
are still reachable and contract-compatible. Stdlib only.

Run with NOTEHUB_PAT in the environment. Exits 0 on success, non-zero on
failure. Designed for a scheduled GitHub Actions job.

Endpoints exercised (everything note-emu calls in production):

  GET  https://api.notefile.net/v1/billing-accounts   (PAT auth, UID resolution)
  POST https://softcard.blues.com/v1/write            (Notecard request bytes in)
  POST https://softcard.blues.com/v1/read             (Notecard response bytes out)

The full round-trip is `{"req":"card.version"}` -> a JSON line containing
"version":"notecard-X.Y.Z.N". The firmware version is logged but not asserted,
so routine Blues updates don't fail the canary; only contract changes do.
"""

import json
import os
import re
import sys
import time
import urllib.error
import urllib.request

NOTEHUB_API = "https://api.notefile.net"
SOFTCARD = "https://softcard.blues.com"
READ_TIMEOUT_S = 30  # matches NOTE_EMU_READ_TIMEOUT_MS


def fail(msg):
    print(f"FAIL: {msg}", file=sys.stderr)
    sys.exit(1)


def http(method, url, headers, body=None, timeout=30):
    """Returns (status, body_bytes). Network errors surface as exceptions."""
    req = urllib.request.Request(url, data=body, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.status, resp.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read() if hasattr(e, "read") else b""


def resolve_uid(pat):
    print("1. GET /v1/billing-accounts (PAT auth, UID resolution)...")
    status, body = http(
        "GET",
        f"{NOTEHUB_API}/v1/billing-accounts",
        {"Authorization": f"Bearer {pat}"},
    )
    if status != 200:
        fail(f"billing-accounts -> HTTP {status}: {body[:200]!r}")
    try:
        data = json.loads(body)
    except json.JSONDecodeError:
        fail(f"billing-accounts response not JSON: {body[:200]!r}")
    # Actual response shape (as of 2026-05): {"billing_accounts":[{"uid":...},...]}.
    # Also tolerate a bare list or top-level uid in case the API shape changes back.
    uid = None
    if isinstance(data, dict):
        accounts = data.get("billing_accounts")
        if isinstance(accounts, list) and accounts and isinstance(accounts[0], dict):
            uid = accounts[0].get("uid")
        if not uid:
            uid = data.get("uid")
    elif isinstance(data, list) and data and isinstance(data[0], dict):
        uid = data[0].get("uid")
    if not uid:
        fail(f"no uid in billing-accounts response: {body[:200]!r}")
    print(f"   OK uid={uid}")
    return uid


def write_request(headers, body):
    status, resp = http("POST", f"{SOFTCARD}/v1/write", headers, body)
    if status != 200:
        fail(f"/v1/write -> HTTP {status}: {resp[:200]!r}")


def read_response(headers):
    """Returns (body_bytes, elapsed_s). Empty bytes on timeout or non-200."""
    t0 = time.time()
    try:
        status, body = http(
            "POST", f"{SOFTCARD}/v1/read", headers, b"", timeout=READ_TIMEOUT_S
        )
    except (urllib.error.URLError, TimeoutError):
        return b"", time.time() - t0
    return (body if status == 200 else b""), time.time() - t0


def parse_and_validate(body):
    """Parse a softcard response. Returns (version, error_msg).
    version is None and error_msg explains why on any failure (non-JSON,
    explicit "err" field, missing/malformed version)."""
    if not body:
        return None, "empty response body"
    try:
        rsp = json.loads(body.decode().strip())
    except (json.JSONDecodeError, UnicodeDecodeError):
        return None, f"response not valid JSON: {body[:200]!r}"
    if "err" in rsp:
        return None, f"softcard error: {rsp['err']!r} (body: {body[:200]!r})"
    version = rsp.get("version", "")
    if not re.match(r"notecard-\d+\.\d+\.\d+\.\d+$", version):
        return None, f"unexpected version format {version!r} (body: {body[:200]!r})"
    return version, None


def main():
    pat = os.environ.get("NOTEHUB_PAT")
    if not pat:
        fail("NOTEHUB_PAT not set in environment")

    uid = resolve_uid(pat)

    softcard_headers = {
        "X-User-UID": uid,
        "Authorization": f"Bearer {pat}",
    }
    request_body = b'{"req":"card.version"}\n'

    def round_trip():
        """One write+read+validate transaction. Returns (version, err, elapsed)."""
        write_request(softcard_headers, request_body)
        body, elapsed = read_response(softcard_headers)
        version, err = parse_and_validate(body)
        return version, err, elapsed

    print("2. softcard round-trip (card.version)...")
    version, err, elapsed = round_trip()
    if err:
        # One transaction-level retry — mirrors note-c. Covers both cold-start
        # empty reads and transient softcard error responses. A genuine contract
        # break will still fail the second attempt.
        print(f"   first attempt: {err} (in {elapsed:.1f}s) — retrying once")
        version, err, elapsed = round_trip()
        if err:
            fail(f"retry also failed: {err} (in {elapsed:.1f}s)")
    print(f"   OK ({elapsed:.1f}s) notecard firmware: {version}")

    print("\nAll endpoint checks passed.")


if __name__ == "__main__":
    main()

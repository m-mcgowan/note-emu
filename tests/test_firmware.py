#!/usr/bin/env python3
"""
note-emu firmware integration test — exercises the Notecard API on real
hardware via USB serial, then verifies events arrive in Notehub.

Requires:
    - ESP32 running the platformio-notecard example firmware
    - pyserial (included in PlatformIO venv)
    - NOTEHUB_PAT environment variable
    - NOTEHUB_PRODUCT_UID environment variable (for event verification)

Usage:
    python3 tests/test_firmware.py --port /dev/cu.usbmodem1434301
    python3 tests/test_firmware.py --port auto   # auto-detect
"""

import argparse
import json
import os
import sys
import time
import uuid

import serial
import serial.tools.list_ports

from notehub_api import verify_event

BAUD_RATE = 115200
READY_TIMEOUT = 45      # seconds to wait for READY after reset
RESPONSE_TIMEOUT = 30   # seconds to wait for RSP:/ERR: after sending command


def find_port():
    """Auto-detect an ESP32 USB serial port."""
    ports = serial.tools.list_ports.comports()
    for p in ports:
        # ESP32-S3 shows up as usbmodem on macOS
        if "usbmodem" in p.device or "USB" in (p.description or ""):
            return p.device
    return None


def open_serial(port):
    """Open serial port. On ESP32-S3 USB CDC, this triggers a device reset."""
    ser = serial.Serial(port, BAUD_RATE, timeout=1)
    return ser


def wait_for_ready(port, timeout=READY_TIMEOUT):
    """Open the serial port and wait until the device is ready for commands.

    Handles two scenarios:
    1. Device reboots on port open (ESP32-S3 USB CDC) — waits for READY marker
    2. Device is already running — sends a probe command to verify

    On ESP32-S3 with USB CDC, opening the port may reset the device and the
    port briefly disappears. This function handles reconnection.

    Returns (serial_connection, True) on success, or (None, None).
    """
    print(f"Connecting to device (timeout {timeout}s)...")
    deadline = time.time() + timeout
    ser = None
    saw_boot = False

    while time.time() < deadline:
        # (Re)open port if needed
        if ser is None:
            try:
                ser = serial.Serial(port, BAUD_RATE, timeout=1)
            except (serial.SerialException, OSError):
                time.sleep(0.5)
                continue

            if not saw_boot:
                # Drain any stale data, then probe to see if already running
                time.sleep(0.2)
                try:
                    ser.reset_input_buffer()
                    ser.write(b'{"req":"card.version"}\n')
                    ser.flush()
                except (serial.SerialException, OSError):
                    try:
                        ser.close()
                    except Exception:
                        pass
                    ser = None
                    continue

        # Try to read a line
        try:
            raw = ser.readline()
        except (serial.SerialException, OSError):
            # Port dropped during USB CDC reset — close and retry
            try:
                ser.close()
            except Exception:
                pass
            ser = None
            saw_boot = True  # reset happened
            time.sleep(0.5)
            continue

        if not raw:
            continue
        text = raw.decode("utf-8", errors="replace").rstrip()
        if not text:
            continue

        print(f"  {text}")

        # Device is booting — wait for READY
        if text == "READY":
            return ser, True

        # Device responded to our probe — it's already in command mode
        if text.startswith("RSP: "):
            print("  (device already running)")
            return ser, True

        # Boot sequence indicator
        if "ESP-ROM:" in text or "note-emu" in text:
            saw_boot = True

    if ser:
        ser.close()
    return None, None


def send_command(ser, request_json):
    """Send a JSON command and wait for RSP: or ERR: response.

    Returns (response_dict, raw_line) on success, (None, error_msg) on failure.
    Skips [INFO] debug lines from note-arduino.
    """
    line = json.dumps(request_json)
    print(f"  >> {line}")
    ser.write((line + "\n").encode("utf-8"))
    ser.flush()

    deadline = time.time() + RESPONSE_TIMEOUT
    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        text = raw.decode("utf-8", errors="replace").rstrip()
        if not text:
            continue

        # Skip note-arduino debug output and note-emu log lines
        if text.startswith("[INFO]") or text.startswith("note-emu:"):
            print(f"  .. {text}")
            continue

        if text.startswith("RSP: "):
            json_str = text[5:]
            print(f"  << {json_str}")
            try:
                return json.loads(json_str), text
            except json.JSONDecodeError:
                return None, f"invalid JSON in response: {json_str}"

        if text.startswith("ERR: "):
            print(f"  !! {text}")
            return None, text[5:]

        # Other output — print but keep waiting
        print(f"  .. {text}")

    return None, "timeout waiting for response"


# ── Tests ────────────────────────────────────────────────────────────

def test_card_version(ser):
    """Send card.version and verify we get a device UID back."""
    print("\nTEST: card.version")
    rsp, err = send_command(ser, {"req": "card.version"})
    if not rsp:
        print(f"  FAIL: {err}")
        return None
    device = rsp.get("device", "")
    version = rsp.get("version", "")
    if not device:
        print("  FAIL: no device field in response")
        return None
    print(f"  device:  {device}")
    print(f"  version: {version}")
    print("  PASS")
    return rsp


def test_card_temp(ser):
    """Send card.temp and verify we get a value back."""
    print("\nTEST: card.temp")
    rsp, err = send_command(ser, {"req": "card.temp"})
    if not rsp:
        print(f"  FAIL: {err}")
        return False
    if "value" not in rsp:
        print("  FAIL: no value field in response")
        return False
    print(f"  temp: {rsp['value']} C")
    print("  PASS")
    return True


def test_note_add(ser, token, product_uid, device_uid):
    """Send note.add with a unique test_id, verify it arrives in Notehub."""
    print("\nTEST: note.add + Notehub verification")
    test_id = f"fw_{uuid.uuid4().hex[:12]}"
    timestamp = int(time.time())

    rsp, err = send_command(ser, {
        "req": "note.add",
        "file": "test.qo",
        "body": {"test_id": test_id, "timestamp": timestamp},
        "sync": True,
    })
    if not rsp:
        print(f"  FAIL: note.add failed: {err}")
        return False
    if "err" in rsp:
        print(f"  FAIL: note.add returned error: {rsp['err']}")
        return False
    print(f"  note.add OK (test_id={test_id})")

    # Verify the event arrived in Notehub
    found = verify_event(
        token=token,
        product_uid=product_uid,
        device_uid=device_uid,
        test_id=test_id,
    )
    if found:
        print("  PASS")
    else:
        print("  FAIL: event not found in Notehub")
    return found


def test_hub_get(ser):
    """Send hub.get and verify product is set."""
    print("\nTEST: hub.get")
    rsp, err = send_command(ser, {"req": "hub.get"})
    if not rsp:
        print(f"  FAIL: {err}")
        return False
    product = rsp.get("product", "")
    mode = rsp.get("mode", "")
    print(f"  product: {product}")
    print(f"  mode:    {mode}")
    if not product:
        print("  FAIL: no product in hub.get response")
        return False
    print("  PASS")
    return True


# ── Main ─────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="note-emu firmware integration test")
    parser.add_argument("--port", default="auto", help="Serial port (default: auto-detect)")
    args = parser.parse_args()

    token = os.environ.get("NOTEHUB_PAT")
    if not token:
        print("error: NOTEHUB_PAT environment variable is required", file=sys.stderr)
        sys.exit(1)

    product_uid = os.environ.get("NOTEHUB_PRODUCT_UID")

    # Find serial port
    port = args.port
    if port == "auto":
        port = find_port()
        if not port:
            print("error: could not auto-detect serial port", file=sys.stderr)
            sys.exit(1)
    print(f"Using serial port: {port}")

    # Open port and wait for READY. On ESP32-S3 USB CDC, opening the port
    # triggers a device reset. wait_for_ready handles the port reconnection.
    ser, ready = wait_for_ready(port)
    if not ready:
        print("\nFAIL: timed out waiting for READY", file=sys.stderr)
        sys.exit(1)
    print()

    # Run tests
    results = {}

    version_rsp = test_card_version(ser)
    results["card.version"] = version_rsp is not None
    device_uid = version_rsp.get("device", "") if version_rsp else ""

    results["card.temp"] = test_card_temp(ser)

    results["hub.get"] = test_hub_get(ser)

    if product_uid and device_uid:
        results["note.add"] = test_note_add(ser, token, product_uid, device_uid)
    else:
        if not product_uid:
            print("\nSKIP: note.add (NOTEHUB_PRODUCT_UID not set)")
        elif not device_uid:
            print("\nSKIP: note.add (no device UID from card.version)")
        results["note.add"] = None

    ser.close()

    # Summary
    print("\n" + "=" * 60)
    print("SUMMARY")
    print("=" * 60)
    all_pass = True
    for name, passed in results.items():
        if passed is None:
            status = "SKIP"
        elif passed:
            status = "PASS"
        else:
            status = "FAIL"
            all_pass = False
        print(f"  {name}: {status}")

    sys.exit(0 if all_pass else 1)


if __name__ == "__main__":
    main()

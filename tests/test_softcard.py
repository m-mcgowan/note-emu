#!/usr/bin/env python3
"""
note-emu integration test — verifies end-to-end softcard → Notehub flow.

Builds and runs the native demo in "project" mode, then checks that the
test note appears in the Notehub event stream via the REST API.

Environment variables:
    NOTEHUB_PAT          — Notehub Personal Access Token (required)
    NOTEHUB_PRODUCT_UID  — Notehub product UID (required)
    SOFTCARD_URL         — Override service URL (optional)

Usage:
    python3 tests/test_softcard.py           # run all tests
    python3 tests/test_softcard.py basic     # basic test only (no project needed)
    python3 tests/test_softcard.py project   # project + event verification

Future: these tests will be orchestrated by embedded-bridge test runner.
"""

import json
import os
import re
import subprocess
import sys
from pathlib import Path

from notehub_api import notehub_get, verify_event

DEMO_DIR = Path(__file__).resolve().parent.parent / "examples" / "native"
DEMO_BIN = DEMO_DIR / "note-emu-demo"


def build_demo():
    """Build the native demo if needed."""
    print("Building native demo...")
    result = subprocess.run(
        ["make"], cwd=DEMO_DIR,
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        print(f"Build failed:\n{result.stderr}", file=sys.stderr)
        sys.exit(1)
    print("Build OK\n")


def run_demo(mode):
    """Run the demo and return (stdout, returncode)."""
    print(f"Running: {DEMO_BIN} {mode}\n")
    env = {**os.environ}
    result = subprocess.run(
        [str(DEMO_BIN), mode],
        capture_output=True, text=True, env=env,
        timeout=120,
    )
    print(result.stdout)
    if result.stderr:
        print(result.stderr, file=sys.stderr)
    return result.stdout, result.returncode


def parse_result_line(output):
    """Parse 'RESULT: device=... test_id=... product=...' from demo output."""
    match = re.search(
        r"RESULT:\s+device=(\S+)\s+test_id=(\S+)\s+product=(\S+)",
        output,
    )
    if not match:
        return None
    return {
        "device": match.group(1),
        "test_id": match.group(2),
        "product": match.group(3),
    }


def test_basic(token):
    """Run basic test (no project needed)."""
    print("=" * 60)
    print("TEST: basic")
    print("=" * 60 + "\n")

    build_demo()
    output, rc = run_demo("basic")

    if rc != 0:
        print("\nFAIL: basic test exited with error")
        return False

    if "PASS" in output:
        print("PASS: basic test")
        return True
    else:
        print("FAIL: basic test")
        return False


def test_project(token):
    """Run project test with event verification."""
    print("=" * 60)
    print("TEST: project + event verification")
    print("=" * 60 + "\n")

    product_uid = os.environ.get("NOTEHUB_PRODUCT_UID")
    if not product_uid:
        print("SKIP: NOTEHUB_PRODUCT_UID not set", file=sys.stderr)
        return None  # skip, not fail

    build_demo()
    output, rc = run_demo("project")

    if rc != 0:
        print("\nFAIL: project test exited with error")
        return False

    result = parse_result_line(output)
    if not result:
        print("FAIL: could not parse RESULT line from demo output")
        return False

    # Verify the event arrived in Notehub
    found = verify_event(
        token=token,
        product_uid=result["product"],
        device_uid=result["device"],
        test_id=result["test_id"],
    )

    if found:
        print("\nPASS: project + event verification")
        return True
    else:
        print("\nFAIL: event not found in Notehub")
        return False


def main():
    token = os.environ.get("NOTEHUB_PAT")
    if not token:
        print("error: NOTEHUB_PAT environment variable is required", file=sys.stderr)
        sys.exit(1)

    # Which tests to run
    modes = sys.argv[1:] if len(sys.argv) > 1 else ["basic", "project"]

    results = {}
    for mode in modes:
        print()
        if mode == "basic":
            results["basic"] = test_basic(token)
        elif mode == "project":
            results["project"] = test_project(token)
        else:
            print(f"Unknown test mode: {mode}", file=sys.stderr)
            sys.exit(1)

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

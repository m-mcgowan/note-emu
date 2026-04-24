#!/bin/bash
# CI build script — compiles all examples and runs unit tests.
#
# Usage: ./ci.sh
#
# Exits non-zero on any build/test failure.

set -uo pipefail  # don't set -e — we collect failures across steps
cd "$(dirname "$0")"

FAILED=()

run() {
    local name="$1"
    shift
    echo ""
    echo "=== $name ==="
    if "$@"; then
        echo "=== $name: OK ==="
    else
        echo "=== $name: FAIL ==="
        FAILED+=("$name")
    fi
}

# ── Unit tests ──────────────────────────────────────────────────────

run "unit tests" bash -c '
    cd tests/unit
    cmake -B build . >/dev/null
    cmake --build build >/dev/null
    ./build/note-emu-tests
'

# ── Native build ────────────────────────────────────────────────────

run "native demo" bash -c '
    cd examples/native
    make clean >/dev/null
    make
'

# ── PlatformIO builds ───────────────────────────────────────────────

# Build each PlatformIO example (compile only — no upload)
for example in \
    examples/platformio \
    examples/platformio-notecard \
; do
    run "pio $example" bash -c "set -o pipefail; cd $example && pio run 2>&1 | tail -15"
done

# Wokwi projects have multiple envs — only build the wokwi env in CI
for example in \
    wokwi/esp32-softcard \
; do
    run "pio $example" bash -c "set -o pipefail; cd $example && pio run -e wokwi 2>&1 | tail -15"
done

# Examples that depend on note-cpp (symlinked from ~/e/note-cpp)
if [[ -d "$HOME/e/note-cpp/src" ]]; then
    for example in \
        wokwi/esp32-notecpp \
        examples/platformio-notecpp \
    ; do
        run "pio $example" bash -c "set -o pipefail; cd $example && pio run 2>&1 | tail -15"
    done
else
    echo "SKIP: note-cpp examples (~/e/note-cpp not found)"
fi

# note-cpp-app example is optional (depends on ~/e/note-cpp-app)
# Build manually: cd examples/platformio-note-cpp-app && pio run

# ── Arduino library examples (via compat-check) ─────────────────────
# Verifies the arduino/ sketches compile as a proper Arduino library,
# across multiple C++ standards. Requires embedded-cpp-compat-check.

if command -v compat-check >/dev/null; then
    run "compat-check esp32s3" bash -c '
        rm -rf /tmp/compat-check-work
        compat-check library . \
            --platform esp32s3-arduino-v3 \
            --lib-deps "blues/Blues Wireless Notecard" \
            --work-dir /tmp/compat-check-work 2>&1 | tail -20
    '
else
    echo "SKIP: compat-check not installed (install from ~/e/embedded-cpp-compat-check)"
fi

# ── Summary ─────────────────────────────────────────────────────────

echo ""
echo "=============================================="
if [[ ${#FAILED[@]} -eq 0 ]]; then
    echo "All builds and tests passed."
    exit 0
else
    echo "FAILED: ${FAILED[*]}"
    exit 1
fi

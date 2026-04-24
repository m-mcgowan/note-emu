#!/bin/bash
# Run a WASI probe chip in the Wokwi simulator.
# Usage: ./run-probe.sh <probe-source.c> [timeout_ms]
#
# Examples:
#   ./run-probe.sh probe1-stdio.c
#   ./run-probe.sh probe2-dns.c 15000
set -euo pipefail
cd "$(dirname "$0")"

SRC="${1:?Usage: $0 <probe-source.c> [timeout_ms]}"
TIMEOUT="${2:-10000}"

if [[ ! -f "$SRC" ]]; then
    echo "Error: $SRC not found" >&2
    exit 1
fi

PROBE_NAME="${SRC%.c}"

# --- Wokwi token ---
WOKWI_TOKEN_FILE="$(git rev-parse --show-toplevel 2>/dev/null)/.wokwi"
if [[ -f "$WOKWI_TOKEN_FILE" ]]; then
    source "$WOKWI_TOKEN_FILE"
elif [[ -f "$HOME/e/note-cpp/.wokwi" ]]; then
    source "$HOME/e/note-cpp/.wokwi"
fi

if [[ -z "${WOKWI_CLI_TOKEN:-}" ]]; then
    echo "Error: WOKWI_CLI_TOKEN not set (no .wokwi token file found)" >&2
    exit 1
fi
export WOKWI_CLI_TOKEN

# --- Local Wokwi CI server (Docker) ---
WOKWI_LOCAL_PORT=9177
if command -v docker &>/dev/null; then
    if docker ps --filter name=wokwi-ci-server --format '{{.Names}}' 2>/dev/null | grep -q wokwi-ci-server; then
        export WOKWI_CLI_SERVER="ws://localhost:${WOKWI_LOCAL_PORT}"
        echo "Using local Wokwi server (${WOKWI_CLI_SERVER})"
    fi
fi

# --- Build host firmware (once) ---
FW_HEX=".pio/build/uno/firmware.hex"
FW_ELF=".pio/build/uno/firmware.elf"
if [[ ! -f "$FW_HEX" ]]; then
    echo "=== Building host firmware ==="
    pio run -e uno 2>&1 | tail -5
fi

# --- Compile probe chip ---
echo "=== Compiling $SRC ==="
~/.wokwi/bin/wokwi-cli chip compile "$SRC" -o probe-chip.wasm 2>&1

# --- Write wokwi.toml ---
cat > wokwi.toml << EOF
[wokwi]
version = 1
firmware = '${FW_HEX}'
elf = '${FW_ELF}'

[[chip]]
name = 'probe-chip'
binary = 'probe-chip.wasm'
EOF

# --- Run simulation ---
echo ""
echo "=== Running $PROBE_NAME (timeout=${TIMEOUT}ms) ==="
~/.wokwi/bin/wokwi-cli \
    --timeout "$TIMEOUT" \
    --fail-text "FAIL:" \
    --expect-text "PASS:" \
    . 2>&1

#!/bin/bash
# Run note-emu integration tests with credentials from ~/.notehub-blues-mat
#
# Usage:
#   ./tests/run.sh              # all tests
#   ./tests/run.sh basic        # basic test only
#   ./tests/run.sh project      # project + event verification

set -euo pipefail

cd "$(dirname "$0")/.."

# Source credentials
if [ -f ~/.notehub-blues-mat ]; then
    set -a
    source ~/.notehub-blues-mat
    set +a
else
    echo "warning: ~/.notehub-blues-mat not found, using existing env" >&2
fi

exec python3 tests/test_softcard.py "$@"

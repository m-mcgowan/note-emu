#!/usr/bin/env bash
# Verify documentation snippets are in sync with their source files.
# Called by .githooks/pre-commit and ci.sh.
#
# Markers in markdown (see tools/inject-snippets.py for the full vocabulary):
#   <!-- snippet:<name> <source-file> -->
#   <!-- snippet:<source-file>:<start>-<end> -->
#
# Run `python3 tools/inject-snippets.py --inject README.md` to update
# snippets after editing the source file.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

echo "=== Snippet verification ==="
SNIPPET_MDS=$(find "$ROOT" -name '*.md' \
    -not -path '*/.pio/*' \
    -not -path '*/build/*' \
    -not -path '*/node_modules/*' \
    2>/dev/null || true)
python3 "$ROOT/tools/inject-snippets.py" --check $SNIPPET_MDS
echo "  OK"

#!/bin/sh
# Real SDK packages, performance comparison, Leaks and 30-minute stability.
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "${MINVEX_TEST_PYTHON:-python3}" "$SCRIPT_DIR/../RunTests.py" --platform macos --suite release --execute "$@"

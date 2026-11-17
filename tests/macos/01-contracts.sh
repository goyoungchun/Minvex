#!/bin/sh
# Optional: --plan or --output <directory>. Run the three suites in order.
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "${MINVEX_TEST_PYTHON:-python3}" "$SCRIPT_DIR/../RunTests.py" --platform macos --suite contracts --execute "$@"

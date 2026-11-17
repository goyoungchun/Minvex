#!/bin/sh
# Metal validation, Viewer, Editor and shader combinations; desktop required.
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "${MINVEX_TEST_PYTHON:-python3}" "$SCRIPT_DIR/../RunTests.py" --platform macos --suite graphics --execute "$@"

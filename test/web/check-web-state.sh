#!/bin/sh
# Runs the shared widget for a refusal and for a refusal being read again.
set -e
LC_ALL=C
export LC_ALL
SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || { echo "usage: check-web-state.sh <top source directory>" >&2; exit 2; }
CASES="$SRC/test/web/state-cases.mjs"
[ -r "$CASES" ] || { echo "check-web-state.sh: cannot read $CASES" >&2; exit 1; }
NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || { echo "check-web-state.sh: no node" >&2; exit 1; }
"$NODE" "$CASES"

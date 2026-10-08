#!/bin/sh
# Runs the remote screen's picture loader over faked captures and refusals.
set -e
LC_ALL=C
export LC_ALL
SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || { echo "usage: check-web-shot.sh <top source directory>" >&2; exit 2; }
CASES="$SRC/test/web/shot-cases.mjs"
[ -r "$CASES" ] || { echo "check-web-shot.sh: cannot read $CASES" >&2; exit 1; }
NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || { echo "check-web-shot.sh: no node" >&2; exit 1; }
"$NODE" "$CASES"

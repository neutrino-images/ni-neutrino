#!/bin/sh
# Runs the archive part's own functions over the answers its routes give.
set -e
LC_ALL=C
export LC_ALL
SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || { echo "usage: check-web-archive.sh <top source directory>" >&2; exit 2; }
CASES="$SRC/test/web/archive-cases.mjs"
[ -r "$CASES" ] || { echo "check-web-archive.sh: cannot read $CASES" >&2; exit 1; }
NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || { echo "check-web-archive.sh: no node" >&2; exit 1; }
"$NODE" "$CASES"

#!/bin/sh
# Runs the Now tile's own functions over what the playback route answers.
set -e
LC_ALL=C
export LC_ALL
SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || { echo "usage: check-web-now.sh <top source directory>" >&2; exit 2; }
CASES="$SRC/test/web/now-cases.mjs"
[ -r "$CASES" ] || { echo "check-web-now.sh: cannot read $CASES" >&2; exit 1; }
NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || { echo "check-web-now.sh: no node" >&2; exit 1; }
"$NODE" "$CASES"

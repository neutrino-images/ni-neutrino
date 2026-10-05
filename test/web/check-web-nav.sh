#!/bin/sh
# Runs the visibility rule rather than reading it.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-nav.sh <top source directory>" >&2
	exit 2
}

CASES="$SRC/test/web/nav-cases.mjs"
UNDER="$SRC/data/ni-web/app/nav.js"
for f in "$CASES" "$UNDER"; do
	[ -r "$f" ] || { echo "check-web-nav.sh: cannot read $f" >&2; exit 1; }
done

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-nav.sh: no node" >&2
	exit 1
}

"$NODE" "$CASES"

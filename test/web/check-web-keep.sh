#!/bin/sh
# What the viewer opened stays open when a screen is drawn anew.
#
# It needs node and nothing else.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-keep.sh <top source directory>" >&2
	exit 2
}

CASES="$SRC/test/web/keep-cases.mjs"
UNDER="$SRC/data/ni-web/app/ui/kept.js"

for f in "$CASES" "$UNDER"; do
	[ -r "$f" ] || { echo "check-web-keep.sh: cannot read $f" >&2; exit 1; }
done

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-keep.sh: no node, and this check runs the screens rather than reading them" >&2
	echo "  The development container carries one; set NI_WEB_NODE to use another." >&2
	exit 1
}

"$NODE" "$CASES"

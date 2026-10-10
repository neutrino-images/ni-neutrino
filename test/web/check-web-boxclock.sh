#!/bin/sh
# The box's clock as the page reads it, and what the record button sends by it.
#
# It needs node and nothing else.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-boxclock.sh <top source directory>" >&2
	exit 2
}

CASES="$SRC/test/web/boxclock-cases.mjs"
[ -r "$CASES" ] || { echo "check-web-boxclock.sh: cannot read $CASES" >&2; exit 1; }

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-boxclock.sh: no node, and this check runs the clock rather than reading it" >&2
	echo "  The development container carries one; set NI_WEB_NODE to use another." >&2
	exit 1
}

"$NODE" "$CASES"

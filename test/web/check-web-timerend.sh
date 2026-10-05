#!/bin/sh
# The end of a recording as a moment, kept in step with its minutes.
#
# It needs node and nothing else.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-timerend.sh <top source directory>" >&2
	exit 2
}

CASES="$SRC/test/web/timerend-cases.mjs"
[ -r "$CASES" ] || { echo "check-web-timerend.sh: cannot read $CASES" >&2; exit 1; }

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-timerend.sh: no node, and this check runs the model rather than reading it" >&2
	echo "  The development container carries one; set NI_WEB_NODE to use another." >&2
	exit 1
}

"$NODE" "$CASES"

#!/bin/sh
# What the page does when a call is refused under a session the box no longer has.
#
# It needs node and nothing else.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-session.sh <top source directory>" >&2
	exit 2
}

CASES="$SRC/test/web/session-cases.mjs"
UNDER="$SRC/data/ni-web/app/session.js"

for f in "$CASES" "$UNDER"; do
	[ -r "$f" ] || { echo "check-web-session.sh: cannot read $f" >&2; exit 1; }
done

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-session.sh: no node, and this check runs the page rather than reading it" >&2
	echo "  The development container carries one; set NI_WEB_NODE to use another." >&2
	exit 1
}

"$NODE" "$CASES"

#!/bin/sh
# Whether the page reads the box again when its event stream says so or comes back.
#
# It needs node and nothing else.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-events.sh <top source directory>" >&2
	exit 2
}

CASES="$SRC/test/web/events-cases.mjs"
UNDER="$SRC/data/ni-web/app/events.js"

for f in "$CASES" "$UNDER"; do
	[ -r "$f" ] || { echo "check-web-events.sh: cannot read $f" >&2; exit 1; }
done

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-events.sh: no node, and this check runs the stream's bookkeeping rather than reading it" >&2
	echo "  The development container carries one; set NI_WEB_NODE to use another." >&2
	exit 1
}

"$NODE" "$CASES"

#!/bin/sh
# Where the grid's window opens for the evening, and a day later.
#
# It needs node and nothing else. The cases are written on a Berlin clock,
# because the two nights a year the clocks change are what they are about.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-gridtime.sh <top source directory>" >&2
	exit 2
}

CASES="$SRC/test/web/gridtime-cases.mjs"
UNDER="$SRC/data/ni-web/app/screens/epg/grid.model.js"

for f in "$CASES" "$UNDER"; do
	[ -r "$f" ] || { echo "check-web-gridtime.sh: cannot read $f" >&2; exit 1; }
done

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-gridtime.sh: no node, and this check runs the rule rather than reading it" >&2
	echo "  The development container carries one; set NI_WEB_NODE to use another." >&2
	exit 1
}

TZ=Europe/Berlin "$NODE" "$CASES"

#!/bin/sh
# What the settings page draws for a stored enum value the box does not offer.
#
# It needs node and nothing else.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-settingsrows.sh <top source directory>" >&2
	exit 2
}

CASES="$SRC/test/web/settingsrows-cases.mjs"
[ -r "$CASES" ] || { echo "check-web-settingsrows.sh: cannot read $CASES" >&2; exit 1; }

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-settingsrows.sh: no node, and this check runs the page rather than reading it" >&2
	echo "  The development container carries one; set NI_WEB_NODE to use another." >&2
	exit 1
}

"$NODE" "$CASES"

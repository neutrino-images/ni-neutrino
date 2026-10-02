#!/bin/sh
# What the page does when the box answers that it is in standby.
#
# It needs node and nothing else.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-wake.sh <top source directory>" >&2
	exit 2
}

CASES="$SRC/test/web/wake-cases.mjs"
UNDER="$SRC/data/ni-web/app/ui/wake.js"

for f in "$CASES" "$UNDER"; do
	[ -r "$f" ] || { echo "check-web-wake.sh: cannot read $f" >&2; exit 1; }
done

# The code the page branches on is the server's, spelled a second time here.
SERVER="$SRC/src/coreapi/base/errors.h"
[ -r "$SERVER" ] || { echo "check-web-wake.sh: cannot read $SERVER" >&2; exit 1; }
grep -q 'case ErrorCode::BoxInStandby: return "box-in-standby";' "$SERVER" || {
	echo "check-web-wake.sh: $SERVER no longer spells BoxInStandby as box-in-standby" >&2
	exit 1
}
grep -q "'/errors/box-in-standby'" "$UNDER" || {
	echo "check-web-wake.sh: $UNDER does not branch on /errors/box-in-standby" >&2
	exit 1
}

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-wake.sh: no node, and this check runs the page rather than reading it" >&2
	echo "  The development container carries one; set NI_WEB_NODE to use another." >&2
	exit 1
}

"$NODE" "$CASES"

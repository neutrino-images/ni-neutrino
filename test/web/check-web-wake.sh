#!/bin/sh
# What the page does when the box refuses for want of leave: in standby, while
# a file plays, while a recording holds the tuner.
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
for pair in BoxInStandby:box-in-standby PlaybackRunning:playback-running RecordingHoldsTuner:recording-holds-tuner; do
	code=${pair%%:*}
	wire=${pair#*:}
	grep -q "case ErrorCode::$code: return \"$wire\";" "$SERVER" || {
		echo "check-web-wake.sh: $SERVER no longer spells $code as $wire" >&2
		exit 1
	}
	grep -q "'/errors/$wire'" "$UNDER" || {
		echo "check-web-wake.sh: $UNDER does not branch on /errors/$wire" >&2
		exit 1
	}
done

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-wake.sh: no node, and this check runs the page rather than reading it" >&2
	echo "  The development container carries one; set NI_WEB_NODE to use another." >&2
	exit 1
}

"$NODE" "$CASES"

#!/bin/sh
# The one line a screen puts on a refusal, and the toasts that carry it.
#
# It needs node and nothing else.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-refusal.sh <top source directory>" >&2
	exit 2
}

CASES="$SRC/test/web/refusal-cases.mjs"
UNDER="$SRC/data/ni-web/app/problem.js"

for f in "$CASES" "$UNDER"; do
	[ -r "$f" ] || { echo "check-web-refusal.sh: cannot read $f" >&2; exit 1; }
done

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-refusal.sh: no node, and this check runs the rule rather than reading it" >&2
	echo "  The development container carries one; set NI_WEB_NODE to use another." >&2
	exit 1
}

"$NODE" "$CASES"

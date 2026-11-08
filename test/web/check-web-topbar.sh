#!/bin/sh
# The name in the top left corner of the header, and that it leads to the main page.
#
# It needs node and the runtime fetch-types.sh puts under node_modules.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-topbar.sh <top source directory>" >&2
	exit 2
}

CASES="$SRC/test/web/topbar-cases.mjs"
[ -r "$CASES" ] || { echo "check-web-topbar.sh: cannot read $CASES" >&2; exit 1; }

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-topbar.sh: no node, and this check draws the header rather than reading it" >&2
	echo "  The development container carries one; set NI_WEB_NODE to use another." >&2
	exit 1
}

"$NODE" "$CASES"

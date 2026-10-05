#!/bin/sh
# Runs the AI area's own functions over the answers its routes give.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-ai.sh <top source directory>" >&2
	exit 2
}

CASES="$SRC/test/web/ai-cases.mjs"
[ -r "$CASES" ] || { echo "check-web-ai.sh: cannot read $CASES" >&2; exit 1; }

# The frame carries only the destination table of this area and names none of its routes.
TABLE="$SRC/data/ni-web/app/screens/ai"
[ -r "$TABLE/nav.js" ] || { echo "check-web-ai.sh: no destination table at $TABLE/nav.js" >&2; exit 1; }
extra=`find "$TABLE" -type f ! -name nav.js -print`
[ -z "$extra" ] || { echo "check-web-ai.sh: only nav.js belongs under $TABLE: $extra" >&2; exit 1; }
named=`grep -rl '/api/v1/ai/' "$SRC/data/ni-web/app" || true`
[ -z "$named" ] || { echo "check-web-ai.sh: the frame names an AI route: $named" >&2; exit 1; }

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-ai.sh: no node, and this check runs the model rather than reading it" >&2
	exit 1
}

"$NODE" "$CASES"

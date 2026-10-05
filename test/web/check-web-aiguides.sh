#!/bin/sh
# The words beside the snippets, held to the ids and codes the server uses.
# Reads the source tree only, so it runs whatever NI_WEB_MCP says.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-aiguides.sh <top source directory>" >&2
	exit 2
}

CASES="$SRC/test/web/aiguides-cases.mjs"
UNDER="$SRC/data/ni-web/ai/guidewords.js"
SERVER="$SRC/src/httpd/mcp/aiguides.cpp"
ERRORS="$SRC/src/coreapi/base/errors.h"
for f in "$CASES" "$UNDER" "$SERVER" "$ERRORS"; do
	[ -r "$f" ] || { echo "check-web-aiguides.sh: cannot read $f" >&2; exit 1; }
done

for code in ai-public-url-refused ai-trusted-proxies-refused ai-caller-would-be-tunnel ai-default-password forwarded-by-untrusted-peer; do
	grep -q "return \"$code\";" "$ERRORS" || {
		echo "check-web-aiguides.sh: $ERRORS does not spell $code" >&2
		exit 1
	}
done

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-aiguides.sh: no node, and this check runs the module rather than reading it" >&2
	exit 1
}

"$NODE" "$CASES"

#!/bin/sh
# A file whose symbols cannot be listed is refused: a stripped program would pass everything.
set -e
LC_ALL=C
export LC_ALL

NM="$1"
[ -n "$NM" ] && [ $# -ge 2 ] || {
	echo "usage: check-mcp-absent.sh <nm command> <file>..." >&2
	exit 2
}
shift

bad=0
for f in "$@"; do
	[ -r "$f" ] || { echo "check-mcp-absent.sh: cannot read $f" >&2; exit 1; }
	syms=`$NM -C "$f" 2>/dev/null` || { echo "check-mcp-absent.sh: $NM cannot read $f" >&2; exit 1; }
	[ -n "$syms" ] || { echo "check-mcp-absent.sh: $NM lists no symbols in $f" >&2; exit 1; }
	hits=`printf '%s\n' "$syms" | grep -E '(^|[^[:alnum:]_])(mcp|oauth|exposure)::' || true`
	strs=`strings -a "$f" 2>/dev/null` || { echo "check-mcp-absent.sh: strings cannot read $f" >&2; exit 1; }
	paths=`printf '%s\n' "$strs" | grep -E '^/(mcp$|mcp/|oauth/|\.well-known/oauth-|api/v1/ai$|api/v1/ai/)' || true`
	if [ -n "$hits$paths" ]; then
		echo "check-mcp-absent.sh: $f was built without --enable-mcp and carries:" >&2
		printf '%s\n' "$hits" "$paths" | grep -v '^$' | sort -u | head -20 >&2
		bad=1
	fi
done
exit $bad

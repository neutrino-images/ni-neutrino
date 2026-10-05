#!/bin/sh
# Modules under ai/ exist only on a box built with --enable-mcp; a static import of one blanks the whole page elsewhere.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-mcp.sh <top source directory>" >&2
	exit 2
}
WEB=`realpath -m "$SRC/data/ni-web"`
[ -d "$WEB/app" ] || { echo "check-web-mcp.sh: cannot read $WEB/app" >&2; exit 1; }

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
: > "$tmp/bad"

find "$WEB/app" "$WEB/info" -name '*.js' -print | sort > "$tmp/files"
[ -s "$tmp/files" ] || { echo "check-web-mcp.sh: no modules under $WEB/app" >&2; exit 1; }

while read -r f; do
	dir=`dirname "$f"`
	grep -nE "(^|[^[:alnum:]_.])from[[:blank:]]*['\"][./][^'\"]*['\"]|^[[:blank:]]*import[[:blank:]]*['\"][./][^'\"]*['\"]" "$f" \
	| while IFS= read -r line; do
		spec=`printf '%s\n' "$line" | sed -E "s/.*['\"]([./][^'\"]*)['\"].*/\1/"`
		case "$spec" in
		/*) target="$WEB$spec";;
		*) target=`realpath -m "$dir/$spec"`;;
		esac
		case "$target" in
		"$WEB"/ai/*) printf '%s:%s\n' "${f#$WEB/}" "$line" >> "$tmp/bad";;
		esac
	done
	grep -nE "['\"\`]/api/v1/ai([/?'\"\`])" "$f" | sed "s|^|${f#$WEB/}:|" >> "$tmp/bad" || true
done < "$tmp/files"

[ ! -s "$tmp/bad" ] || {
	echo "check-web-mcp.sh: these load ai/ statically or name an AI route outside ai/:" >&2
	cat "$tmp/bad" >&2
	echo "  reach ai/ with import() behind build.mcp, and call AI routes from ai/ only" >&2
	exit 1
}
echo "check-web-mcp.sh: `wc -l < "$tmp/files" | tr -d ' '` modules, none loads ai/ statically"

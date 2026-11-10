#!/bin/sh
# Copies a file of the page minified, for the install rules: the tree keeps
# its prose and its layout, the flash of a box gets neither.
#
#   minify.sh <esbuild> <source> <target>
#
# Called like cp after the first argument, so that cp stands in for the whole
# of it when the pages are installed as they are (configure.ac, WEB_STRIP).
# JavaScript and CSS go through esbuild; everything else is copied as it is.
#
# --target=es2022 is the floor jsconfig.json holds the pages to, so esbuild
# writes nothing newer than the browsers this interface supports. utf8 keeps
# umlauts as they are rather than spelling each one as a \u escape.
#
# White space and syntax are minified, names are not: a stack trace from the
# box still names the function it came from, for about 15 kB.
#
# Held to the pages reading the same by test/web/check-web-strip.sh.
set -e

[ $# -eq 3 ] || { echo "usage: minify.sh <esbuild> <source> <target>" >&2; exit 2; }
ESBUILD=$1
src=$2
dst=$3

case "$src" in
*.js|*.mjs)
	set -- --loader=js --target=es2022;;
*.css)
	set -- --loader=css;;
*)
	exec cp "$src" "$dst";;
esac

# written beside the target and renamed, so an interrupted install leaves no
# half file under the target's name
"$ESBUILD" "$@" --minify-whitespace --minify-syntax --charset=utf8 --log-level=warning < "$src" > "$dst.tmp" || {
	rm -f "$dst.tmp"
	echo "minify.sh: esbuild failed on $src" >&2
	exit 1
}
mv -f "$dst.tmp" "$dst"

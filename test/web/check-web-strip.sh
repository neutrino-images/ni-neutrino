#!/bin/sh
# With --enable-web-strip the install rules put the JavaScript and CSS of the
# page on the box minified by esbuild (data/ni-web/minify.sh). This runs the
# same script over every file they install and holds what comes out to what
# went in; strip-cases.mjs does the reading, with the type checker's parser.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-strip.sh <top source directory>" >&2
	exit 2
}

HERE="$SRC/test/web"
CASES="$HERE/strip-cases.mjs"
MINIFY="$SRC/data/ni-web/minify.sh"
TSDIR="$HERE/node_modules/typescript"
for f in "$CASES" "$MINIFY" "$TSDIR/lib/typescript.js"; do
	[ -r "$f" ] || {
		echo "check-web-strip.sh: cannot read $f" >&2
		case "$f" in *node_modules*) echo "  fetch it once with: sh test/web/fetch-types.sh" >&2;; esac
		exit 1
	}
done

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-strip.sh: no node, and the modules are read by its parser" >&2
	exit 1
}

# The esbuild configure found, else the one fetch-types.sh put here, else one
# on the path. Not skipped without one: a check that steps aside reports a
# tree nobody checked as a clean one.
ESBUILD="${ESBUILD:-${NI_WEB_ESBUILD:-}}"
[ -n "$ESBUILD" ] || { [ -x "$HERE/node_modules/esbuild/bin/esbuild" ] && ESBUILD="$HERE/node_modules/esbuild/bin/esbuild"; } || true
[ -n "$ESBUILD" ] || ESBUILD=`command -v esbuild 2>/dev/null || true`
[ -n "$ESBUILD" ] && [ -x "$ESBUILD" ] || {
	echo "check-web-strip.sh: no esbuild" >&2
	echo "  fetch it once with: sh test/web/fetch-types.sh (or set NI_WEB_ESBUILD)" >&2
	exit 1
}
export ESBUILD

"$NODE" "$CASES" "$SRC" "$TSDIR"

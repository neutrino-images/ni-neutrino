#!/bin/sh
# Runs the Now tile's own functions over what the playback route answers, and
# holds the overview's cards in one row to the height of the tallest of them.
set -e
LC_ALL=C
export LC_ALL
SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || { echo "usage: check-web-now.sh <top source directory>" >&2; exit 2; }
CASES="$SRC/test/web/now-cases.mjs"
[ -r "$CASES" ] || { echo "check-web-now.sh: cannot read $CASES" >&2; exit 1; }
NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || { echo "check-web-now.sh: no node" >&2; exit 1; }
"$NODE" "$CASES"

CSS="$SRC/data/ni-web/app/screens/now/now.css"
[ -r "$CSS" ] || { echo "check-web-now.sh: cannot read $CSS" >&2; exit 1; }
grid=$(tr '\n' ' ' <"$CSS" | grep -o '\.now-grid *{[^}]*}' || true)
[ -n "$grid" ] || { echo "check-web-now.sh: no .now-grid rule in $CSS" >&2; exit 1; }
case "$grid" in
*'align-items: stretch'*) ;;
*) echo "check-web-now.sh: .now-grid does not stretch its cards, so the cards in one row are no longer as tall as the tallest of them" >&2; exit 1 ;;
esac
# A card that sets its own alignment would drop out of its row's height again.
if tr '\n' ' ' <"$CSS" | grep -o '\.now-[a-z-]* *{[^}]*}' | grep -q 'align-self'; then
	echo "check-web-now.sh: a .now- rule sets align-self, so that card no longer stands as tall as its row" >&2
	exit 1
fi
echo "check-web-now.sh: the overview's cards stand as tall as the tallest in their row"

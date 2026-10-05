#!/bin/sh
# The EPG grid's record glyph is readable without widening the button.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-grid.sh <top source directory>" >&2
	exit 2
}

CSS="$SRC/data/ni-web/app/screens/epg/grid.css"
[ -r "$CSS" ] || { echo "check-web-grid.sh: cannot read $CSS" >&2; exit 1; }

rule=$(grep -o '\.grid__srec *{[^}]*}' "$CSS" || true)
[ -n "$rule" ] || {
	echo "check-web-grid.sh: no .grid__srec rule in $CSS" >&2
	exit 1
}

# The button's own box: unmoved by this, whatever the glyph inside it asks for.
case "$rule" in
*'min-width: var(--tap-height)'*) ;;
*) echo "check-web-grid.sh: .grid__srec lost its min-width: var(--tap-height), so the button's own box is no longer held to it" >&2; exit 1 ;;
esac
case "$rule" in
*'min-height: var(--tap-height)'*) ;;
*) echo "check-web-grid.sh: .grid__srec lost its min-height: var(--tap-height), so the button's own box is no longer held to it" >&2; exit 1 ;;
esac
case "$rule" in
*'; width:'*|*'{ width:'*) echo "check-web-grid.sh: .grid__srec names a width, which stops the content box scaling with the text size the way it did before" >&2; exit 1 ;;
esac

glyph=$(grep -o '\.grid__srec span\[aria-hidden\] *{[^}]*}' "$CSS" || true)
[ -n "$glyph" ] || {
	echo "check-web-grid.sh: no rule scales .grid__srec's glyph, so it is still the button's inherited text size" >&2
	exit 1
}

factor=$(printf '%s\n' "$glyph" | grep -o 'scale([0-9.]*)' | grep -o '[0-9.]*' || true)
[ -n "$factor" ] || {
	echo "check-web-grid.sh: the glyph rule names no transform: scale(), so nothing makes the glyph bigger" >&2
	exit 1
}

# The glyph inherits 0.9375rem; this is the factor that reads as 1.3rem or more.
awk -v v="$factor" 'BEGIN { if (v + 0 < 1.3867) exit 1 }' || {
	echo "check-web-grid.sh: the glyph's scale($factor) reads under the 1.3rem a visible timer glyph needs" >&2
	exit 1
}

echo "check-web-grid.sh: .grid__srec scales its glyph by $factor, unwidened and still held to --tap-height"

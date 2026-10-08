#!/bin/sh
# The default of a colour row is the colour the program's own theme load falls back
# to for each channel, written as the text the row states it in. The tables write
# the text and the load writes four numbers a colour, steps from 0 to 100, so a
# default changed on one side and not the other is a colour the web resets to and
# the box does not start with. Nothing else holds the two together.
#
# A channel is a step and the text is a byte: the byte of a step is
# (step * 255 + 50) / 100, which is what coreapi::colorText writes.
#
# Comments are blanked and a row under #if 0 is no row, as in the scans beside this.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-color-defaults.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
IF0="$HERE/blank-if0.awk"
for f in "$STRIP" "$IF0" "$SRC/src/gui/themes.cpp" "$SRC/src/gui/glcdthemes.cpp"; do
	[ -r "$f" ] || { echo "check-color-defaults.sh: cannot read $f" >&2; exit 1; }
done

# Below this the scan is not reading the rows any more: nineteen colours of the
# theme and three of the panel.
FLOOR=22

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# The load: every call that reads a channel with a number to fall back to.
for f in "$SRC/src/gui/themes.cpp" "$SRC/src/gui/glcdthemes.cpp"; do
	awk -v keepstrings=1 -f "$STRIP" "$f" | awk -f "$IF0"
done | awk '
	function num(s,   i, c, v, d) {
		if (s !~ /^(0[xX][0-9a-fA-F]+|[0-9]+)$/) return -1
		if (s ~ /^0[xX]/) {
			v = 0
			for (i = 3; i <= length(s); i++) {
				c = tolower(substr(s, i, 1)); d = index("0123456789abcdef", c) - 1
				v = v * 16 + d
			}
			return v
		}
		return s + 0
	}
	{
		line = $0
		while (match(line, /getInt32\("[A-Za-z0-9_]+_(red|green|blue|alpha)",[ \t]*[0-9a-fA-FxX]+\)/)) {
			call = substr(line, RSTART, RLENGTH)
			line = substr(line, RSTART + RLENGTH)
			name = call; sub(/^getInt32\("/, "", name); sub(/",.*$/, "", name)
			val = call; sub(/^[^,]*,[ \t]*/, "", val); sub(/\)$/, "", val)
			v = num(val)
			if (v < 0) continue
			print name "\t" v
		}
	}' | LC_ALL=C sort -u > "$tmp/load"

# The rows: the key, whether a fourth channel is declared, and the stated default.
find "$SRC/src/coreapi/settings" -name 'settingstable*.cpp' | LC_ALL=C sort | while read -r f; do
	awk -v keepstrings=1 -f "$STRIP" "$f" | awk -f "$IF0"
done | awk '
	/colorRow\("[A-Za-z0-9_.]+"\)/ {
		match($0, /colorRow\("[A-Za-z0-9_.]+"\)/)
		key = substr($0, RSTART + 10, RLENGTH - 12); alpha = 0; text = ""; open = 1
	}
	open && /\.withAlpha\(\)/ { alpha = 1 }
	open && match($0, /\.defaultValue\("#[0-9a-fA-F]+"\)/) {
		text = substr($0, RSTART + 15, RLENGTH - 17)
	}
	open && /\.field\(/ { print key "\t" alpha "\t" text; open = 0 }' > "$tmp/rows"

n=`awk 'END { print NR }' "$tmp/rows"`
if [ "$n" -lt "$FLOOR" ]; then
	echo "check-color-defaults.sh: $n colour rows read, the scan has stopped matching" >&2
	exit 1
fi

awk -F'\t' -v loadfile="$tmp/load" '
	FILENAME == loadfile { load[$1] = $2; next }
	{
		key = $1; alpha = $2; text = $3
		prefix = key; sub(/^[A-Za-z_]+\./, "", prefix)
		channels = alpha ? 4 : 3
		if (length(text) != 1 + 2 * channels) {
			print "  " key ": the default " text " is not " channels " channels wide"; bad = 1; next
		}
		split("red green blue alpha", ch, " ")
		for (i = 1; i <= channels; i++) {
			name = prefix "_" ch[i]
			if (!(name in load)) { print "  " key ": the load names no fallback for " name; bad = 1; continue }
			byte = int((load[name] * 255 + 50) / 100)
			want = sprintf("%02x", byte)
			got = tolower(substr(text, 2 + 2 * (i - 1), 2))
			if (got != want) {
				print "  " key ": " name " falls back to " load[name] ", which is " want ", and the row says " got
				bad = 1
			}
		}
		compared++
	}
	END {
		if (bad) { exit 1 }
		print "colour defaults compared against the theme load       " compared
	}' "$tmp/load" "$tmp/rows" || { echo "check-color-defaults.sh: a colour row starts on another colour than the program does" >&2; exit 1; }
exit 0

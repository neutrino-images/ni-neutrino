#!/bin/sh
# The headings the screens file the settings under, so that the heading a row names
# (RowBuilder::group) and the order of a section's headings (MenuGroup::order) are held
# to the menus they were read off rather than to themselves. groups.awk says what is
# read and how.
#
# Two kinds of row, tab between, the locales written as the names a row declares:
#   S  key  intro  separator  title  where   one per site that builds a setting
#   H  heading  where                        one per item leading to or titling a heading
# The H rows keep the order of each file, which is the order the menu shows them in;
# the S rows are sorted.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] || { echo "usage: extract-groups.sh <source directory>" >&2; exit 2; }

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"

# Below this the scan has stopped reading the screens. The tree holds some three hundred
# sites and some two hundred headings.
FLOOR_SITES=200
FLOOR_HEADINGS=100

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

MARK='@@file@@'

find "$SRC/gui" -name '*.cpp' | LC_ALL=C sort > "$tmp/files"
xargs awk -v keepstrings=1 -v mark="$MARK" -f "$STRIP" < "$tmp/files" \
	| awk -f "$BLANK" \
	| awk -v mark="$MARK" -f "$HERE/groups.awk" > "$tmp/raw"

sh "$HERE/extract-bounds.sh" -m "$SRC" > "$tmp/locale"

# Every enumerator turned into the name it stands for; one the map does not hold is ?,
# so a heading the scan misread shows as one rather than as a name nobody declared.
awk -F'\t' -v OFS='\t' -v locfile="$tmp/locale" '
	function named(s,   n, i, part, out) {
		if (s == "-") return s
		n = split(s, part, "|")
		out = ""
		for (i = 1; i <= n; i++) out = out (i > 1 ? "|" : "") ((part[i] in loc) ? loc[part[i]] : "?")
		return out
	}
	FILENAME == locfile { loc[$1] = $2; next }
	$1 == "S" { print "S", $2, named($3), named($4), named($5), $6; next }
	$1 == "H" { print "H", named($2), $3 }
' "$tmp/locale" "$tmp/raw" > "$tmp/named"

grep '^S' "$tmp/named" | sort -u > "$tmp/sites" || true
grep '^H' "$tmp/named" > "$tmp/headings" || true

sites=`wc -l < "$tmp/sites"`
headings=`wc -l < "$tmp/headings"`
if [ "$sites" -lt "$FLOOR_SITES" ] || [ "$headings" -lt "$FLOOR_HEADINGS" ]; then
	echo "extract-groups.sh: $sites sites and $headings headings read out of $SRC/gui," >&2
	echo "  under $FLOOR_SITES and $FLOOR_HEADINGS: the scan has stopped matching" >&2
	exit 1
fi

cat "$tmp/sites" "$tmp/headings"

#!/bin/sh
# Every label_key, hint_key, unit_key and format_key a settings row declares, and every label_key one of
# an Enum's own choices declares, held to the catalog each is said to be a name in:
# data/locale/english.locale. check-fieldtypes.sh and check-sections.sh both read
# these same tables and neither opens the catalog, so a key mistyped here compiles,
# links, passes both of them, and answers on the wire as the very spelling this
# route exists to translate away.
#
# The catalog is what CLocaleManager::getLocale (src/system/localize.cpp) resolves a
# name against by way of locale_real_names, the compiled table src/system/
# locals_intern.h carries. What is missing only here, and the one CLocaleManager
# cannot itself tell apart from a real translation, is a name locals_intern.h
# carries and english.locale, the file actually loaded at start, does not: for the
# default locale that load leaves the row holding its own key as the "text" nothing
# overwrote, so getText() answers the spelling back rather than failing where
# getLocale() could have said so.
#
# One key per line out of the tables, held against one key per line out of the
# catalog with comm, so an addition on either side is named rather than counted.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-locale-catalog.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
CATALOG="$SRC/data/locale/english.locale"
TABLEDIR="$SRC/src/coreapi/settings"

[ -r "$STRIP" ] || { echo "check-locale-catalog.sh: cannot read $STRIP" >&2; exit 1; }
[ -r "$CATALOG" ] || { echo "check-locale-catalog.sh: cannot read $CATALOG" >&2; exit 1; }

# settingstable.cpp declares five rows of the general section itself, beside
# the fifteen files that each hold one further section's; a glob of
# settingstable_*.cpp alone misses those five and the keys they name.
set -- "$TABLEDIR/settingstable.cpp" "$TABLEDIR"/settingstable_*.cpp
# One more table file, for the self test that holds a mistyped key to the catalog.
[ -z "$CATALOG_CHECK_EXTRA" ] || set -- "$@" "$CATALOG_CHECK_EXTRA"
for f in "$@"; do
	[ -r "$f" ] || { echo "check-locale-catalog.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

awk -v keepstrings=1 -f "$STRIP" "$@" > "$tmp/stripped"

# The catalog itself: one key per line, up to the first space, sorted in the C
# locale so a byte order the box never runs under cannot move a comparison.
awk '{ print $1 }' "$CATALOG" | LC_ALL=C sort -u > "$tmp/catalog"

cn=`wc -l < "$tmp/catalog"`
if [ "$cn" -lt 3000 ]; then
	echo "check-locale-catalog.sh: $cn keys read out of $CATALOG, the catalog is not the one this expects" >&2
	exit 1
fi

# A row names its label, hint, unit and format in calls of their own, one to a line, and
# an entry names its label in the same call on the line of its option. The two
# are told apart by that line: a row's call starts the line, an entry's follows
# the option it belongs to.
awk '
	/^[ \t]*\.(label|hint|unit|format)\("[^"]*"\)[ \t]*;?[ \t]*$/ {
		line = $0
		match(line, /"[^"]*"/)
		print substr(line, RSTART + 1, RLENGTH - 2)
	}
' "$tmp/stripped" | LC_ALL=C sort -u > "$tmp/labelhint"

# An Enum's own choices are one option per line, with the key of its label in a
# call on that line or fixed text in another. Kept apart from the pairs above
# rather than merged in the same pass, because a route answers a choice's own
# label as never absent (ep_settings.cpp's kEnumValueFields), which is a claim
# this makes for the choices alone and has to be checked, and reported, on its
# own count: a scan that stopped matching only this shape would otherwise hide
# behind the pairs still matching theirs. Only label() names a key. An entry
# that carries fixed text has its text in text(), which is shown as written and
# is no catalog name, so the pattern takes no quote anywhere else.
awk '
	/^[ \t]*option\([^"]*\)\.label\("[^"]*"\)/ {
		line = $0
		if (match(line, /\.label\("[^"]*"\)/)) {
			line = substr(line, RSTART, RLENGTH)
			match(line, /"[^"]*"/)
			print substr(line, RSTART + 1, RLENGTH - 2)
		}
	}
' "$tmp/stripped" | LC_ALL=C sort -u > "$tmp/choices"

lh=`wc -l < "$tmp/labelhint"`
ch=`wc -l < "$tmp/choices"`
# Both floors are well under half of what the tables carry today, so a scan
# that has stopped matching either shape reads as a fault here rather than
# as a quiet pass over nothing.
if [ "$lh" -lt 300 ]; then
	echo "check-locale-catalog.sh: $lh distinct label_key/hint_key names read, out of `wc -l < "$tmp/stripped"` lines," >&2
	echo "  the scan has stopped matching that shape" >&2
	exit 1
fi
if [ "$ch" -lt 100 ]; then
	echo "check-locale-catalog.sh: $ch distinct choice label names read, out of `wc -l < "$tmp/stripped"` lines," >&2
	echo "  the scan has stopped matching that shape" >&2
	exit 1
fi

# Every label(, hint(, unit( and format( call the tables make has to be one the two patterns
# above read, which the floors cannot say: a call wrapped onto the next line, or
# two calls on one line, would drop out of the check one key at a time and leave
# the totals standing. A line carrying such a call that neither pattern takes is
# named and fails.
awk '
	function calls(l,   n, t) { n = 0; t = l; while (match(t, /\.(label|hint|unit|format)\(/)) { n++; t = substr(t, RSTART + RLENGTH) } return n }
	{
		n = calls($0)
		if (n == 0) next
		if (n == 1 && ($0 ~ /^[ \t]*\.(label|hint|unit|format)\("[^"]*"\)[ \t]*;?[ \t]*$/ ||
		               $0 ~ /^[ \t]*option\([^"]*\)\.label\("[^"]*"\)/))
			next
		print
	}
' "$tmp/stripped" > "$tmp/unread"
if [ -s "$tmp/unread" ]; then
	echo "check-locale-catalog.sh: a label, hint, unit or format call the scan does not read, so its key is held to nothing:" >&2
	sed 's/^[ \t]*/  /' "$tmp/unread" >&2
	exit 1
fi

# A unit is the text after a number and a format holds the number itself, so a
# unit that carries a %d would print twice and a format without one would never
# show the value. Told apart by the call, which is why they are read apart.
awk '
	/^[ \t]*\.unit\("[^"]*"\)[ \t]*$/ { match($0, /"[^"]*"/); print substr($0, RSTART + 1, RLENGTH - 2) }
' "$tmp/stripped" | LC_ALL=C sort -u > "$tmp/units"
awk '
	/^[ \t]*\.format\("[^"]*"\)[ \t]*$/ { match($0, /"[^"]*"/); print substr($0, RSTART + 1, RLENGTH - 2) }
' "$tmp/stripped" | LC_ALL=C sort -u > "$tmp/formats"
if [ ! -s "$tmp/units" ] || [ ! -s "$tmp/formats" ]; then
	echo "check-locale-catalog.sh: no unit or no format name read out of the tables, the scan has stopped matching that shape" >&2
	exit 1
fi
while read -r k; do
	text=`awk -v k="$k" '$1 == k { sub(/^[^ ]* /, ""); print }' "$CATALOG"`
	case "$text" in *%d*) echo "check-locale-catalog.sh: the unit $k holds a %d, which the number would print twice" >&2; exit 1;; esac
done < "$tmp/units"
while read -r k; do
	text=`awk -v k="$k" '$1 == k { sub(/^[^ ]* /, ""); print }' "$CATALOG"`
	case "$text" in *%d*) ;; *) echo "check-locale-catalog.sh: the format $k holds no %d, so it would never show the value" >&2; exit 1;; esac
done < "$tmp/formats"

cat "$tmp/labelhint" "$tmp/choices" | LC_ALL=C sort -u > "$tmp/used"
used=`wc -l < "$tmp/used"`

missing=`comm -23 "$tmp/used" "$tmp/catalog"`
if [ -n "$missing" ]; then
	echo "check-locale-catalog.sh: a label_key, hint_key, unit_key, format_key or choice label the tables declare that the catalog does not carry:" >&2
	printf '%s\n' "$missing" | sed 's/^/  /' >&2
	exit 1
fi

echo "settings label/hint/unit/format keys held to the catalog     $used"
echo "  of which the choices' own labels                          $ch"
exit 0

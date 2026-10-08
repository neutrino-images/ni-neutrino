#!/bin/sh
# The key a setting is stored under, the field its value lives in and the default it
# falls back to, as the program states all three itself: a load site names them together
# and the save site beside it names the first two again. Four columns, tab between: key,
# field, what kind of default it is, and the default.
#
# The kind is int for a number, str for a string, none for a read with no fallback at
# all, and expr for a default this scan cannot turn into a value. An expression is
# printed rather than dropped, so a row it cannot check is visible as one.
#
# A key loaded on two lines under different conditions has a row for each, because which
# of them a build sees depends on the hardware and this scan reads both arms.
#
# The intersection of the two sides and not the load side alone. Three keys are still
# read under a spelling the program no longer writes, and a scan of the load side would
# take those for the current one.
#
# With -b, and the build's config.h as a third argument, only the loads and saves the
# build itself compiles: the conditional arms are resolved against that header by the
# preprocessor and what it drops is not read. Where the table above holds every arm
# a key has, this holds the one arm this build takes, which is the only one a row
# written for this build can be held to exactly.
#
# With -k and a source directory instead, every key any read in the tree names. That is
# the weaker question, and the only one that can be asked of a setting whose value is
# read outside the file the loop above lives in.
set -e
LC_ALL=C
export LC_ALL

# Below this the scan is not measuring the file any more. A regex that stopped
# matching would otherwise leave every check built on this comparing nothing,
# and pass. The tree held 430 pairs, 292 of them with a default this can
# compare, and 732 keys when this was written.
FLOOR_PAIRS=400
FLOOR_DEFAULTS=270
FLOOR_KEYS=680

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

if [ "$1" = "-k" ]; then
	[ -n "$2" ] || { echo "usage: extract-pairs.sh -k <source directory>" >&2; exit 2; }
	find "$2" -name '*.cpp' -o -name '*.h' | LC_ALL=C sort > "$tmp/files"
	while read -r f; do
		awk -v keepstrings=1 -f "$STRIP" "$f"
	done < "$tmp/files" \
		| grep -oE '(configfile\.|tconfig->)get[A-Za-z0-9_]*\([ 	]*"[^"]*"' \
		| sed -E 's/^(configfile\.|tconfig->)get[A-Za-z0-9_]*\([ 	]*"([^"]*)"$/\2/' \
		| sort -u > "$tmp/keys"
	n=`wc -l < "$tmp/keys"`
	if [ "$n" -lt "$FLOOR_KEYS" ]; then
		echo "extract-pairs.sh: $n keys is below the floor of $FLOOR_KEYS, the scan has stopped matching" >&2
		exit 1
	fi
	cat "$tmp/keys"
	exit 0
fi

BUILT=""
if [ "$1" = "-b" ]; then
	BUILT=1
	shift
	# Fewer pairs than the arms together, by the keys only another model loads.
	FLOOR_PAIRS=330
	FLOOR_DEFAULTS=220
fi

SRC="$1"
SRCDIR="$2"
CONFIG="$3"
[ -n "$SRC" ] && [ -n "$SRCDIR" ] || { echo "usage: extract-pairs.sh [-b] <neutrino.cpp> <source directory> [<config.h>]" >&2; exit 2; }
if [ -n "$BUILT" ]; then
	[ -r "$CONFIG" ] || { echo "extract-pairs.sh: -b needs the build's config.h as a third argument" >&2; exit 2; }
fi

awk -v keepstrings=1 -f "$STRIP" "$SRC" > "$tmp/code"

# The two themes are loaded and saved in files of their own, through a reference
# to the struct they keep: written out as the member that holds it, so that they are
# read as every other load is. Only a name that stands alone is rewritten. Their own
# definitions of macros are left out: the two files each define the same name for a
# different thing, which the preprocessor would say, and nothing here reads one.
for pair in "gui/themes.cpp theme" "gui/glcdthemes.cpp glcd_theme"; do
	file=${pair% *}
	member=${pair#* }
	[ -r "$SRCDIR/$file" ] || { echo "extract-pairs.sh: cannot read $SRCDIR/$file" >&2; exit 2; }
	awk -v keepstrings=1 -f "$STRIP" "$SRCDIR/$file" \
		| sed -E "/^[ 	]*#[ 	]*(define|undef)/d; s/(^|[^A-Za-z0-9_.])t\\./\\1g_settings.$member./g" >> "$tmp/code"
done

# Only the conditionals are evaluated: the includes are taken out so that nothing is
# looked for, and the directives-only mode leaves the text between them as it was,
# so a name the code uses is not replaced by what the header defines it as.
if [ -n "$BUILT" ]; then
	grep -v '^[ 	]*#[ 	]*include' "$tmp/code" > "$tmp/code.in"
	${CXXCPP:-cpp} -P -fdirectives-only -include "$CONFIG" -x c++ "$tmp/code.in" > "$tmp/code" \
		|| { echo "extract-pairs.sh: the preprocessor refused the source" >&2; exit 1; }
fi

# The field is a plain name in both patterns, so an element of an array and a
# member reached through one are left out rather than half read. None of them is
# a setting a single key names.
awk -f "$HERE/pairs.awk" "$tmp/code" | sort -u > "$tmp/load"

# Two spellings of the same save, because a field a thread writes is copied
# through the accessor on the way out. Both name the key and the member, which is
# all this reads them for.
{
grep -oE '(configfile\.|tconfig->)set[A-Za-z0-9_]*\([ 	]*"[^"]*"[ 	]*,[ 	]*g_settings\.(theme\.|glcd_theme\.)?[A-Za-z_][A-Za-z_0-9]*[ 	]*\)' "$tmp/code" \
	| sed -E 's/^(configfile\.|tconfig->)set[A-Za-z0-9_]*\([ 	]*"([^"]*)"[ 	]*,[ 	]*g_settings\.((theme\.|glcd_theme\.)?[A-Za-z_0-9]+)[ 	]*\)$/\2	\3/'
grep -oE '(configfile\.|tconfig->)set[A-Za-z0-9_]*\([ 	]*"[^"]*"[ 	]*,[ 	]*settingsText\([ 	]*g_settings\.(theme\.|glcd_theme\.)?[A-Za-z_][A-Za-z_0-9]*[ 	]*\)' "$tmp/code" \
	| sed -E 's/^(configfile\.|tconfig->)set[A-Za-z0-9_]*\([ 	]*"([^"]*)"[ 	]*,[ 	]*settingsText\([ 	]*g_settings\.((theme\.|glcd_theme\.)?[A-Za-z_0-9]+)[ 	]*\)$/\2	\3/'
} | sort -u > "$tmp/save"

sh "$HERE/extract-defines.sh" "$SRCDIR" > "$tmp/defines"

# The pair has to be on both sides, and then the default is classified. A name
# the headers give a number to becomes that number; anything else the scan
# cannot evaluate stays as the text it was, under expr.
awk -F'	' -v savefile="$tmp/save" -v deffile="$tmp/defines" '
	FILENAME == savefile { seen[$1 "\t" $2] = 1; next }
	FILENAME == deffile { def[$1] = $2; next }
	!(($1 "\t" $2) in seen) { next }
	{
		d = $3
		if (d == "@none") { print $1 "\t" $2 "\tnone\t"; next }
		if (d ~ /^-?[0-9]+$/) { print $1 "\t" $2 "\tint\t" d; next }
		if (d == "true") { print $1 "\t" $2 "\tint\t1"; next }
		if (d == "false") { print $1 "\t" $2 "\tint\t0"; next }
		if (d ~ /^"[^"]*"$/) {
			t = substr(d, 2, length(d) - 2)
			print $1 "\t" $2 "\tstr\t" t
			next
		}
		if (d in def) { print $1 "\t" $2 "\tint\t" def[d]; next }
		print $1 "\t" $2 "\texpr\t" d
	}
' "$tmp/save" "$tmp/defines" "$tmp/load" | sort -u > "$tmp/both"

pairs=`cut -f1,2 "$tmp/both" | sort -u | wc -l`
if [ "$pairs" -lt "$FLOOR_PAIRS" ]; then
	echo "extract-pairs.sh: $pairs pairs is below the floor of $FLOOR_PAIRS, the scan has stopped matching" >&2
	echo "  loads matched: `cut -f1,2 "$tmp/load" | sort -u | wc -l`, saves matched: `wc -l < "$tmp/save"`" >&2
	exit 1
fi

# A default the scan can compare, counted on its own: a change that left the
# pairs standing and turned every default into an expression would otherwise
# pass while checking none of them.
comparable=`awk -F'	' '$3 != "expr" && $3 != "none"' "$tmp/both" | cut -f1,2 | sort -u | wc -l`
if [ "$comparable" -lt "$FLOOR_DEFAULTS" ]; then
	echo "extract-pairs.sh: $comparable comparable defaults is below the floor of $FLOOR_DEFAULTS," >&2
	echo "  the scan has stopped reading them; $pairs pairs were matched" >&2
	exit 1
fi

cat "$tmp/both"

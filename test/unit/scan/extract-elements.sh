#!/bin/sh
# The key an element of an array is stored under and the default it falls back to, as
# the program states both itself. An array is loaded in a loop that builds the key from
# the number of the element, or out of a table that lists the elements in the order the
# array's enumeration does, so no key stands in the source as one literal the way a
# plain setting's does and the scan that reads those cannot see these.
#
# Two kinds of line, tab between:
#
#   L member format kind default
#       a loop: the key is the format with the element's number put where %d is.
#       Written once for the loop and not once for each element, because how far the
#       loop runs is not in the line that builds the key.
#   T member enumerator key kind default guard
#       a table: one line for each entry, named by the enumerator of the array's
#       enumeration it stands at. guard is the condition the enumerator sits behind,
#       - for none, so that a case compiling the line can leave it out of a build
#       that lacks the enumerator.
#
# The kind is int for a number, str for a string, none for a read with no fallback and
# expr for a default this scan cannot turn into a value, as extract-pairs.sh states them.
set -e
LC_ALL=C
export LC_ALL

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"

SRC="$1"
SRCDIR="$2"
LOCALES="$3"
[ -n "$SRC" ] && [ -n "$SRCDIR" ] && [ -n "$LOCALES" ] || {
	echo "usage: extract-elements.sh <neutrino.cpp> <source directory> <locale names>" >&2
	exit 2
}
for f in "$SRC" "$SRCDIR/system/settings.h" "$SRCDIR/system/settings.cpp" "$LOCALES" "$STRIP"; do
	[ -r "$f" ] || { echo "extract-elements.sh: cannot read $f" >&2; exit 2; }
done

# Below this the scan is not reading the source any more. The tree held 14 loops and
# 90 table entries when this was written.
FLOOR_LOOPS=10
FLOOR_ENTRIES=80

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

sh "$HERE/extract-defines.sh" "$SRCDIR" > "$tmp/defines"

# Two kinds of name the plain integers above leave out and the defaults of these
# elements are written with: a number in hexadecimal, and an enumerator of the screen
# that offers personalizing, named through its class.
awk -v keepstrings=0 -f "$STRIP" "$SRCDIR/system/settings.h" | awk '
	function hexval(h,   i, c, v, d) {
		v = 0
		for (i = 3; i <= length(h); i++) {
			c = tolower(substr(h, i, 1))
			d = index("0123456789abcdef", c) - 1
			v = v * 16 + d
		}
		return v
	}
	/^[ \t]*#[ \t]*define[ \t]+[A-Za-z_][A-Za-z_0-9]*[ \t]+0[xX][0-9a-fA-F]+[ \t]*$/ {
		n = $2; v = $3
		print n "\t" hexval(v)
	}
' >> "$tmp/defines"
if [ -r "$SRCDIR/gui/personalize.h" ]; then
	awk -v keepstrings=0 -f "$STRIP" "$SRCDIR/gui/personalize.h" | awk '
		/^[ \t]*enum[ \t]/ { inenum = 1; seen = 0; next }
		inenum && /\{/ { next }
		inenum && /\}/ { inenum = 0; next }
		inenum {
			e = $0; gsub(/^[ \t]+|[ \t,]+$/, "", e)
			if (e == "") next
			name = e; sub(/[ \t]*=.*$/, "", name)
			if (e ~ /=/) { v = e; sub(/^[^=]*=[ \t]*/, "", v); sub(/[ \t,].*$/, "", v); value = v + 0 }
			if (v == "" || e !~ /=/) value = (seen ? value + 1 : 0)
			seen = 1
			print "CPersonalizeGui::" name "\t" value
			v = ""
		}
	' >> "$tmp/defines"
fi
sort -u "$tmp/defines" -o "$tmp/defines"
awk -v keepstrings=1 -f "$STRIP" "$SRC" > "$tmp/neutrino"
awk -v keepstrings=1 -f "$STRIP" "$SRCDIR/system/settings.h" > "$tmp/settings.h"
awk -v keepstrings=1 -f "$STRIP" "$SRCDIR/system/settings.cpp" > "$tmp/settings.cpp"

# The loops, which are all in the one function that loads the settings.
awk -F'\t' -v defines="$tmp/defines" '
	function classify(d,   t) {
		gsub(/^[ \t]+|[ \t]+$/, "", d)
		if (d == "") return "none\t"
		if (d ~ /^-?[0-9]+$/) return "int\t" d
		if (d == "true") return "int\t1"
		if (d == "false") return "int\t0"
		if (d ~ /^"[^"]*"$/) { t = substr(d, 2, length(d) - 2); return "str\t" t }
		if (d in def) return "int\t" def[d]
		return "expr\t" d
	}
	FILENAME == defines { def[$1] = $2; next }
	{
		line = $0
		if (match(line, /sprintf\(cfg_key,[ \t]*"[^"]*"/)) {
			f = substr(line, RSTART, RLENGTH)
			sub(/^[^"]*"/, "", f); sub(/"$/, "", f)
			fmt = f
			next
		}
		if (match(line, /g_settings\.[A-Za-z_0-9]+\[i\][ \t]*=[ \t]*configfile\.get(Int32|Int64|Bool|String)\(cfg_key,/)) {
			head = substr(line, RSTART, RLENGTH); rest = substr(line, RSTART + RLENGTH)
			member = head; sub(/^g_settings\./, "", member); sub(/\[.*$/, "", member)
			sub(/\)[ \t]*;[ \t]*$/, "", rest)
			if (fmt != "") print "L\t" member "\t" fmt "\t" classify(rest)
			next
		}
		if (match(line, /setSettingsText\([ \t]*g_settings\.[A-Za-z_0-9]+\[i\],[ \t]*configfile\.getString\(cfg_key,/)) {
			head = substr(line, RSTART, RLENGTH); rest = substr(line, RSTART + RLENGTH)
			member = head; sub(/^setSettingsText\([ \t]*g_settings\./, "", member); sub(/\[.*$/, "", member)
			sub(/\)\)[ \t]*;[ \t]*$/, "", rest)
			if (fmt != "") print "L\t" member "\t" fmt "\t" classify(rest)
			next
		}
	}
' "$tmp/defines" "$tmp/neutrino" | sort -u > "$tmp/loops"

# The enumerators of an enumeration in the order it declares them, each with the
# condition it sits behind. The last, which counts the others, is not an entry.
enumerators() {
	awk -v name="$1" '
		$0 ~ ("enum[ \t]+" name "([ \t]|$|/)") { found = 1; next }
		found && !body { if ($0 ~ /\{/) body = 1; next }
		!body { next }
		/^[ \t]*#[ \t]*(if|ifdef)[ \t]/ { c = $0; sub(/^[ \t]*#[ \t]*(if|ifdef)[ \t]+/, "", c); gsub(/[ \t]+$/, "", c); guard[++depth] = c; next }
		/^[ \t]*#[ \t]*endif/ { depth--; next }
		/^[ \t]*#/ { next }
		/\}/ { exit }
		{
			e = $0; gsub(/^[ \t]+|[ \t]*(=[^,]*)?,?[ \t]*$/, "", e)
			if (e ~ /^[A-Z][A-Z_0-9]*$/ && e !~ /(_MAX|_COUNT)$/) print e "\t" (depth > 0 ? guard[depth] : "-")
		}
	' "$tmp/settings.h"
}

# The names a locale enumerator stands for: the name in the catalog written in capitals
# with an underscore for each dot.
awk '{ n = toupper($0); gsub(/\./, "_", n); print "LOCALE_" n "\t" $0 }' "$LOCALES" | sort -u > "$tmp/localenames"

# A table of entries in the order of an enumeration, one line for each entry.
#   $1 member, $2 the enumeration, $3 the file, $4 how an entry is read:
#   key "name", default   |   timing { default, LOCALE_X, ...
table() {
	member="$1"; enum="$2"; file="$3"; mode="$4"; start="$5"
	enumerators "$enum" > "$tmp/enum.$member"
	awk -v start="$start" -v mode="$mode" '
		$0 ~ start { found = 1; next }
		found && !body { if ($0 ~ /\{/) body = 1; if (!body) next }
		!body { next }
		/^[ \t]*#/ { next }
		/^[ \t]*\};/ { exit }
		{
			if (mode == "key") {
				if (match($0, /\{[ \t]*"[a-z_0-9]+"[ \t]*,[ \t]*[^}]*\}/)) {
					e = substr($0, RSTART + 1, RLENGTH - 2)
					key = e; sub(/^[ \t]*"/, "", key); sub(/".*$/, "", key)
					d = e; sub(/^[^,]*,/, "", d)
					print key "\t" d
				}
			} else {
				if (match($0, /\{[ \t]*-?[0-9]+[ \t]*,[ \t]*LOCALE_[A-Z0-9_]+/)) {
					e = substr($0, RSTART + 1, RLENGTH - 1)
					d = e; sub(/,.*$/, "", d)
					l = e; sub(/^[^,]*,[ \t]*/, "", l)
					print l "\t" d
				}
			}
		}
	' "$file" > "$tmp/entries.$member"
	n1=`awk 'END { print NR }' "$tmp/enum.$member"`
	n2=`awk 'END { print NR }' "$tmp/entries.$member"`
	if [ "$n1" -ne "$n2" ]; then
		echo "extract-elements.sh: $member has $n1 enumerators and $n2 table entries, the two have to agree" >&2
		exit 1
	fi
	paste "$tmp/enum.$member" "$tmp/entries.$member"
}

: > "$tmp/tables"
table personalize PERSONALIZE_SETTINGS "$tmp/settings.cpp" key 'personalize_settings\[SNeutrinoSettings::P_SETTINGS_MAX\][ \t]*=' \
	| awk -F'\t' '{ print "personalize\t" $1 "\t" $3 "\t" $4 "\t" $2 }' >> "$tmp/tables"
table lcd_setting LCD_SETTINGS "$tmp/neutrino" key 'lcd_setting\[SNeutrinoSettings::LCD_SETTING_COUNT\][ \t]*=' \
	| awk -F'\t' '{ print "lcd_setting\t" $1 "\t" $3 "\t" $4 "\t" $2 }' >> "$tmp/tables"
table timing TIMING_SETTINGS "$tmp/settings.h" locale 'timing_setting\[SNeutrinoSettings::TIMING_SETTING_COUNT\][ \t]*=' \
	| awk -F'\t' -v names="$tmp/localenames" 'BEGIN { while ((getline l < names) > 0) { split(l, a, "\t"); real[a[1]] = a[2] } } { print "timing\t" $1 "\t" real[$3] "\t" $4 "\t" $2 }' >> "$tmp/tables"
table handling_infobar HANDLING_INFOBAR_SETTINGS "$tmp/settings.h" locale 'handling_infobar_setting\[SNeutrinoSettings::HANDLING_INFOBAR_SETTING_COUNT\][ \t]*=' \
	| awk -F'\t' -v names="$tmp/localenames" 'BEGIN { while ((getline l < names) > 0) { split(l, a, "\t"); real[a[1]] = a[2] } } { print "handling_infobar\t" $1 "\t" real[$3] "\t" $4 "\t" $2 }' >> "$tmp/tables"

# member, enumerator, key, raw default, guard: the default classified as a loop's is.
awk -F'\t' -v defines="$tmp/defines" '
	function classify(d,   t) {
		gsub(/^[ \t]+|[ \t]+$/, "", d)
		if (d == "") return "none\t"
		if (d ~ /^-?[0-9]+$/) return "int\t" d
		if (d == "true") return "int\t1"
		if (d == "false") return "int\t0"
		if (d ~ /^"[^"]*"$/) { t = substr(d, 2, length(d) - 2); return "str\t" t }
		if (d in def) return "int\t" def[d]
		return "expr\t" d
	}
	FILENAME == defines { def[$1] = $2; next }
	{
		if ($3 == "") { print "extract-elements.sh: no key for " $1 " " $2 > "/dev/stderr"; bad = 1 }
		print "T\t" $1 "\t" $2 "\t" $3 "\t" classify($4) "\t" $5
	}
	END { if (bad) exit 1 }
' "$tmp/defines" "$tmp/tables" > "$tmp/entries"

loops=`awk 'END { print NR }' "$tmp/loops"`
entries=`awk 'END { print NR }' "$tmp/entries"`
if [ "$loops" -lt "$FLOOR_LOOPS" ]; then
	echo "extract-elements.sh: $loops loops is below the floor of $FLOOR_LOOPS, the scan has stopped matching" >&2
	exit 1
fi
if [ "$entries" -lt "$FLOOR_ENTRIES" ]; then
	echo "extract-elements.sh: $entries table entries is below the floor of $FLOOR_ENTRIES, the scan has stopped matching" >&2
	exit 1
fi

cat "$tmp/loops" "$tmp/entries"

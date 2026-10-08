#!/bin/sh
# The values a setting is offered under, as the screen that offers it states them.
# CMenuOptionChooser takes its label, the value it edits and a table of choices
# (src/gui/widget/menue.h), and that table is what a declared choice has to agree with:
# a value the screen does not offer, one it offers that the declaration leaves out, and
# a value paired with the wrong words are all invisible to every other check.
#
# One row per call site, tab between: field, label key, where it was read, the table it
# names, and the entries of that table.
#
# A site built from the declaration (addSetting) has DERIVED in the table column:
# there is nothing on the screen to compare. In place of the entries it says where
# the row behind the key stands in the source, as told where the DERIVED rows are
# written below.
#
# An entry is written as value=label, with ! after one a preprocessor arm inside the
# table gates: a choice only one box model is given is one a declaration may leave out
# while an ungated one is not. A value or a label no scan can resolve is written as ?,
# and an empty entry list is a table this cannot read at all.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] || { echo "usage: extract-choices.sh <source directory> [more directories]" >&2; exit 2; }

# Below this the scan is not reading the screens any more. The tree held 210 call
# sites, 192 of them naming a table whose every entry resolved, over 674 entries in 129
# tables when this was written, and 313 of those entries stated their value under a
# name rather than as a number.
#
# The named ones are counted apart from the rest because the site count barely moves
# without them: two thirds of the entries state a plain number.
#
# Each DERIVED site counts the entries of the row behind it toward both floors,
# once per site, as a table read at that site would have counted: an entry the
# row states under a name counts as named.
FLOOR_SITES=190
FLOOR_RESOLVED=170
FLOOR_ENTRIES=600
FLOOR_NAMED=250

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

MARK='@@file@@'

# SCAN_EXTRA names one more directory to read, which is how the self test in the
# Makefile feeds a fixture through the same scan as the tree.
find "$@" ${SCAN_EXTRA:+"$SCAN_EXTRA"} -name '*.cpp' -o -name '*.h' | LC_ALL=C sort > "$tmp/files"
xargs awk -v keepstrings=0 -v mark="$MARK" -f "$STRIP" < "$tmp/files" > "$tmp/code"
xargs awk -v keepstrings=1 -v markspan=1 -v mark="$MARK" -f "$STRIP" < "$tmp/files" \
	| awk -v mark="$MARK" -f "$HERE/addsetting.awk" | sort -u > "$tmp/derived"
awk -v mark="$MARK" -f "$HERE/keyvals.awk" "$tmp/code" > "$tmp/entries"
awk -v mark="$MARK" -f "$HERE/choices.awk" "$tmp/code" > "$tmp/sites"

[ -s "$tmp/entries" ] || { echo "extract-choices.sh: no table of choices was read at all" >&2; exit 1; }
[ -s "$tmp/sites" ] || { echo "extract-choices.sh: no call site was read at all" >&2; exit 1; }

# The names a value can be written under, and the words a label stands for. Both
# maps are the ones the scans beside this read, so a name resolves here to what
# it resolves to there.
sh "$HERE/extract-constants.sh" "$@" > "$tmp/names"
sh "$HERE/extract-bounds.sh" -m "$SRC" > "$tmp/locale"

# A site built from the declaration takes its entries from the row behind it: one
# per value of the row's list, two for a flag. Key and value, one line per entry.
# Each table file behind its own mark, so no state of one carries into the next.
find "$@" ${SCAN_EXTRA:+"$SCAN_EXTRA"} -name 'settingstable*.cpp' | LC_ALL=C sort > "$tmp/tablefiles"
while read -r f; do
	echo "$MARK$f"
	awk -v keepstrings=1 -v markspan=1 -f "$STRIP" "$f" | awk -f "$HERE/blank-if0.awk"
done < "$tmp/tablefiles" > "$tmp/tablesource"
awk -v mark="$MARK" -f "$HERE/rows.awk" "$tmp/tablesource" > "$tmp/rows"

# Every row the table source writes has to come out of the row scan: a row it
# did not read has no arms and no list there, and nothing downstream would say
# so. Counted on the same text, so a row under #if 0 is in neither number.
written=`grep -oE '(^|[^A-Za-z0-9_])(bool|int|enum|text|key|color|list|records)Row\(' "$tmp/tablesource" | wc -l`
read_rows=`grep -c '^R	' "$tmp/rows"` || read_rows=0
if [ "$written" -ne "$read_rows" ]; then
	echo "extract-choices.sh: the tables write $written rows and the row scan read $read_rows, it has stopped matching a row" >&2
	exit 1
fi
awk -F'	' -v rowfile="$tmp/rows" '
	FILENAME == rowfile {
		if ($1 == "R") { type[$2] = $3; tab[$2] = $4 }
		else { n[$2]++; val[$2, n[$2]] = $3 }
		next
	}
	{
		k = $1
		if (type[k] == "Bool") { print k "\t0"; print k "\t1"; next }
		for (i = 1; i <= n[tab[k]]; i++) print k "\t" val[tab[k], i]
	}
' "$tmp/rows" "$tmp/derived" > "$tmp/rowentries"

entries=`cat "$tmp/entries" "$tmp/rowentries" | wc -l`
if [ "$entries" -lt "$FLOOR_ENTRIES" ]; then
	echo "extract-choices.sh: $entries entries is below the floor of $FLOOR_ENTRIES, the scan has stopped matching" >&2
	exit 1
fi

# One line per table and call site, its entries in order, joined. An entry the
# scan could not read is left in as a question mark rather than dropped, so a
# table it read in part is not mistaken for one it read whole.
#
# The element on the left of an assignment is written before the right is read
# in some awks, so nothing here asks whether it is there from inside its own
# assignment.
awk -F'	' -v namefile="$tmp/names" -v locfile="$tmp/locale" '
	FILENAME == namefile { num[$1] = $2; next }
	FILENAME == locfile { loc[$1] = $2; next }
	{
		e = resolve($2) "=" (($3 in loc) ? loc[$3] : "?") ($4 == 1 ? "!" : "")
		site = $1 "\t" $5
		if (site in text)
			text[site] = text[site] "," e
		else {
			text[site] = e
			order[++n] = site
		}
	}
	END {
		for (i = 1; i <= n; i++) {
			split(order[i], p, "\t")
			print p[1] "\t" text[order[i]]
		}
	}
	# a name qualified by the type that declares it stands for the same value as
	# the bare one, and a value two declarations disagree on is in neither map
	function resolve(x,   b) {
		if (x ~ /^-?[0-9]+$/) return x + 0
		if (x ~ /^-?0[xX][0-9a-fA-F]+$/) return hex(x)
		if (x == "true") return 1
		if (x == "false") return 0
		b = x
		sub(/^.*::/, "", b)
		return (b in num) ? num[b] : "?"
	}
	function hex(x,   d, r, i, c, s, neg) {
		neg = (substr(x, 1, 1) == "-")
		s = tolower(x)
		sub(/^-/, "", s)
		s = substr(s, 3)
		d = "0123456789abcdef"
		r = 0
		for (i = 1; i <= length(s); i++) { c = substr(s, i, 1); r = r * 16 + index(d, c) - 1 }
		return neg ? -r : r
	}
' "$tmp/names" "$tmp/locale" "$tmp/entries" | sort > "$tmp/tables.raw"

# A name two files state with the same entries in the same order is one table;
# stated with different entries it is two, and neither of them answers for a
# call site that names it.
awk -F'	' '
	{
		if ($1 in text) {
			if (text[$1] != $2) text[$1] = ""
		} else {
			text[$1] = $2
			order[++n] = $1
		}
	}
	END { for (i = 1; i <= n; i++) print order[i] "\t" text[order[i]] }
' "$tmp/tables.raw" | sort > "$tmp/tables"

sort -u "$tmp/sites" \
	| awk -F'	' -v tabfile="$tmp/tables" -v locfile="$tmp/locale" '
		FILENAME == tabfile { text[$1] = $2; next }
		FILENAME == locfile { loc[$1] = $2; next }
		{ print $1 "\t" (($2 in loc) ? loc[$2] : "?") "\t" $4 "\t" $3 "\t" (($3 in text) ? text[$3] : "") }
	' "$tmp/tables" "$tmp/locale" - > "$tmp/out"

# A site built from the declaration states no table, so it is a row marked
# DERIVED where the table would be. It is counted as a site below: a screen moved
# over to the declaration is still a screen offering the setting.
#
# A screen file the build compiles only under an automake conditional is left out
# with it. One line per such file: the file and the conditional, as a !-prefixed
# name in an else.
cut -f2 "$tmp/derived" | sed 's/:[0-9]*$//' | sort -u > "$tmp/derivedfiles"
while read -r f; do
	am="`dirname "$f"`/Makefile.am"
	[ -r "$am" ] || continue
	b=`basename "$f"`
	awk -v base="$b" -v file="$f" '
		/^if[ \t]/ { c = $2; stack[++n] = c; next }
		/^else([ \t]|$)/ { if (n > 0) stack[n] = (substr(stack[n], 1, 1) == "!") ? substr(stack[n], 2) : "!" stack[n]; next }
		/^endif([ \t]|$)/ { if (n > 0) n--; next }
		{
			for (i = 1; i <= NF; i++) if ($i == base) {
				g = ""
				for (k = 1; k <= n; k++) g = g (g == "" ? "" : " && ") stack[k]
				seen++
				if (seen == 1) first = g
				else if (g != first) mixed = 1
			}
		}
		# listed once, or every time under the same conditionals: those are its own.
		# Listed also bare or under others, it has none.
		END { if (seen > 0 && !mixed && first != "") print file "\t" first }
	' "$am"
done < "$tmp/derivedfiles" > "$tmp/amconds"

# Column five of a DERIVED row: - when the row stands in no preprocessor arm, ? when
# no table file declares the key (a row under #if 0 is none), "arm" and the row's
# arms when the site is left out under every one of them too, by arms of its own or
# by the conditional its file is built under, and "row" and the row's arms when the
# site is not: a build without the row then still builds the site, which finds no
# row. Arms are compared as the conjuncts && joins, a bare name and defined() of it
# being one.
#
# Only an && at the top level of a guard splits it, and a parenthesised group is
# opened only when nothing at its own top level is an ||: a negated group, an else
# or an #elif chain stays one conjunct, which matches nothing in a row's arms. What
# cannot be read that way reads as "row", never as "arm".
awk -F'	' -v rowfile="$tmp/rows" -v amfile="$tmp/amconds" '
	function norm(c) {
		gsub(/[ \t]/, "", c)
		while (c ~ /^\([A-Za-z_][A-Za-z_0-9]*\)$/ || c ~ /^\(defined\([A-Za-z_][A-Za-z_0-9]*\)\)$/) c = substr(c, 2, length(c) - 2)
		if (c ~ /^!?defined\([A-Za-z_][A-Za-z_0-9]*\)$/) { sub(/defined\(/, "", c); sub(/\)$/, "", c) }
		if (c ~ /^!\([A-Za-z_][A-Za-z_0-9]*\)$/) { sub(/\(/, "", c); sub(/\)$/, "", c) }
		return c
	}
	# The guard cut at each && outside parentheses into P[1..n], or 0 when the
	# parentheses do not balance or an || stands outside them.
	function toplevel(g,   i, c, d, cur, n) {
		d = 0; cur = ""; n = 0
		for (i = 1; i <= length(g); i++) {
			c = substr(g, i, 1)
			if (c == "(") d++
			if (c == ")") { d--; if (d < 0) return 0 }
			if (d == 0 && substr(g, i, 2) == "||") return 0
			if (d == 0 && substr(g, i, 2) == "&&") { P[++n] = cur; cur = ""; i++; continue }
			cur = cur c
		}
		if (d != 0) return 0
		P[++n] = cur
		return n
	}
	# Whether the parenthesis opening g closes at its end.
	function wrapped(g,   i, d) {
		if (substr(g, 1, 1) != "(") return 0
		d = 0
		for (i = 1; i <= length(g); i++) {
			if (substr(g, i, 1) == "(") d++
			if (substr(g, i, 1) == ")") { d--; if (d == 0) return i == length(g) }
		}
		return 0
	}
	function conjuncts(g, set,   n, i, parts) {
		gsub(/[ \t]/, "", g)
		if (g == "-" || g == "") return
		while (wrapped(g) && toplevel(substr(g, 2, length(g) - 2)) > 0) g = substr(g, 2, length(g) - 2)
		n = toplevel(g)
		if (n < 2) { set[norm(g)] = 1; return }
		for (i = 1; i <= n; i++) parts[i] = P[i]
		for (i = 1; i <= n; i++) conjuncts(parts[i], set)
	}
	FILENAME == rowfile { if ($1 == "R") guard[$2] = $5; next }
	FILENAME == amfile { am[$1] = $2; next }
	{
		k = $1
		if (!(k in guard)) { col = "?" }
		else if (guard[k] == "-") { col = "-" }
		else {
			f = $2; sub(/:[0-9]+$/, "", f)
			delete site; delete row
			conjuncts($3, site)
			if (f in am) conjuncts(am[f], site)
			conjuncts(guard[k], row)
			covered = 1
			for (c in row) if (!(c in site)) covered = 0
			col = (covered ? "arm " : "row ") guard[k]
		}
		print k "\t?\t" $2 "\tDERIVED\t" col
	}
' "$tmp/rows" "$tmp/amconds" "$tmp/derived" >> "$tmp/out"
derived=`wc -l < "$tmp/derived"`

sites=`wc -l < "$tmp/out"`
if [ "$sites" -lt "$FLOOR_SITES" ]; then
	echo "extract-choices.sh: $sites call sites is below the floor of $FLOOR_SITES, the scan has stopped matching" >&2
	exit 1
fi

# A site whose every entry resolved, counted on its own: a change that left the
# sites standing and turned every value into a question mark would otherwise
# pass while checking none of them.
resolved=`awk -F'	' '$4 == "DERIVED" || ($5 != "" && $5 !~ /(^|[,=])\?/)' "$tmp/out" | wc -l`
if [ "$resolved" -lt "$FLOOR_RESOLVED" ]; then
	echo "extract-choices.sh: $resolved call sites with every entry resolved is below the floor of $FLOOR_RESOLVED," >&2
	echo "  the scan has stopped reading them; $sites sites ($derived derived) and $entries entries were matched" >&2
	exit 1
fi

named=`cat "$tmp/entries" "$tmp/rowentries" \
	| awk -F'	' '$2 !~ /^-?([0-9]+|0[xX][0-9a-fA-F]+)$/ && $2 != "true" && $2 != "false"' \
	| awk -F'	' -v namefile="$tmp/names" '
		FILENAME == namefile { num[$1] = 1; next }
		{ b = $2; sub(/^.*::/, "", b); if (b in num) n++ }
		END { print n + 0 }
	' "$tmp/names" -`
if [ "$named" -lt "$FLOOR_NAMED" ]; then
	echo "extract-choices.sh: $named entries stating their value under a name resolved, below the floor of $FLOOR_NAMED;" >&2
	echo "  the scan that reads those names has stopped matching" >&2
	exit 1
fi

cat "$tmp/out"

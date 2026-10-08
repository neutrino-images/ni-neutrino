#!/bin/sh
# What the coverage of the settings is measured against, recomputed here from the
# settings struct rather than read from a number written down. A field added to the
# struct otherwise moves the denominator and nothing says so, which leaves a criterion
# reporting complete because nothing was compared.
#
# Three sets, and the whole of the check is that the first is exactly the other two
# together:
#   every member of SNeutrinoSettings a row could carry, those of the two structs
#   inside it and every list or array among them,
#   every field the declaration tables name,
#   every field listed beside them as left out on purpose.
#
# A list, an array or a nested struct is accounted for by a row naming it: one to each
# element for an array, one for a list, and one to each member of the two structs.
#
# Names and not counts, so a field that changed sides shows as the field it is rather
# than as an arithmetic that still adds up. The parse of the struct is held to the same
# three sets: one that stopped finding members would drop names the other two still
# carry, and that fails here.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-denominator.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
SETTINGS="$SRC/src/system/settings.h"
UNDECLARED="$SRC/src/coreapi/settings/settingsundeclared.cpp"
for f in "$SETTINGS" "$UNDECLARED" "$HERE/fields.awk" "$HERE/strip-comments.awk"; do
	[ -r "$f" ] || { echo "check-denominator.sh: cannot read $f" >&2; exit 1; }
done

# Below these the scan is not reading the struct or the tables any more. The
# tree held 652 members with the two structs inside it, and 416 declared rows when
# this was written.
MEMBER_FLOOR=400
DECLARED_FLOOR=300

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# The struct and the two structs inside it, a member of either being a setting like
# any other: read apart, and named with the member that holds the struct so that a
# row can say which.
awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$SETTINGS" > "$tmp/settings.stripped"
{
	awk -f "$HERE/fields.awk" "$tmp/settings.stripped"
	awk -v struct=SNeutrinoTheme -v qual=theme. -f "$HERE/fields.awk" "$tmp/settings.stripped"
	awk -v struct=SNeutrinoGlcdTheme -v qual=glcd_theme. -f "$HERE/fields.awk" "$tmp/settings.stripped"
} > "$tmp/members"

members=`awk 'END { print NR }' "$tmp/members"`
if [ "$members" -lt "$MEMBER_FLOOR" ]; then
	echo "check-denominator.sh: $members members read out of $SETTINGS," >&2
	echo "  the scan has stopped matching" >&2
	exit 1
fi

# What a row, or an entry of the list, has to account for: every scalar, and every
# list, array and nested block. The two structs are not among them, because what is
# inside them is what is counted. glcd_theme exists on some builds only, so its
# members are not what every build has.
#
# The bytes a colour is made of are covered by the one row the colour has, which names
# the colour and not each byte, so no row names them. A colour group is a member
# prefix that has a red, a green and a blue member in its struct, read from the struct
# and not from a list written here. Each group has to have exactly one colour row in
# the tables, and each colour row has to name such a group, so a colour added to the
# struct without a row, or a row for a group that is none, fails here. The bytes are
# exempt by group, never by a pattern on their ending.
awk -F'\t' '
	{ seen[$1] = 1 }
	END {
		for (n in seen) {
			if (n !~ /_red$/)
				continue
			base = n; sub(/_red$/, "", base)
			if ((base "_green") in seen && (base "_blue") in seen)
				print base
		}
	}
' "$tmp/members" | sort > "$tmp/colour-groups"
n_groups=`awk 'END { print NR }' "$tmp/colour-groups"`
if [ "$n_groups" -lt 20 ]; then
	echo "check-denominator.sh: only $n_groups colour groups read out of the structs, the scan has stopped matching" >&2
	exit 1
fi
grep -hoE 'COREAPI_COLOR_FIELD\([A-Za-z_]+, *[A-Za-z_0-9]+' "$SRC"/src/coreapi/settings/settingstable*.cpp \
	| sed -E 's/.*\(([A-Za-z_]+), *([A-Za-z_0-9]+)/\1.\2/' | sort > "$tmp/colour-rows"
if ! cmp -s "$tmp/colour-groups" "$tmp/colour-rows"; then
	echo "check-denominator.sh: the colour groups of the structs and the colour rows of the tables differ:" >&2
	diff "$tmp/colour-groups" "$tmp/colour-rows" >&2 || true
	exit 1
fi
awk -F'\t' -v groups="$tmp/colour-groups" '
	FILENAME == groups { g[$1] = 1; next }
	{
		name = $1
		if (name ~ /_(red|green|blue|alpha)$/) {
			base = name; sub(/_(red|green|blue|alpha)$/, "", base)
			if (base in g) next
		}
		if ($2 == "scalar" || ($2 == "aggregate" && $4 != "a struct of its own")) print name
	}
' "$tmp/colour-groups" "$tmp/members" | sort > "$tmp/carriable"
awk -F'\t' -v groups="$tmp/colour-groups" '
	FILENAME == groups { g[$1] = 1; next }
	{
		name = $1
		if (name ~ /_(red|green|blue|alpha)$/) {
			base = name; sub(/_(red|green|blue|alpha)$/, "", base)
			if (base in g) next
		}
		if (($2 == "scalar" || ($2 == "aggregate" && $4 != "a struct of its own")) && $3 == "plain" && name !~ /^glcd_theme\./) print name
	}
' "$tmp/colour-groups" "$tmp/members" | sort > "$tmp/unconditional"

# Every macro a row writes its field with, named one by one rather than matched
# by a pattern wide enough to take whatever is added next: a row written with a
# macro nothing here names would leave the member it carries unaccounted for,
# and this is the one place that would say so. The member a row names is always
# the first argument, whatever else the macro takes after it.
FIELD_MACROS='COREAPI_(NUMBER_FIELD_ON|NUMBER_FIELD|TEXT_FIELD_ON|TEXT_FIELD|MASK_BIT_FIELD|CHANNEL_ID_FIELD|SERVICE_FIELD|ELEMENT_FIELD|ELEMENT_TEXT_FIELD|ELEMENT_CHANNEL_ID_FIELD|LIST_FIELD|RECORDS_FIELD|AGGREGATE_FIELD)'
THEME_MACROS='COREAPI_THEME_FIELD'
GLCD_MACROS='COREAPI_(GLCD_THEME_FIELD|GLCD_THEME_TEXT_FIELD)'

# The members a file's rows point at: the plain ones, and the members of the two
# structs, each under the name of the member that holds it.
fieldsOf() {
	{
		grep -hoE "$FIELD_MACROS"'\([A-Za-z_][A-Za-z_0-9]*[,)]' "$@" \
			| sed -E 's/.*\(([A-Za-z_0-9]*)[,)]$/\1/'
		grep -hoE "$THEME_MACROS"'\([A-Za-z_][A-Za-z_0-9]*[,)]' "$@" \
			| sed -E 's/.*\(([A-Za-z_0-9]*)[,)]$/theme.\1/'
		grep -hoE "$GLCD_MACROS"'\([A-Za-z_][A-Za-z_0-9]*[,)]' "$@" \
			| sed -E 's/.*\(([A-Za-z_0-9]*)[,)]$/glcd_theme.\1/'
	} || true
}

# The declared set is every field a row points at. The listed file is not among
# these, so the two sets are read apart.
fieldsOf "$SRC"/src/coreapi/settings/settingstable*.cpp | sort -u > "$tmp/declared"

fieldsOf "$UNDECLARED" | sort > "$tmp/listed.raw"
sort -u "$tmp/listed.raw" > "$tmp/listed"

carriable=`awk 'END { print NR }' "$tmp/carriable"`
declared=`awk 'END { print NR }' "$tmp/declared"`
listed=`awk 'END { print NR }' "$tmp/listed"`

if [ "$declared" -lt "$DECLARED_FLOOR" ]; then
	echo "check-denominator.sh: $declared declared fields found, the scan has stopped matching" >&2
	exit 1
fi
if [ "$listed" -lt 1 ]; then
	echo "check-denominator.sh: no listed field found in $UNDECLARED," >&2
	echo "  the scan has stopped matching" >&2
	exit 1
fi

fail=0

if [ "$listed" -ne "`awk 'END { print NR }' "$tmp/listed.raw"`" ]; then
	echo "a field is listed twice:" >&2
	sort "$tmp/listed.raw" | uniq -d | sed 's/^/  /' >&2
	fail=1
fi

both=`comm -12 "$tmp/declared" "$tmp/listed"`
if [ -n "$both" ]; then
	echo "declared by a row and listed as undeclared:" >&2
	printf '%s\n' "$both" | sed 's/^/  /' >&2
	fail=1
fi

# The listed file is compiled by this build alone, so a field it names that only
# some builds have would reach the others uncompiled.
conditional=`comm -23 "$tmp/listed" "$tmp/unconditional"`
if [ -n "$conditional" ]; then
	echo "listed as undeclared but not a member every build has:" >&2
	printf '%s\n' "$conditional" | sed 's/^/  /' >&2
	fail=1
fi

sort -u "$tmp/declared" "$tmp/listed" > "$tmp/accounted"

missing=`comm -23 "$tmp/carriable" "$tmp/accounted"`
if [ -n "$missing" ]; then
	echo "a member of SNeutrinoSettings, or of a struct in it, that no row declares and nothing lists:" >&2
	printf '%s\n' "$missing" | sed 's/^/  /' >&2
	fail=1
fi

stray=`comm -13 "$tmp/carriable" "$tmp/accounted"`
if [ -n "$stray" ]; then
	echo "named by a row or by the list and not a member of SNeutrinoSettings or of a struct in it:" >&2
	printf '%s\n' "$stray" | sed 's/^/  /' >&2
	fail=1
fi

if [ "$fail" -ne 0 ]; then
	echo "  the settings struct holds $carriable such members; $declared are declared and $listed are listed" >&2
	exit 1
fi

echo "settings the struct holds that a row could carry   $carriable"
echo "declared by a row                                  $declared"
echo "listed with a reason                               $listed"
exit 0

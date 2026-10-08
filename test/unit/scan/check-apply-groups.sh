#!/bin/sh
# Holds every setting that needs applying to an apply group.
#
# A setting that is read where it is used needs nothing when it changes. One that is
# read only by the code that makes a change take effect, a change notifier, an
# action branch of a setup screen's exec or the startup that runs once, is changed
# on a running box by a web write and then does nothing until the next start,
# unless an apply group names it. This reads the keys of the declared rows, the
# members they live in, every read of those members in the tree and the key lists
# of the groups, and fails on:
#   a key whose every read is an applying one and that has no group
#   a key in two groups, or twice in one
#   a group that names a key no row declares
#
# The key lists of a group are read as text, because the files they live in link
# against the drivers: a COREAPI_KEYS(list) in a group's initialiser names an array
# of strings in the same file. Comments are blanked first.
#
# A row that takes .needsRestart() is read only by the start that loads it and one
# that takes .readOutside() by a plugin, a script or a page the tree cannot see;
# neither needs a group, and the scan takes the row's word for it, so the word is
# the row's to keep true.
#
# A key the scan does not find itself but the hand reading in bugsV5/settings-eval
# did is in apply-eval-needs.txt. It needs a group like the rest, and stays on that
# file because the scan cannot tell such a key from one read where it is used.
#
# A group is written in src/coreapi/box/apply_*.cpp, as the shape in
# coreapi/base/apply.h says, and every ApplyGroup initialiser there has to be read:
# one that is not is an error even when the file holds others. A group
# registered or written anywhere else is an error too; registerApplyGroups is the
# only function that registers.
#
# usage: check-apply-groups.sh <top source directory> [eval list]
# Floors for what a run must have read are set by APPLY_FLOOR_* in the
# environment; a fixture sets them to 0.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-apply-groups.sh <top source directory> [eval list]" >&2
	exit 2
}

HERE=`dirname "$0"`
EVALNEEDS="${2:-$HERE/apply-eval-needs.txt}"
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
for f in "$STRIP" "$BLANK" "$HERE/applyrows.awk" "$HERE/applyreads.awk" "$HERE/applygroups.awk" "$HERE/applyregs.awk"; do
	[ -r "$f" ] || { echo "check-apply-groups.sh: cannot read $f" >&2; exit 1; }
done

# Below these the scan is not reading the tree any more. The applying reads fall as
# screens hand their effects to groups and are not expected to rise, so their floor
# sits about a tenth under what the finished tree holds (242 of them, beside 752 keys
# and 2266 reads): close enough that a pattern that stops matching a shape of screen
# is caught, far enough that taking one screen's notifier out is not. Set once, from
# the tree the settings series ends with; move it only with a measured figure.
FLOOR_KEYS="${APPLY_FLOOR_KEYS:-650}"
FLOOR_READS="${APPLY_FLOOR_READS:-2000}"
FLOOR_APPLY="${APPLY_FLOOR_APPLY:-218}"

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# A fixture tree without keys of this kind sets APPLY_EVAL_OPTIONAL; the tree never does.
if [ -r "$EVALNEEDS" ]; then
	{ grep -v '^[ 	]*#' "$EVALNEEDS" | grep -v '^[ 	]*$' || true; } | cut -f1 | sort -u > "$tmp/evalneeds"
elif [ -n "$APPLY_EVAL_OPTIONAL" ]; then
	: > "$tmp/evalneeds"
else
	echo "check-apply-groups.sh: cannot read $EVALNEEDS" >&2
	exit 1
fi

TABLES=`ls "$SRC"/src/coreapi/settings/settingstable*.cpp 2>/dev/null` || true
[ -n "$TABLES" ] || { echo "check-apply-groups.sh: no settings table under $SRC" >&2; exit 1; }

# key, member
awk -v keepstrings=1 -f "$STRIP" $TABLES | awk -f "$BLANK" | awk -f "$HERE/applyrows.awk" \
	| sort -u > "$tmp/rows"
awk -F'\t' '$2 == "?" { print $1 }' "$tmp/rows" > "$tmp/nomember"
if [ -s "$tmp/nomember" ]; then
	echo "check-apply-groups.sh: no field read out of the row of:" >&2
	sed 's/^/  /' "$tmp/nomember" >&2
	exit 1
fi
cut -f1 "$tmp/rows" | sort -u > "$tmp/declared"
# A row that says its value is read only by the start that loads it, or by something
# outside the tree, has nothing a group could run, so it needs none.
awk -F'\t' '$3 == "restart" || $3 == "outside" { print $1 }' "$tmp/rows" | sort -u > "$tmp/exempt"

# member, kind, file, function
find "$SRC/src" "$SRC/lib" \( -name '*.cpp' -o -name '*.h' \) 2>/dev/null | sort > "$tmp/files"
[ -s "$tmp/files" ] || { echo "check-apply-groups.sh: no source under $SRC" >&2; exit 1; }
xargs awk -v keepstrings=0 -v mark=@@file@@ -f "$STRIP" < "$tmp/files" \
	| awk -f "$BLANK" \
	| awk -v mark=@@file@@ -v root="$SRC/" -f "$HERE/applyreads.awk" > "$tmp/reads"

# key, apply reads, other reads. A member a row names with a trailing underscore
# is a prefix, the way a colour is.
awk -F'\t' '
	FILENAME == rowsfile {
		toks[$2] = toks[$2] " " $1
		if ($2 ~ /_$/ && $2 != "-") prefix[$2] = 1
		next
	}
	$2 == "skip" { next }
	{ seen[$1] = 1; cnt[$1 SUBSEP $2]++ }
	END {
		for (m in seen) {
			n = split(toks[m], ks, " ")
			for (i = 1; i <= n; i++) { a[ks[i]] += cnt[m SUBSEP "apply"]; u[ks[i]] += cnt[m SUBSEP "use"] }
			for (p in prefix) if (index(m, p) == 1) {
				n = split(toks[p], ks, " ")
				for (i = 1; i <= n; i++) { a[ks[i]] += cnt[m SUBSEP "apply"]; u[ks[i]] += cnt[m SUBSEP "use"] }
			}
		}
		for (k in a) print k "\t" a[k] "\t" u[k]
	}
' rowsfile="$tmp/rows" "$tmp/rows" "$tmp/reads" | sort > "$tmp/counts"

keys=`awk 'END { print NR }' "$tmp/declared"`
nreads=`awk -F'\t' '$2 != "skip"' "$tmp/reads" | awk 'END { print NR }'`
napply=`awk -F'\t' '$2 == "apply"' "$tmp/reads" | awk 'END { print NR }'`
for pair in "keys:$keys:$FLOOR_KEYS" "reads:$nreads:$FLOOR_READS" "applying reads:$napply:$FLOOR_APPLY"; do
	what=${pair%%:*}; rest=${pair#*:}; got=${rest%%:*}; floor=${rest#*:}
	if [ "$got" -lt "$floor" ]; then
		echo "check-apply-groups.sh: $got $what is below the floor of $floor, the scan has stopped matching" >&2
		exit 1
	fi
done

awk -F'\t' '$2 > 0 && $3 == 0 { print $1 }' "$tmp/counts" | sort > "$tmp/needs.scan"
sort -u "$tmp/needs.scan" "$tmp/evalneeds" | comm -23 - "$tmp/exempt" > "$tmp/needs"

# The groups: the name and the array a group's initialiser hands COREAPI_KEYS, then
# the strings of that array.
GROUPS_FILES=`ls "$SRC"/src/coreapi/box/apply_*.cpp 2>/dev/null` || true
: > "$tmp/grouped"
for f in $GROUPS_FILES; do
	awk -v keepstrings=1 -f "$STRIP" "$f" | awk -f "$BLANK" > "$tmp/g.src"
	awk -v file="$f" -f "$HERE/applygroups.awk" "$tmp/g.src" >> "$tmp/grouped"
done
if grep -q '^ERR	' "$tmp/grouped"; then
	echo "check-apply-groups.sh: a group cannot be read, so its keys would count as none:" >&2
	grep '^ERR	' "$tmp/grouped" | cut -f2- | sed 's/^/  /' >&2
	exit 1
fi
# a file named apply_*.cpp that yields no key is a list this has misread
for f in $GROUPS_FILES; do
	if ! awk -F'\t' -v f="$f" '$3 == f { n++ } END { exit n > 0 ? 0 : 1 }' "$tmp/grouped"; then
		echo "check-apply-groups.sh: no key read out of any group of $f" >&2
		exit 1
	fi
done

fail=0
report()
{
	echo "$1" >&2
	sed 's/^/  /' "$2" >&2
	fail=1
}

# key in two groups, or twice in one
cut -f2 "$tmp/grouped" | sort | uniq -d > "$tmp/twice"
if [ -s "$tmp/twice" ]; then
	: > "$tmp/twice-where"
	while read -r k; do
		printf '%s in %s\n' "$k" "`awk -F'\t' -v k="$k" '$2 == k { printf "%s%s", (n++ ? ", " : ""), $1 }' "$tmp/grouped"`" >> "$tmp/twice-where"
	done < "$tmp/twice"
	report "a key is in two groups, so one change would run two:" "$tmp/twice-where"
fi

cut -f2 "$tmp/grouped" | sort -u > "$tmp/in-group"

comm -13 "$tmp/declared" "$tmp/in-group" > "$tmp/undeclared"
[ -s "$tmp/undeclared" ] && report "a group names a key no row declares:" "$tmp/undeclared"

# groups written or registered outside the files that hold them
xargs awk -v keepstrings=0 -v mark=@@file@@ -f "$STRIP" < "$tmp/files" | awk -f "$BLANK" \
	| awk -v mark=@@file@@ -v root="$SRC/" -f "$HERE/applyregs.awk" > "$tmp/outside"
[ -s "$tmp/outside" ] && report "a group is registered or written outside src/coreapi/box/apply_*.cpp, where the scan does not read it; register from registerApplyGroups only:" "$tmp/outside"

comm -13 "$tmp/declared" "$tmp/evalneeds" > "$tmp/eval-undeclared"
[ -s "$tmp/eval-undeclared" ] && report "apply-eval-needs.txt names a key no row declares:" "$tmp/eval-undeclared"

comm -23 "$tmp/needs" "$tmp/in-group" > "$tmp/missing"
[ -s "$tmp/missing" ] && report "a setting needs an apply group, since only a change notifier, an action branch or startup reads it, and has none:" "$tmp/missing"

[ "$fail" -eq 0 ] || exit 1

needs=`awk 'END { print NR }' "$tmp/needs"`
groups=`cut -f1 "$tmp/grouped" | sort -u | awk 'END { print NR }'`
echo "declared keys                                      $keys"
echo "settings reads outside persistence                 $nreads, $napply of them applying"
echo "keys read only by what applies                     $needs"
echo "apply groups                                       $groups"
echo "keys in a group                                    `awk 'END { print NR }' "$tmp/in-group"`"
exit 0

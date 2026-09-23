#!/bin/sh
# What one part of the page hands another has to be what the other reads.
#
# Two scans, one rule, because the defect is the same both times: two sides of a
# handover that type check, pass their own tests, and no longer agree. There is no
# call between them, so nothing fails; there is only a screen quietly drawing less
# than it should.
#
# FIRST: the names that go into a query.
#
# The screens hand each other work through the address bar: the guide offers to
# record a programme and the timer form is supposed to open already filled in. The
# guide wrote duration, the form reads stop, and what came of it was a form with
# an empty end time under a button that said it would make a timer out of the
# programme. Both sides type check. Neither one is wrong on its own.
#
# So the agreement is held here. Every name a page builds into a query must be
# read by something: by a screen, which reads it off the address, or by the box,
# which states its query parameters in the document it writes about itself.
#
# Deliberately not the other direction. A form may read a name nobody sends yet,
# which is a form that can be reached two ways, and holding that to an identity
# would forbid the second way before it is built.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
BUILD="$2"
[ -n "$SRC" ] && [ -d "$SRC" ] && [ -n "$BUILD" ] || {
	echo "usage: check-web-handover.sh <top source directory> <build directory>" >&2
	exit 2
}

APP="$SRC/data/ni-web/app"
DOC="$BUILD/openapi.json"
STRIP="$SRC/test/unit/scan/strip-comments.awk"

[ -d "$APP" ] || { echo "check-web-handover.sh: cannot read $APP" >&2; exit 1; }
[ -r "$DOC" ] || { echo "check-web-handover.sh: cannot read $DOC" >&2; exit 1; }
[ -r "$STRIP" ] || { echo "check-web-handover.sh: cannot read $STRIP" >&2; exit 1; }

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# Comments go first. A name written down in prose, in the very sentence
# explaining why the old name was wrong, would otherwise count as one in use.
# keepstrings=1 because the names are inside the literals: a scan that blanked
# them would read a file full of queries as a file with none.
find "$APP" -type f -name '*.js' -print | LC_ALL=C sort > "$tmp/files"
xargs awk -v keepstrings=1 -f "$STRIP" < "$tmp/files" > "$tmp/stripped"

# What the pages put into a query, built a piece at a time the way an address is.
# The literal may carry the punctuation that puts the name where it goes, which
# is how a first name and a later one are spelt differently for the same job.
sed -n "s/.*'[?&]*\\([A-Za-z_][A-Za-z0-9_]*\\)=' *+ *encodeURIComponent.*/\\1/p" "$tmp/stripped" \
	| LC_ALL=C sort -u > "$tmp/sent"

# What a screen reads back off its own address.
sed -n "s/.*\\.get('\\([A-Za-z_][A-Za-z0-9_]*\\)').*/\\1/p" "$tmp/stripped" \
	| LC_ALL=C sort -u > "$tmp/read"

# What the box states it reads, taken out of the document the server writes
# rather than out of the routes, so a parameter that stops being declared stops
# counting here too.
tr ',' '\n' < "$DOC" \
	| sed -n 's/.*"name" *: *"\([A-Za-z_][A-Za-z0-9_]*\)".*/\1/p' \
	| LC_ALL=C sort -u > "$tmp/declared"

cat "$tmp/read" "$tmp/declared" | LC_ALL=C sort -u > "$tmp/known"

sent=`wc -l < "$tmp/sent" | tr -d ' '`
[ "$sent" -gt 0 ] || {
	echo "check-web-handover.sh: no page builds a query, and these screens are reached with them" >&2
	exit 1
}

LC_ALL=C comm -23 "$tmp/sent" "$tmp/known" > "$tmp/orphan"
if [ -s "$tmp/orphan" ]; then
	while read -r name; do
		echo "check-web-handover.sh: a page sends $name and nothing reads it" >&2
		grep -rn "'$name=' *+ *encodeURIComponent" "$APP" >&2 || true
	done < "$tmp/orphan"
	exit 1
fi

echo "check-web-handover.sh: $sent names go into a query, every one read by a screen or declared by the box"

# SECOND: the words a snapshot's phase is said in.
#
# The store names its loads and the shared widget draws them. The widget used to
# have words of its own, so six screens each wrote the translation between the
# two, one of them slightly wrong and one of them not at all: on that screen the
# reloading and writing marks simply never appeared. A word that matches nothing
# draws the content and no mark, which is why nothing failed.
#
# The declaration is the authority, because it is what the store is written
# against. The annotation alone is not a net: the pages are drawn through tagged
# templates, and what goes into one is not checked against the type of what
# receives it.
TYPES="$SRC/test/web/types/client.d.ts"
STATE="$APP/ui/state.js"
[ -r "$TYPES" ] || { echo "check-web-handover.sh: cannot read $TYPES" >&2; exit 1; }
[ -r "$STATE" ] || { echo "check-web-handover.sh: cannot read $STATE" >&2; exit 1; }

sed -n "s/.*type Phase *= *\(.*\);.*/\1/p" "$TYPES" \
	| tr '|' '\n' \
	| sed -n "s/.*'\([a-z]*\)'.*/\1/p" \
	| grep -v '^$' \
	| LC_ALL=C sort -u > "$tmp/phases"

phases=`wc -l < "$tmp/phases" | tr -d ' '`
[ "$phases" -gt 1 ] || {
	echo "check-web-handover.sh: $TYPES declares no Phase to hold anything to" >&2
	exit 1
}

# What the widget compares against, and what a screen writes out in full. Every
# occurrence and not the last one on its line: the line that decides whether to
# draw a mark at all names two of the three phases, and a scan that took one
# match per line read that line as naming only the second, which left the first
# unheld by exactly the check written to hold it.
{
	grep -o "props\.phase === '[a-z]*'" "$STATE" | sed "s/.*'\([a-z]*\)'/\1/"
	grep -o "phase=[\"'][a-z]*[\"']" "$tmp/stripped" | sed "s/phase=[\"']\([a-z]*\)[\"']/\1/"
} | grep -v '^$' | LC_ALL=C sort -u > "$tmp/spoken"

LC_ALL=C comm -23 "$tmp/spoken" "$tmp/phases" > "$tmp/invented"
if [ -s "$tmp/invented" ]; then
	while read -r word; do
		echo "check-web-handover.sh: $word is drawn as a phase and the store never says it" >&2
		grep -rn "phase[= ]*[=\"']*$word" "$STATE" "$APP" >&2 | head -5 || true
	done < "$tmp/invented"
	exit 1
fi

# And the other way, which is the half that was actually broken: a phase the
# store sets and nothing draws is a mark that never appears, and that is
# invisible from either side on its own.
LC_ALL=C comm -13 "$tmp/spoken" "$tmp/phases" > "$tmp/undrawn"
if [ -s "$tmp/undrawn" ]; then
	while read -r word; do
		echo "check-web-handover.sh: the store sets the phase $word and nothing draws it" >&2
	done < "$tmp/undrawn"
	exit 1
fi

# And a phase is handed on as it stands, never converted on the way.
#
# This is the hole the two scans above had. They read words where words are
# written: the literal a screen spells out, the literal the widget compares
# against. A screen that hands over phase=${drawPhase(shot.phase)} writes no word
# at all, so its translation, and the two phases it silently swallowed, were
# invisible to a check that reported every phase accounted for. Eight screens had
# written that translation and six were found.
#
# So the shape is held and not only the words: what goes to phase is a literal or
# a plain reference, and a call there is a translation whether or not it looks
# like one.
grep -rn 'phase=${[^}]*(' "$APP" --include='*.js' > "$tmp/converted" 2>/dev/null || true
if [ -s "$tmp/converted" ]; then
	echo "check-web-handover.sh: a phase is converted on its way to being drawn, and the words it is converted into are held by nothing" >&2
	cat "$tmp/converted" >&2
	echo "check-web-handover.sh: hand the phase over as it stands" >&2
	exit 1
fi

echo "check-web-handover.sh: the $phases phases the store declares are the $phases the page draws, each handed over unconverted"

# THIRD: the mark a condition of the box is drawn with.
#
# ui/dot.js knows three and draws a mark for each, and everything else on the page
# hands it one of the three. Two things are held here.
#
# The words, first: a mark this page spells and the dot does not know draws no
# mark at all, only the word beside it, which is a screen quietly losing the half
# of the pair that is seen from across the room.
#
# And then the half that had gone wrong. The recordings list is drawn by two
# screens and counted by a third, off one answer. What a row of it is has to be
# one answer too, and it is fmt.js. Before that it was worked out three times: the
# frame left a timeshift out of its count, the overview drew it as a warning and
# the recordings screen drew it as running, so the same row was orange on one
# screen and green on the next. So the field they disagreed about does not appear
# in a mark: a screen that reads the answer again to pick a colour is a screen
# that will disagree with the next one.
DOT="$APP/ui/dot.js"
[ -r "$DOT" ] || { echo "check-web-handover.sh: cannot read $DOT" >&2; exit 1; }

sed -n 's/^const MARKS = {\(.*\)};$/\1/p' "$DOT" \
	| tr ',' '\n' \
	| sed -n 's/^ *\([A-Za-z_][A-Za-z0-9_]*\) *:.*/\1/p' \
	| LC_ALL=C sort -u > "$tmp/marks"

marks=`wc -l < "$tmp/marks" | tr -d ' '`
[ "$marks" -gt 0 ] || {
	echo "check-web-handover.sh: $DOT names no mark to hold anything to" >&2
	exit 1
}

# Every handover, as it is written. A mark reaches the dot either spelled out
# here or out of a function that decides it, and both are followed: reading
# only the spelled ones is how the check next door went blind to a screen that
# hid its words behind a call.
grep -oE "kind=(\"[^\"]*\"|'[^']*'|\\\$\{[^}]*\})" "$tmp/stripped" > "$tmp/kinds" || true
[ -s "$tmp/kinds" ] || {
	echo "check-web-handover.sh: nothing hands a mark to a dot, and the screens draw them" >&2
	exit 1
}

# The deciders, by the name they are called under, and the words each of them
# answers with. Read out of the whole tree rather than out of one file, because
# a decider stands beside whatever it decides for.
awk '{
	while (match($0, /kind=\$\{[A-Za-z_][A-Za-z0-9_]*\(/)) {
		one = substr($0, RSTART, RLENGTH)
		sub(/^kind=\$\{/, "", one)
		sub(/\($/, "", one)
		print one
		$0 = substr($0, RSTART + RLENGTH)
	}
}' "$tmp/kinds" | LC_ALL=C sort -u > "$tmp/deciders"

# Every decider read out first and on its own, because a body that could not
# be found has to stop this rather than contribute nothing to it: a scan that
# answers nothing for the one function it could not read reports the tree clean
# for exactly the reason that it never read it.
: > "$tmp/answered"
while read -r name; do
	[ -n "$name" ] || continue
	awk -v want="$name" '
		index($0, "export function " want "(") == 1 { inside = 1 }
		inside { print }
		inside && /^\}/ { inside = 0 }
	' "$tmp/stripped" > "$tmp/body"
	[ -s "$tmp/body" ] || {
		echo "check-web-handover.sh: a mark is decided by $name and no exported function of that name stands in $APP" >&2
		exit 1
	}
	# What it answers with and not what it compares against. A decider reads
	# one vocabulary to answer in another, and the kinds of timer the one next
	# to the timer list tests for are not marks and never were.
	grep 'return' "$tmp/body" \
		| sed "s/[=!]== *'[^']*'//g; s/[=!]= *'[^']*'//g" \
		| grep -oE "'[^']*'" | sed "s/^.//; s/.$//" >> "$tmp/answered"
done < "$tmp/deciders"

{
	grep -oE "'[^']*'|\"[^\"]*\"" "$tmp/kinds" | sed "s/^.//; s/.$//"
	cat "$tmp/answered"
} | grep -v '^$' | LC_ALL=C sort -u > "$tmp/spokenmarks"

marksaid=`wc -l < "$tmp/spokenmarks" | tr -d ' '`
[ "$marksaid" -gt 0 ] || {
	echo "check-web-handover.sh: nothing was read out of a kind at all, so this held nothing" >&2
	exit 1
}

LC_ALL=C comm -23 "$tmp/spokenmarks" "$tmp/marks" > "$tmp/strange"
if [ -s "$tmp/strange" ]; then
	while read -r word; do
		echo "check-web-handover.sh: $word is handed to a dot as a mark and $DOT draws no such mark" >&2
		grep -rn "'$word'" "$APP" --include='*.js' >&2 | head -5 || true
	done < "$tmp/strange"
	exit 1
fi

grep -rn 'kind=${[^}]*timeshift' "$APP" --include='*.js' > "$tmp/reread" 2>/dev/null || true
if [ -s "$tmp/reread" ]; then
	echo "check-web-handover.sh: a screen works out for itself what mark a recording carries, and the screen next to it does the same and answers differently" >&2
	cat "$tmp/reread" >&2
	echo "check-web-handover.sh: hand over what fmt.js answers" >&2
	exit 1
fi

echo "check-web-handover.sh: the $marks marks the dot draws are the $marks anything hands it, and what a recording carries is answered once"

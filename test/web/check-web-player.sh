#!/bin/sh
# The source that goes on playing while somebody walks to another screen.
#
# It goes on playing because the media element belongs to a module and not to a
# screen, and because moving that element from one place in the page to another is
# a move and never a reload. Both halves are checked here, differently, because
# only one of them can be run.
#
# THE HALF THAT IS RUN. Which of the two slots carries the element, where a window
# ends up once the room it is in has had its say, and that a channel and a file go
# through one door so that only one of them is ever playing. player-cases.mjs
# drives all three out of the module itself, the way drag-cases.mjs next door
# drives the pointer gesture: what a person notices when any of them is wrong is a
# picture in the wrong place or two soundtracks at once, which no type checker
# sees.
#
# THE HALF THAT IS READ. Three calls end a stream: pausing the element, taking its
# address off, and load. A media element survives being moved between two parents
# of one document exactly as long as none of those three runs while it is moved,
# so they belong in the one function that means to end a stream and nowhere else.
# A fourth of them written into the code that moves the element would be a stream
# that dies on the first change of screen, would type check perfectly, and would
# look in every other check exactly like the code that works.
#
# It needs node and nothing else: no TypeScript, no document, no build.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-player.sh <top source directory>" >&2
	exit 2
}

CASES="$SRC/test/web/player-cases.mjs"
UNDER="$SRC/data/ni-web/app/ui/playing.js"
STRIP="$SRC/test/unit/scan/strip-comments.awk"

for f in "$CASES" "$UNDER" "$STRIP"; do
	[ -r "$f" ] || { echo "check-web-player.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# The comments come out first, the way they do next door: this file is full of
# sentences about pausing and about load, and a sentence is not a call.
awk -v keepstrings=1 -f "$STRIP" "$UNDER" > "$tmp/code"

# Which function each line stands in, tracked off the ones written at the left
# margin, which is every function in this module.
awk '
	/^(export )?function [A-Za-z]+\(/ {
		name = $0
		sub(/^export /, "", name)
		sub(/^function /, "", name)
		sub(/\(.*$/, "", name)
		fn = name
		next
	}
	{ printf "%s\t%d\t%s\n", fn, FNR, $0 }
' "$tmp/code" > "$tmp/lines"

[ -s "$tmp/lines" ] || {
	echo "check-web-player.sh: nothing left of $UNDER after the comments came out" >&2
	exit 1
}

# The three, on the element and not on the demuxer beside it: the demuxer has a
# load of its own and that one is neither here nor there. The element is named
# el where it is made and element where it is held, and both spellings are read
# rather than one, because which of the two a line uses is where in the module
# the line stands.
ENDS='(^|[^A-Za-z0-9_.])(el|element)\.(pause\(\)|removeAttribute\(|load\(\))'

# Every one of the three is in stop, or this reads a module that no longer ends
# a stream at all, and the rule below would then be reading an empty question.
for said in 'pause()' 'removeAttribute(' 'load()'; do
	one=`printf '%s' "$said" | sed 's/[()]/[&]/g'`
	grep -E "^stop	.*(^|[^A-Za-z0-9_.])(el|element)[.]$one" "$tmp/lines" >/dev/null || {
		echo "check-web-player.sh: stop() in $UNDER does not reach the element's own $said" >&2
		echo "  Either nothing there ends the stream and the box goes on sending" >&2
		echo "  one, or the element goes by another name now and this check has" >&2
		echo "  stopped seeing the module it is about." >&2
		exit 1
	}
done

loose=`grep -vE '^stop	' "$tmp/lines" | grep -E "$ENDS" || true`
[ -z "$loose" ] || {
	echo "check-web-player.sh: $UNDER pauses, unsets or reloads outside stop():" >&2
	printf '%s\n' "$loose" >&2
	echo "  Those three are what ends a stream. Anywhere but stop() they end one" >&2
	echo "  that was meant to keep playing, which is the whole of what this" >&2
	echo "  module is for." >&2
	exit 1
}

# AND THE ROAD A FILE TAKES ENDS NONE EITHER, which is the half of the same rule
# that was actually broken. Before a file was a source like any other, the screen
# that plays one held an element of its own and ended its own stream in its own
# cleanup, and a recording therefore stopped the moment somebody walked away from
# the listing. Either of those two lines left behind would put that back, in a
# module that type checks and whose own cases pass.
#
# Any name and not only the element's, because neither of these two holds a player
# at all: what they do is work out what to do and press a button.
#
# app/ui/webtv.js is deliberately not on this list. It takes the address off and
# loads again, and it must: that is what makes the box let go of the stream it is
# fetching off a far server, and it does it inside the function stop() calls.
ROADS='data/ni-web/app/ui/disk.js data/ni-web/app/screens/files/play.js'
NOPLAYER='(^|[^A-Za-z0-9_.])[A-Za-z_][A-Za-z0-9_.]*[.](pause\(\)|removeAttribute\(|load\(\))'
for road in $ROADS; do
	[ -r "$SRC/$road" ] || {
		echo "check-web-player.sh: cannot read $SRC/$road, and this rule is about it" >&2
		exit 1
	}
	loose=`awk -v keepstrings=1 -f "$STRIP" "$SRC/$road" | grep -nE "$NOPLAYER" || true`
	[ -z "$loose" ] || {
		echo "check-web-player.sh: $road pauses, unsets an address or reloads:" >&2
		printf '%s\n' "$loose" >&2
		echo "  Those three are what ends a stream, and they belong in stop() in" >&2
		echo "  $UNDER. Here they end a stream that was meant to go on playing" >&2
		echo "  while somebody walked to another screen." >&2
		exit 1
	}
done

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-player.sh: no node, and this check runs the sums rather than reading them" >&2
	echo "  The development container carries one; set NI_WEB_NODE to use another." >&2
	exit 1
}

"$NODE" "$CASES"

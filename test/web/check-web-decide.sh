#!/bin/sh
# What the page decides about a channel before anything is fetched.
#
# Four pure functions in app/ui/player.js answer it: which of a channel's sound
# tracks to ask the box for, whether this browser plays what the box would send
# back, which of the four roads the channel takes, and what a media element is
# doing. Nothing decided here is visible in a type or in a compiler: a channel
# offered that cannot play is a black rectangle, and a channel refused that could
# have played is a channel somebody watches in another program for no reason.
#
# Its own check rather than a section of check-web-player.sh: that one asks where
# the element goes, this one asks what to put in it.
#
# TWO TABLES ARE READ RATHER THAN RUN, and they are read because they are copies.
# The page names the sounds this box has a decoder for and the pictures a browser
# is asked about, and the box names both again in src/coreapi/browserplay.cpp. It
# has to: the box refuses what it cannot do whatever the page asks for, and what
# the copy buys is a sentence before the request instead of a failure after it.
# Two lists that drift apart is a page offering a channel the box turns down.
#
# It needs node, which check-web-types.sh needs too and which is in the
# development image. Nothing else: no TypeScript, no document, no build.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-decide.sh <top source directory>" >&2
	exit 2
}

CASES="$SRC/test/web/decide-cases.mjs"
UNDER="$SRC/data/ni-web/app/ui/player.js"
BOX="$SRC/src/coreapi/browserplay.cpp"
STRIP="$SRC/test/unit/scan/strip-comments.awk"

for f in "$CASES" "$UNDER" "$BOX" "$STRIP"; do
	[ -r "$f" ] || { echo "check-web-decide.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# Comments come out first, the way they do next door: this file is full of
# sentences about the browser and about the document, and a sentence is not a
# call.
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
	echo "check-web-decide.sh: nothing left of $UNDER after the comments came out" >&2
	exit 1
}

# The document is reached in browserPlays and nowhere else in this module. That
# is what lets the deciding be driven without a browser: everything but that
# one function is arithmetic over what the channel route wrote, and a second
# reach would be a piece of the deciding nothing here can drive. browserPlays
# itself is stood in for, the way spotFromPoint is next door.
REACHES='(^|[^A-Za-z0-9_.$])(document|window)([^A-Za-z0-9_$]|$)'
loose=`grep -E "$REACHES" "$tmp/lines" | grep -vE '^browserPlays	' || true`
[ -z "$loose" ] || {
	echo "check-web-decide.sh: $UNDER reaches the document outside browserPlays:" >&2
	printf '%s\n' "$loose" >&2
	echo "  Only browserPlays may ask the browser about itself. Everywhere else" >&2
	echo "  this module works out what the box would send from what the channel" >&2
	echo "  route wrote, which is what lets decide-cases.mjs drive it." >&2
	exit 1
}
grep -E '^browserPlays	.*'"$REACHES" "$tmp/lines" >/dev/null || {
	echo "check-web-decide.sh: browserPlays in $UNDER no longer asks the browser anything" >&2
	echo "  A table of browsers written into the page is a table that is wrong" >&2
	echo "  by the time somebody reads it, which is the whole reason that" >&2
	echo "  function exists." >&2
	exit 1
}

# The names in a list written on one line of JavaScript.
page_list() {
	sed -n "s/^const $1 = \[\(.*\)\];\$/\1/p" "$tmp/code" |
		tr -d " '" | tr ',' '\n' | grep -v '^$' | sort
}

# The keys of an object written over several lines of JavaScript.
page_keys() {
	awk -v want="const $1 = {" '
		index($0, want) == 1 { inside = 1; next }
		inside && /^};/ { inside = 0 }
		inside && /:/ { key = $1; sub(/:.*$/, "", key); print key }
	' "$tmp/code" | sort
}

# The names one predicate of the box compares its argument against.
box_list() {
	awk -v want="bool $1(" '
		index($0, want) == 1 { inside = 1 }
		inside {
			line = $0
			while (match(line, /"[^"]*"/)) {
				s = substr(line, RSTART + 1, RLENGTH - 2)
				if (s != "") print s
				line = substr(line, RSTART + RLENGTH)
			}
			if (/;/) inside = 0
		}
	' "$BOX" | sort
}

page_list kConvertible > "$tmp/page-sound"
box_list canDecodeSound > "$tmp/box-sound"
page_keys kPictureTypes > "$tmp/page-picture"
box_list isPicture > "$tmp/box-picture"

for pair in 'sound:kConvertible:canDecodeSound' 'picture:kPictureTypes:isPicture'; do
	what=`echo "$pair" | cut -d: -f1`
	here=`echo "$pair" | cut -d: -f2`
	there=`echo "$pair" | cut -d: -f3`
	[ -s "$tmp/page-$what" ] || {
		echo "check-web-decide.sh: read no names out of $here in $UNDER" >&2
		echo "  Either the table went away or it is written differently now," >&2
		echo "  and either way this has stopped comparing anything." >&2
		exit 1
	}
	[ -s "$tmp/box-$what" ] || {
		echo "check-web-decide.sh: read no names out of $there in $BOX" >&2
		exit 1
	}
	cmp -s "$tmp/page-$what" "$tmp/box-$what" || {
		echo "check-web-decide.sh: $here in $UNDER and $there in $BOX name different codecs:" >&2
		diff "$tmp/box-$what" "$tmp/page-$what" >&2 || true
		echo "  The page holds a copy of that table so it can say what will" >&2
		echo "  happen before it asks. A copy that has drifted says the wrong" >&2
		echo "  thing, and says it confidently." >&2
		exit 1
	}
done

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-decide.sh: no node, and this check runs the deciding rather than reading it" >&2
	echo "  The development container carries one; set NI_WEB_NODE to use another." >&2
	exit 1
}

"$NODE" "$CASES"

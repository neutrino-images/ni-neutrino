#!/bin/sh
# The one gesture on these pages that is performed rather than pressed.
#
# Every other check of this family reads the source as text or as types. This one
# runs it: the bouquet screen's dragging is a machine handed pointer events and a
# handful of arrangements over lists, neither of which reaches the document, and
# drag-cases.mjs drives the whole of it with objects shaped the way a browser
# shapes a PointerEvent.
#
# Its own check rather than a section of check-web-types.sh: that one asks whether
# the gesture is written in a language the browser will accept, this one whether
# the gesture puts the row where the pointer was let go of. The defect this is
# here for, a drag that fires on a mouse and not on a finger, type checked
# perfectly for as long as it existed.
#
# It needs node and nothing else: no TypeScript, no document, no build.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-drag.sh <top source directory>" >&2
	exit 2
}

CASES="$SRC/test/web/drag-cases.mjs"
UNDER="$SRC/data/ni-web/app/screens/channels/drag.js"

for f in "$CASES" "$UNDER"; do
	[ -r "$f" ] || { echo "check-web-drag.sh: cannot read $f" >&2; exit 1; }
done

# The module under test is held to reaching the document in one place and no
# more. Everything else in it is arithmetic over lists, which is what lets this
# check exist at all, and a second reach into the document would be a piece of
# the gesture nothing here can drive.
#
# Comments come out first, the way they do next door: a sentence about reaching
# the document is not a reach, and this file is full of them.
STRIP="$SRC/test/unit/scan/strip-comments.awk"
[ -r "$STRIP" ] || { echo "check-web-drag.sh: cannot read $STRIP" >&2; exit 1; }
reaches=`awk -v keepstrings=1 -f "$STRIP" "$UNDER" | grep -c 'document\.' || true`
[ "$reaches" -le 1 ] || {
	echo "check-web-drag.sh: $UNDER reaches the document $reaches times." >&2
	echo "  Only spotFromPoint may; everything else is handed what it needs," >&2
	echo "  which is what lets the gesture be driven without a browser." >&2
	exit 1
}

# AND THE ONE THING THE CASES CANNOT SEE, which is what is under the pointer.
#
# Everything drag-cases.mjs drives is the gesture, and the gesture is handed what
# the document says is under it. The screen writes that, and it wrote it on the
# rows and nowhere else, so a list that drew no row said nothing about itself: a
# bouquet somebody had just made had nothing under the pointer anywhere in it, the
# aim was nothing, and letting go did nothing. Reported from a living room, and
# green here from the first day to the last, because every case next door hands
# the machine a row.
#
# A target that stands for a whole list is the one that names no row, so the
# screen has to spell one.
SCREEN="$SRC/data/ni-web/app/screens/channels/bouquets.js"
[ -r "$SCREEN" ] || { echo "check-web-drag.sh: cannot read $SCREEN" >&2; exit 1; }
whole=`awk -v keepstrings=1 -f "$STRIP" "$SCREEN" | grep -c 'data-drop-id=""' || true`
[ "$whole" -ge 1 ] || {
	echo "check-web-drag.sh: $SCREEN names no drop target that stands for a whole list." >&2
	echo "  A list that draws no row draws nothing to let go of, so the state" >&2
	echo "  that says the list is empty has to carry the target instead." >&2
	exit 1
}

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-drag.sh: no node, and this check runs the gesture rather than reading it" >&2
	echo "  The development container carries one; set NI_WEB_NODE to use another." >&2
	exit 1
}

"$NODE" "$CASES"
echo "check-web-drag.sh: and $whole place in the screen that a whole list is let go of on, rather than a row of it"

#!/bin/sh
# Which timers ask the box for a margin at each end, and which ask to be moved
# when the broadcast moves.
#
# The page draws a line the box also draws, in another language and in another
# file: times typed into the box's own timer screen are taken as they stand and
# a guide entry pressed for recording asks for both. Neither half of that line
# is visible in a type, on a screen or in the answer the box sends back, so the
# two can come apart and nothing says so until a recording begins late.
#
# So two things are done here. The cases run the page's own function and pin
# what it sends. Then the box's two screens are read, because they are the
# other half of the line: a screen that started asking for something else is a
# line this page is now on the wrong side of.
#
# It needs node, which check-web-types.sh needs too and which is in the
# development image. Nothing else: no TypeScript, no document, no build.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-timerbody.sh <top source directory>" >&2
	exit 2
}

CASES="$SRC/test/web/timerbody-cases.mjs"
TYPED="$SRC/src/gui/timerlist.cpp"
PICKED="$SRC/src/gui/epgview.cpp"

for f in "$CASES" "$TYPED" "$PICKED"; do
	[ -r "$f" ] || { echo "check-web-timerbody.sh: cannot read $f" >&2; exit 1; }
done

# The box's own timer screen, which takes typed times as they stand. Counted
# rather than matched at a line number, because the file moves and the pair of
# assignments is what the rule is.
typed=`grep -c 'eventinfo\.recordingSafety = false;' "$TYPED" || true`
[ "$typed" -ge 1 ] || {
	echo "check-web-timerbody.sh: $TYPED no longer takes typed times as they stand" >&2
	echo "  The page sends no margins for a timer somebody typed the times of," >&2
	echo "  which was a copy of what that screen does. One of the two moved." >&2
	exit 1
}

# And the guide entry, which asks for them. The argument is the eighth of
# addRecordTimerEvent (lib/timerdclient/timerdclient.h); what is looked for here
# is that this screen still passes a true where the page now sends one.
picked=`grep -c 'TIMERD_APIDS_CONF, true,' "$PICKED" || true`
[ "$picked" -ge 1 ] || {
	echo "check-web-timerbody.sh: $PICKED no longer asks for the margins" >&2
	echo "  The page asks for them on a recording picked out of the guide," >&2
	echo "  which was a copy of what that screen does. One of the two moved." >&2
	exit 1
}

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-timerbody.sh: no node, and this check runs the model rather than reading it" >&2
	echo "  The development container carries one; set NI_WEB_NODE to use another." >&2
	exit 1
}

"$NODE" "$CASES"
echo "check-web-timerbody.sh: and the box's own two screens still draw the line this page copies"

#!/bin/sh
# What the page believes about the box, and when it goes back to ask.
#
# The store is the one module of this interface that has no shape on a screen: it
# keys an address, counts who is watching, and decides when the box is asked again.
# Every other check of this family reads the source as text or as types and cannot
# see any of that. This one runs it, with fetch replaced and nothing else.
#
# The defect it was written for was reported from a living room and was invisible to
# every check there was. A card changed the picture mode, the box took the change,
# and the card went on drawing the mode it had read before, because a write marked
# what it touched and then never went back for it. It type checked. Nothing was
# misspelled. The only way to see it was to press the button.
#
# It needs node and nothing else: no TypeScript, no document, no build.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-store.sh <top source directory>" >&2
	exit 2
}

CASES="$SRC/test/web/store-cases.mjs"
UNDER="$SRC/data/ni-web/app/store.js"

for f in "$CASES" "$UNDER"; do
	[ -r "$f" ] || { echo "check-web-store.sh: cannot read $f" >&2; exit 1; }
done

# The module under test reaches nothing outside itself but the one call to the
# box, which is what lets this check exist at all. A reach into the document or
# into the window would be a piece of it nothing here can drive, and it would
# also be a piece of it running on a box's browser and not in this shell.
#
# Comments come out first, the way they do next door: a sentence about the
# window is not a reach, and this file has several.
STRIP="$SRC/test/unit/scan/strip-comments.awk"
[ -r "$STRIP" ] || { echo "check-web-store.sh: cannot read $STRIP" >&2; exit 1; }
reaches=`awk -v keepstrings=1 -f "$STRIP" "$UNDER" | grep -c 'document\.\|window\.' || true`
[ "$reaches" -eq 0 ] || {
	echo "check-web-store.sh: $UNDER reaches the document or the window $reaches times." >&2
	echo "  It is bookkeeping over addresses and nothing else, which is what" >&2
	echo "  lets the whole of it be driven without a browser." >&2
	exit 1
}

# AND THE ONE THING THE CASES CANNOT SEE, which is that the number the page reads is
# the number the server writes.
#
# The store asks the box again for what a write touched, except where the box
# answered that it has only taken the message and has not acted yet. That answer is
# one status code, chosen in src/httpd/http.h, and the page names it a second time
# because nothing carries it across. If the two ever named different ones the page
# would either read a channel list while the box is rebuilding it or stop reading
# anything back at all, and neither shows up as an error anywhere.
SERVER="$SRC/src/httpd/http.h"
[ -r "$SERVER" ] || { echo "check-web-store.sh: cannot read $SERVER" >&2; exit 1; }
theirs=`awk '$0 ~ /StatusAccepted/ && $0 ~ /=/ { gsub(/[^0-9]/, "", $NF); print $NF; exit }' "$SERVER"`
ours=`awk -v keepstrings=1 -f "$STRIP" "$UNDER" | awk '/kTaken[ \t]*=/ { gsub(/[^0-9]/, "", $NF); print $NF; exit }'`
[ -n "$theirs" ] || { echo "check-web-store.sh: $SERVER names no StatusAccepted" >&2; exit 1; }
[ -n "$ours" ] || {
	echo "check-web-store.sh: $UNDER names no kTaken, and the rule it stands for is the whole of this check" >&2
	exit 1
}
[ "$theirs" = "$ours" ] || {
	echo "check-web-store.sh: the server answers $theirs for a change it has only taken and the page reads $ours" >&2
	exit 1
}

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-store.sh: no node, and this check runs the store rather than reading it" >&2
	echo "  The development container carries one; set NI_WEB_NODE to use another." >&2
	exit 1
}

"$NODE" "$CASES"
echo "check-web-store.sh: and the $ours the page reads for a change the box has only taken is the one src/httpd/http.h writes"

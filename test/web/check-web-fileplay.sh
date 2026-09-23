#!/bin/sh
# What the page does with a file on a disk of this box.
#
# A recording is a transport stream, which no browser opens, and the page takes it
# apart the way it does for live television. What that leaves is a set of
# judgements no compiler sees: which container goes which way, what to do when the
# demuxer names a codec this browser will not decode, and what a control may
# promise about jumping in a file that carries no index. fileplay-cases.mjs drives
# all three, the way decide-cases.mjs next door drives the same questions for a
# channel.
#
# TWO FILES AND NOT ONE, since the deciding and the screen came apart. app/ui/
# disk.js is the road app/ui/playing.js takes for a file, the way player.js is the
# road it takes for a channel, and it holds everything that is worked out rather
# than acted on. app/screens/files/play.js is what a person presses. Both are read
# here, because the three faults below would pass every other check in this suite
# from either of them.
#
# The demuxer is fetched in one place. It is sixty kilobytes, app/ui/playing.js
# fetches it for live television already, and a second address written in either
# of these would be a second copy of it in the browser and a second version to
# keep in step.
#
# No address is built in either. api.js builds every address this interface asks
# for, and the screen that lists the files hands this one over; a path spelled
# here would be a path check-web-paths.sh never sees.
#
# And each of the two reaches the document in exactly one function. In disk.js
# that is browserAsks, because everything else there works out what to do from
# what the demuxer said, which is what lets the cases drive it; in the screen it
# is Elsewhere, which turns the address it was handed into the one another program
# can be given. A second reach into the document in either is a piece of the
# deciding nothing here can drive.
#
# It needs node, which the development image carries. Nothing else: no TypeScript,
# no document, no build.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-fileplay.sh <top source directory>" >&2
	exit 2
}

CASES="$SRC/test/web/fileplay-cases.mjs"
DISK="$SRC/data/ni-web/app/ui/disk.js"
SCREEN="$SRC/data/ni-web/app/screens/files/play.js"
DEMUX="$SRC/data/ni-web/app/ui/playing.js"
STRIP="$SRC/test/unit/scan/strip-comments.awk"

for f in "$CASES" "$DISK" "$SCREEN" "$DEMUX" "$STRIP"; do
	[ -r "$f" ] || { echo "check-web-fileplay.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# Which function each line stands in, tracked off the ones written at the left
# margin, which is every function in both of these modules.
#
# The comments come out first, the way they do next door: both files are full of
# sentences about the browser, about addresses and about the demuxer, and a
# sentence is not a call.
cat > "$tmp/fn.awk" <<'AWKEOF'
	/^(export )?function [A-Za-z]+\(/ {
		name = $0
		sub(/^export /, "", name)
		sub(/^function /, "", name)
		sub(/\(.*$/, "", name)
		fn = name
		next
	}
	{ printf "%s\t%d\t%s\n", fn, FNR, $0 }
AWKEOF

# The demuxer, named where it is delivered and nowhere else.
grep -n 'mpegts' "$DEMUX" >/dev/null || {
	echo "check-web-fileplay.sh: $DEMUX no longer names the demuxer," >&2
	echo "  so this has stopped reading the module it is about" >&2
	exit 1
}

# The document, in the one function of each that is allowed to reach it.
REACHES='(^|[^A-Za-z0-9_.$])(document|window)([^A-Za-z0-9_$]|$)'

read_one() {
	awk -v keepstrings=1 -f "$STRIP" "$1" > "$tmp/code"
	[ -s "$tmp/code" ] || {
		echo "check-web-fileplay.sh: nothing left of $1 after the comments came out" >&2
		exit 1
	}
	awk -f "$tmp/fn.awk" "$tmp/code" > "$tmp/lines"

	loose=`grep -n 'vendor/' "$tmp/code" || true`
	[ -z "$loose" ] || {
		echo "check-web-fileplay.sh: $1 names a module under vendor/:" >&2
		printf '%s\n' "$loose" >&2
		echo "  The demuxer is fetched by app/ui/playing.js, once for the page. A" >&2
		echo "  second address here is sixty kilobytes fetched twice and a version" >&2
		echo "  to keep in step in two places." >&2
		exit 1
	}

	loose=`grep -n '/api/' "$tmp/code" || true`
	[ -z "$loose" ] || {
		echo "check-web-fileplay.sh: $1 spells an address of the box:" >&2
		printf '%s\n' "$loose" >&2
		echo "  The listing hands this screen the address it already built. A path" >&2
		echo "  written here is one check-web-paths.sh never compares with the" >&2
		echo "  document the server writes about itself." >&2
		exit 1
	}

	loose=`grep -E "$REACHES" "$tmp/lines" | grep -vE "^$2	" || true`
	[ -z "$loose" ] || {
		echo "check-web-fileplay.sh: $1 reaches the document outside $2:" >&2
		printf '%s\n' "$loose" >&2
		echo "  Only that one may. Everywhere else these two work out what to do" >&2
		echo "  from what they were handed, which is what lets" >&2
		echo "  fileplay-cases.mjs drive them." >&2
		exit 1
	}
	grep -E "^$2	.*$REACHES" "$tmp/lines" >/dev/null || {
		echo "check-web-fileplay.sh: $2 in $1 no longer reaches the document at all," >&2
		echo "  so the rule above is holding an empty question. A table of browsers" >&2
		echo "  written into the page is a table that is wrong by the time somebody" >&2
		echo "  reads it, which is the whole reason that function exists." >&2
		exit 1
	}
}

read_one "$DISK" browserAsks
read_one "$SCREEN" Elsewhere

NODE="${NI_WEB_NODE:-node}"
command -v "$NODE" >/dev/null 2>&1 || {
	echo "check-web-fileplay.sh: no node, and this check runs the deciding rather than reading it" >&2
	echo "  The development container carries one; set NI_WEB_NODE to use another." >&2
	exit 1
}

"$NODE" "$CASES"

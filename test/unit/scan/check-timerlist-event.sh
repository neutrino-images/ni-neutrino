#!/bin/sh
# Timerd reports a list change in the same pass that announces a timer, and an
# open hint closes on any message handleMsg leaves unhandled.
#
# Checked as text because CNeutrinoApp is the application and stands in no LDADD.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-timerlist-event.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
APP="$SRC/src/neutrino.cpp"
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
for f in "$APP" "$STRIP" "$BLANK"; do
	[ -r "$f" ] || { echo "check-timerlist-event.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

awk -v keepstrings=0 -f "$STRIP" "$APP" | awk -f "$BLANK" > "$tmp/app"
[ -s "$tmp/app" ] || {
	echo "check-timerlist-event.sh: nothing left of $APP after stripping" >&2
	exit 1
}

awk '
	/^int CNeutrinoApp::handleMsg\(/ { inside = 1 }
	inside { print }
	inside && /^}/ { exit }
' "$tmp/app" > "$tmp/handlemsg"
[ -s "$tmp/handlemsg" ] || {
	echo "check-timerlist-event.sh: no CNeutrinoApp::handleMsg in $APP" >&2
	exit 1
}

NAME="NeutrinoMessages::EVT_TIMERLIST_CHANGED"
ARM=`awk -v name="$NAME" '
	found == 0 && index($0, name) > 0 { found = 1; $0 = substr($0, index($0, name)) }
	found == 1 {
		print
		n = length($0)
		for (i = 1; i <= n; i++) {
			c = substr($0, i, 1)
			if (c == "{") { depth++; seen = 1 }
			else if (c == "}") { depth--; if (seen && depth == 0) exit }
		}
	}
' "$tmp/handlemsg" | tr -d '[:space:]'`

[ -n "$ARM" ] || {
	echo "check-timerlist-event.sh: CNeutrinoApp::handleMsg has no arm for $NAME" >&2
	echo "  it falls through to unhandled and closes the open hint" >&2
	exit 1
}

case "$ARM" in
	*unhandled*|*cancel_*)
		echo "check-timerlist-event.sh: the arm for $NAME closes the open hint" >&2
		exit 1
		;;
	*"returnmessages_return::handled;"*)
		;;
	*)
		echo "check-timerlist-event.sh: the arm for $NAME does not answer handled" >&2
		exit 1
		;;
esac

echo "check-timerlist-event.sh: a change to the timer list leaves an open hint standing"
exit 0

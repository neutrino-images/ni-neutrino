#!/bin/sh
# The personalisation menu answers a type for every plugin, which is all five plugin lists at
# once. Written into the program's settings one list at a time, or with the type set on the
# loaded plugin beside them, the coupling that keeps a plugin in one list never sees the answer
# and the plugin group reads the list once per list. The unit test of the batch shows what the
# layer does with the five lists; it does not link the menu, so this reads the menu as text.
#
# The menu writes no plugin list itself, sets no type on a plugin, and hands the lists to
# writeBatch.
#
# An optional second argument names one file to read instead of the menu, for the self test.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-personalize-batch.sh <top source directory> [file]" >&2
	exit 2
}

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
[ -r "$STRIP" ] || { echo "check-personalize-batch.sh: cannot read $STRIP" >&2; exit 1; }

FILE="${2:-$SRC/src/gui/personalize.cpp}"
[ -r "$FILE" ] || { echo "check-personalize-batch.sh: cannot read $FILE" >&2; exit 1; }

STRIPPED=`awk -v keepstrings=1 -f "$STRIP" "$FILE"`

BAD=`printf '%s\n' "$STRIPPED" | awk -v file="$FILE" '
	/g_settings\.plugins_(disabled|game|tool|script|lua)/ && /(setSettingsText|appendSettingsText)[ \t]*\(|g_settings\.plugins_[a-z]+[ \t]*(=[^=]|\+=)/ {
		print "    " file ":" NR ": " $0
	}
	/setType[ \t]*\(/ { print "    " file ":" NR ": " $0 }
'`

if ! printf '%s\n' "$STRIPPED" | grep -q 'writeBatch[ 	]*('; then
	BAD="$BAD
    $FILE: the plugin lists are not handed to writeBatch"
fi

if [ -n "$BAD" ]; then
	echo "check-personalize-batch.sh: the plugin lists are written past the batch:" >&2
	printf '%s\n' "$BAD" | sed '/^$/d' >&2
	exit 1
fi

exit 0

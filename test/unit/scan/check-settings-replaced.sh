#!/bin/sh
# The settings file is read over the running settings in more than one place: the
# reload a web write asks for, a backup loaded from the menu, the reset. Each
# apply group compares with what it last sent, so a reload that leaves the groups
# out leaves the fonts, the palette and every other group that keeps such a state
# at the old values until something else runs them.
#
# Checked as text because the places are the application and stand in no LDADD.
#
# Every call of loadSetup but the one at startup, before any group has run, sits
# inside settings::applyReplaced or CSettingsManager::replaceFromMenu, on the line
# that names it.
#
# An optional second argument names one file to read instead of the tree, for
# the self test.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-settings-replaced.sh <top source directory> [file]" >&2
	exit 2
}

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
[ -r "$STRIP" ] || { echo "check-settings-replaced.sh: cannot read $STRIP" >&2; exit 1; }

if [ -n "$2" ]; then
	FILES="$2"
else
	# Only the files that name it, the definition's among them, so an empty list is a moved tree.
	FILES=`find "$SRC/src" -name '*.cpp' -exec grep -l 'loadSetup' {} + | sort`
fi
[ -n "$FILES" ] || { echo "check-settings-replaced.sh: no loadSetup under $SRC/src" >&2; exit 1; }

# The definition and the startup read are the two that need no replacement.
BAD=`for f in $FILES; do
	awk -v keepstrings=0 -f "$STRIP" "$f" | awk -v file="$f" '
		/(^|[^A-Za-z0-9_])loadSetup[ \t]*\(/ {
			if ($0 ~ /CNeutrinoApp::loadSetup[ \t]*\(const char/) next
			if ($0 ~ /loadSettingsErg[ \t]*=[ \t]*loadSetup[ \t]*\(/) next
			if ($0 ~ /applyReplaced[ \t]*\(/ || $0 ~ /replaceFromMenu[ \t]*\(/) next
			print "    " file ":" FNR ": " $0
		}
	'
done`

if [ -n "$BAD" ]; then
	echo "check-settings-replaced.sh: the settings file is read over the settings past the apply groups:" >&2
	printf '%s\n' "$BAD" >&2
	exit 1
fi

exit 0

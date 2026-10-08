#!/bin/sh
# The HDMI link modes are numbered by the hardware library of each family, and
# the box is told the stored number as one of them. The generic build numbers
# them 0, 1 and 2, the same as the literals this row once carried, so no case
# that runs in this build can tell a library name from a number written out; only
# the family whose library numbers them otherwise could, and no suite runs there.
# So the row is held to the names as text: every entry of the table starts with
# a library name, and none with a number.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-cecmodes.sh <top source directory>" >&2
	exit 2
}

TABLE="$SRC/src/coreapi/settings/settingstable_video.cpp"
[ -r "$TABLE" ] || { echo "check-cecmodes.sh: cannot read $TABLE" >&2; exit 1; }

entries=`awk '
	/const EnumValue kCecMode\[\]/ { inside = 1; next }
	inside && /^\};/ { inside = 0 }
	inside && /^[ \t]*\{[ \t]*[A-Za-z0-9]/ { print }
' "$TABLE"`

# A table that was renamed or reshaped would leave nothing to read and pass.
count=`printf '%s\n' "$entries" | grep -c .` || count=0
if [ "$count" -ne 3 ]; then
	echo "check-cecmodes.sh: $count entries found in kCecMode, expected 3, the scan has stopped matching" >&2
	exit 1
fi

named=`printf '%s\n' "$entries" | grep -cE '^[ 	]*\{[ 	]*VIDEO_HDMI_CEC_MODE_[A-Z]+,'` || named=0
if [ "$named" -ne 3 ]; then
	echo "check-cecmodes.sh: an entry of kCecMode does not start with the hardware library's name for the mode:" >&2
	printf '%s\n' "$entries" >&2
	exit 1
fi

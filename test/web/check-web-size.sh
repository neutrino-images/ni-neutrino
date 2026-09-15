#!/bin/sh
# What the web pages cost a box, measured and held to a ceiling.
#
# Measured the way the box stores them: every file the pages install is stored
# compressed and the server inflates it for a caller that took none, so the
# number that matters is the compressed one.
#
# A CEILING AND NOT AN IDENTITY, which is the one number in this suite that is not
# held to being exactly right. Ten screens are written side by side and an identity
# would be a line all ten of them move. A ceiling is touched only by whoever breaks
# it, and that is exactly the moment somebody should look.
#
# The runtime is six files the buildsystem delivers and this tree does not carry.
# They are measured where they have been fetched and counted at their published
# size where they have not, because leaving them out would let the ceiling be met
# by a page that loads more than it says.
#
# The last two are the playlist library and the demuxer, and both are counted whole
# although no page loads either until somebody presses play: what the ceiling is
# about is what a box carries in its flash.
#
# THE HANDSET PHOTOGRAPHS ARE WEIGHED AS ONE BOX GETS THEM AND NOT AS THEY LIE.
# rc/ holds a picture per shape of remote control and a build installs only the ones
# its own model can be sold with (data/ni-web/rc/Makefile.am), so weighing the whole
# directory would be weighing fifteen boxes at once. What is added here is the
# heaviest single model's set, read out of that makefile rather than typed here.
#
# WHERE THE CEILING LIVES. In test/unit/support/counts.txt, but as a comment there: the
# reader beside it holds every plain line in that file to a figure the run measured,
# and nothing in the program measures the size of a web page. Both spellings are
# read here so the check does not depend on which way round it ends up.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-size.sh <top source directory>" >&2
	exit 2
}

# No second argument. This wrote the figure into counts.actual, which nothing
# ever read it out of: the suite writes and compares that file while it runs,
# and a line appended after it has finished is one nobody looks at. What made
# it visible is that the loop in test/Makefile.am hands every check of this
# family the build directory as the second argument, and a directory is not a
# file to append to, so this check failed and every check behind it in the loop
# never ran at all.
ROOT="$SRC/data/ni-web"
HANDSETS="$ROOT/rc"
COUNTS="$SRC/test/unit/support/counts.txt"
CEILING_NAME="web bytes the pages may cost gzipped"

# What each of them costs where it has not been fetched into this tree, gzip -9 over
# the bytes the buildsystem installs: preact 10.24.3 and its hooks, htm 3.1.1,
# preact-router 4.1.2, the light build of hls.js 1.7.3 and mpegts.js 1.8.2.
#
# Per file and not one figure for the five, so a tree with four of them fetched and
# one not is counted whole. One figure compared against the sum of whatever happened
# to be there reads a version a few bytes under it as a runtime nobody had fetched.
#
# The demuxer is 61840 of it on its own, seven times the four modules together, and
# it is counted although it is not on every box: a coolstream cannot convert a
# channel's sound for a browser, so the buildsystem leaves it out there. Counting it
# regardless holds the ceiling against the heaviest box.
published() {
	case "$1" in
		preact.module.js)        echo 4651;;
		hooks.module.js)         echo 1556;;
		htm.module.js)           echo 660;;
		preact-router.module.js) echo 1893;;
		hls.module.js)           echo 115373;;
		mpegts.js)               echo 61840;;
		*)                       echo 0;;
	esac
}

[ -d "$ROOT" ] || { echo "check-web-size.sh: cannot read $ROOT" >&2; exit 1; }
[ -r "$COUNTS" ] || { echo "check-web-size.sh: cannot read $COUNTS" >&2; exit 1; }

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

ceiling=`awk -F'\t' -v name="$CEILING_NAME" '
	{
		row = $1
		sub(/^#[ \t]*/, "", row)
		if (row == name) { print $2; exit }
	}' "$COUNTS"`

[ -n "$ceiling" ] || {
	echo "check-web-size.sh: $COUNTS names no ceiling, and a budget nobody wrote down is not one" >&2
	echo "check-web-size.sh: the line it looks for is: # $CEILING_NAME	<bytes>" >&2
	exit 1
}

# Everything the pages install, and only that. The directories below are
# walked by their own install rules, so a file in one of them is a file that
# reaches a box. The top of this tree is not walked: its rule names index.html
# and nothing else (data/ni-web/Makefile.am), so LICENSE, README.md and
# jsconfig.json stay here and are not weighed. They were, until this line was
# written, and the figure was that much too large.
find "$ROOT" -type f ! -name 'Makefile.*' ! -name '.gitignore' -print \
	| grep -v "^$ROOT/vendor/" \
	| grep -v "^$ROOT/swagger/vendor/" \
	| grep -v "^$HANDSETS/" \
	| grep -vE "^$ROOT/[^/]+$|^$ROOT/index\.html$" \
	| LC_ALL=C sort > "$tmp/files"
printf '%s\n' "$ROOT/index.html" >> "$tmp/files"
LC_ALL=C sort -o "$tmp/files" "$tmp/files"

files=`wc -l < "$tmp/files" | tr -d ' '`
[ "$files" -gt 0 ] || {
	echo "check-web-size.sh: nothing under $ROOT, so this weighed nothing" >&2
	exit 1
}

pages=0
while read -r file; do
	one=`gzip -9 -c "$file" | wc -c | tr -d ' '`
	pages=`expr $pages + $one`
done < "$tmp/files"

# The handset photographs, grouped the way the makefile installs them: a name
# outside any model's arm goes on every box, a name inside one goes on that
# model. What the heaviest of them weighs is what a box pays.
#
# Read rather than listed, because a list here would be a second opinion about
# what is installed, and the two would drift the first time a model is added.
awk '
	/^if BOXMODEL_/ { group = $2; next }
	/^endif/ { group = ""; next }
	/^install_DATA[ \t]*\+?=/ { inlist = 1 }
	/^[A-Za-z_]+[ \t]*\+?=/ && $1 != "install_DATA" { inlist = 0 }
	inlist {
		for (i = 1; i <= NF; i++)
			if ($i ~ /\.webp$/)
				printf "%s\t%s\n", (group == "" ? "-" : group), $i
		if ($0 !~ /\\$/)
			inlist = 0
	}
' "$HANDSETS/Makefile.am" | LC_ALL=C sort -u > "$tmp/handsets"

[ -s "$tmp/handsets" ] || {
	echo "check-web-size.sh: $HANDSETS/Makefile.am installs no picture, and the directory holds some" >&2
	exit 1
}

# Weighed once per name, so that a picture two models share is not measured
# twice over. A name the makefile installs and the directory does not hold is a
# build that fails on somebody else's model, which is exactly the fault this
# whole arrangement can introduce.
cut -f2 "$tmp/handsets" | LC_ALL=C sort -u > "$tmp/named"
: > "$tmp/weights"
while read -r name; do
	[ -r "$HANDSETS/$name" ] || {
		echo "check-web-size.sh: $HANDSETS/Makefile.am installs $name, which is not there" >&2
		exit 1
	}
	bytes=`gzip -9 -c "$HANDSETS/$name" | wc -c | tr -d ' '`
	printf '%s\t%s\n' "$name" "$bytes" >> "$tmp/weights"
done < "$tmp/named"

# And nothing lies there that no model is given, which would be flash spent in
# the archive on a picture no box can ask for.
find "$HANDSETS" -type f -name '*.webp' -print | sed "s|^$HANDSETS/||" \
	| LC_ALL=C sort > "$tmp/present"
unnamed=`LC_ALL=C comm -23 "$tmp/present" "$tmp/named"`
[ -z "$unnamed" ] || {
	echo "check-web-size.sh: $HANDSETS holds pictures no model installs:" >&2
	printf '%s\n' "$unnamed" >&2
	exit 1
}

handsets=`awk -F'\t' -v weightfile="$tmp/weights" '
	BEGIN {
		while ((getline line < weightfile) > 0) {
			split(line, part, "\t")
			weight[part[1]] = part[2] + 0
		}
	}
	{ total[$1] += weight[$2] }
	END {
		most = total["-"]
		for (g in total)
			if (g != "-" && total["-"] + total[g] > most)
				most = total["-"] + total[g]
		print most + 0
	}
' "$tmp/handsets"`

runtime=0
fetched=0
absent=0
for one in preact.module.js hooks.module.js htm.module.js preact-router.module.js \
	   hls.module.js mpegts.js; do
	if [ -r "$ROOT/vendor/$one" ]; then
		bytes=`gzip -9 -c "$ROOT/vendor/$one" | wc -c | tr -d ' '`
		fetched=`expr $fetched + 1`
	else
		bytes=`published "$one"`
		absent=`expr $absent + 1`
		[ "$bytes" -gt 0 ] || {
			echo "check-web-size.sh: $one is neither fetched nor named above, so it was weighed as nothing" >&2
			exit 1
		}
	fi
	runtime=`expr $runtime + $bytes`
done

where="measured"
if [ "$absent" -gt 0 ]; then
	where="$fetched measured, $absent as published"
fi

total=`expr $pages + $runtime + $handsets`

# The two files the whole look is spelled in, uncompressed, because that is what a
# person reads and edits.
#
# Raised to 30720 when six streams met. Three of them had reason to write here and
# each held back so as not to raise it twice: a switch drawn rather than ticked, one
# gutter reserved on every settings row rather than added to the marked one, a
# column saying once where its content sits. All three are the trade this figure
# exists to invite. It stays a ceiling and not an identity.
look=0
for one in "$ROOT/app/css/tokens.css" "$ROOT/app/css/shell.css"; do
	[ -r "$one" ] || { echo "check-web-size.sh: cannot read $one" >&2; exit 1; }
	bytes=`wc -c < "$one" | tr -d ' '`
	look=`expr $look + $bytes`
done

bad=0
if [ "$total" -gt "$ceiling" ]; then
	echo "check-web-size.sh: $total bytes over $files files, the runtime and the handset one model gets, and the ceiling is $ceiling" >&2
	bad=1
fi

# TWENTY THREE AND NOT TEN AND A QUARTER, AND WHAT MOVED IT. The figure was set for a
# frame with one bar and one fold: a row along the top, a single line carrying the
# path where the window was narrow, and a column down the left for the screens of
# the destination that was open.
#
# The frame has two bars now and three widths. The destinations moved into a bar
# along the bottom below laptop width, because a row of eight along the top ran off
# the edge of a box that scrolled in silence. What is playing became an element
# rather than a line of text. The screens of the open destination became a row of
# pills at every width, so the column went. And a table folds into cards below phone
# width as one rule here rather than as a decision each of twenty screens makes for
# itself, which is the same trade this file's own ceiling exists to make.
#
# Raised to a round figure with room in it rather than to what the redesign happened
# to cost, because a ceiling raised to exactly fit is one the next person meets again
# on their first line.
#
# TWENTY EIGHT AND WHAT MOVED IT AGAIN. Two controls of this interface were the
# browser's own with this page's colours on them, a checkbox where the design has a
# switch and a select in whatever frame the machine draws. Both are painted here now:
# one control defined once against twenty screens each drawing their own. The fields
# gained a width and the telephone gained its target heights at the same time.
#
# THIRTY TWO AND WHAT MOVED IT AGAIN. Every size in every stylesheet of these pages
# became a fraction of one number on the root element, so a reader who cannot read
# the page at the size it ships with can enlarge it and a browser already told to
# draw text larger is obeyed. Measured at 31913.
if [ "$look" -gt 32768 ]; then
	echo "check-web-size.sh: the colours and the frame are $look bytes and the two of them stay under 32768" >&2
	bad=`expr $bad + 1`
fi

[ "$bad" -eq 0 ] || exit 1

echo "check-web-size.sh: $total bytes gzipped, $pages over $files files, $runtime for the runtime ($where) and $handsets for the handsets of the heaviest model, under the ceiling of $ceiling; the look is $look bytes raw"

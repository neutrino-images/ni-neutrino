#!/bin/sh
# A menu that waits for a key marks itself as the one on top, and the settings
# written from outside are drawn into that menu only. An item it runs puts
# something else on the screen, so the mark has to be cleared for as long as that
# item runs; one run outside the clearing paints the menu's items over the
# dialog the item opened.
#
# So every run of an item (`->exec(`, `.exec(` or `exec(`) inside
# CMenuWidget::exec sits in a block that declares `WaitingMenu busy(NULL)` before
# it. Checked as text because the menu loop needs
# a framebuffer and a remote control to run.
#
# With a second argument the scan reads that file instead of the menu source, for
# the fixtures that show it goes red.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-menu-busy.sh <top source directory> [file]" >&2
	exit 2
}

HERE=`dirname "$0"`
FILE="${2:-$SRC/src/gui/widget/menue.cpp}"
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
for f in "$FILE" "$STRIP" "$BLANK"; do
	[ -r "$f" ] || { echo "check-menu-busy.sh: cannot read $f" >&2; exit 1; }
done

# Below this the scan is not measuring the loop any more: the two sites that
# run an item were found when this was written.
FLOOR_SITES=2

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

awk -v keepstrings=0 -f "$STRIP" "$FILE" | awk -f "$BLANK" > "$tmp/menu"

awk '
	/^int CMenuWidget::exec\(/ { inside = 1 }
	inside { print }
	inside && /^}/ { exit }
' "$tmp/menu" > "$tmp/body"
[ -s "$tmp/body" ] || {
	echo "check-menu-busy.sh: no CMenuWidget::exec in $FILE, the scan has stopped matching" >&2
	exit 1
}

# Brace depth, walked in the order the text comes: a clearing declared at a depth
# holds until the block it is in closes. A line is cut at each brace so that a
# block written on one line, `{ WaitingMenu busy(NULL); x->exec(); }`, is
# cleared for its own run and no longer once its brace closes. A run is `->exec(`,
# `.exec(` or a bare `exec(`. P is a site in the clear, U one outside it. The
# first line is the definition itself.
awk '
	function look(text, line,    rest, e, el, b, bl, isbusy, guarded, d) {
		rest = text
		for (;;) {
			e = match(rest, /(^|[^A-Za-z0-9_])exec[ \t]*\(/)
			el = RLENGTH
			b = match(rest, /WaitingMenu[ \t]+busy\(NULL\)/)
			bl = RLENGTH
			if (!e && !b)
				return
			isbusy = b && (!e || b < e)
			if (isbusy) {
				clear[depth] = 1
				rest = substr(rest, b + bl)
			} else {
				if (NR > 1) {
					guarded = 0
					for (d = 0; d <= depth; d++)
						if (clear[d])
							guarded = 1
					printf "%s\t%d\t%s\n", guarded ? "P" : "U", NR, line
				}
				rest = substr(rest, e + el)
			}
		}
	}
	{
		line = $0
		n = length(line)
		seg = ""
		for (i = 1; i <= n; i++) {
			c = substr(line, i, 1)
			if (c == "{" || c == "}") {
				look(seg, line)
				seg = ""
				if (c == "{")
					depth++
				else {
					clear[depth] = 0
					depth--
				}
			} else
				seg = seg c
		}
		look(seg, line)
	}
' "$tmp/body" > "$tmp/sites"

sites=`grep -c '^P	' "$tmp/sites" || true`
if grep -q '^U	' "$tmp/sites"; then
	echo "check-menu-busy.sh: these run an item in CMenuWidget::exec without" >&2
	echo "  WaitingMenu busy(NULL) around them, so a write from outside paints the" >&2
	echo "  menu over the dialog the item opened:" >&2
	grep '^U	' "$tmp/sites" | cut -f3- | sed 's/^[[:space:]]*/  /' >&2
	exit 1
fi
if [ "$sites" -lt "$FLOOR_SITES" ]; then
	echo "check-menu-busy.sh: $sites item runs found in CMenuWidget::exec, below the" >&2
	echo "  floor of $FLOOR_SITES: the scan has stopped matching" >&2
	exit 1
fi

exit 0

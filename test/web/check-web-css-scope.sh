#!/bin/sh
# A screen's stylesheet may not restyle a class the frame owns.
#
# The frame's two sheets are in the document from the first byte; a screen's sheet
# is attached when that screen is first opened and stays there for as long as the
# page is open. So a rule in a screen's sheet whose subject is a bare class
# reaches every screen visited after it.
#
# It happened with .grid. The frame draws every table as table.grid; the guide
# called its own outermost box .grid and laid it out as a flex box. Once the guide
# had been opened, every table on every screen after it was a flex box. A reload
# put it right, because a reload takes the guide's sheet back out of the document,
# which is what makes this kind of fault so hard to see.
#
# What is allowed is the same class under a name of the screen's own:
# .ch-filters .field is this screen's fields and nobody else's. The subject of the
# selector is what decides it, since that is the element the rule paints.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-css-scope.sh <top source directory>" >&2
	exit 2
}

WEB="$SRC/data/ni-web/app"
FRAME="$WEB/css"
[ -d "$FRAME" ] || {
	echo "check-web-css-scope.sh: $FRAME is not in the tree, so nothing here" >&2
	echo "  can say which class names the frame owns" >&2
	exit 1
}

frame_sheets=$(find "$FRAME" -name '*.css' | sort)
[ -n "$frame_sheets" ] || {
	echo "check-web-css-scope.sh: no stylesheet under $FRAME" >&2
	exit 1
}

# Every sheet that is not the frame's, which is every one a screen or a shared
# control names for itself.
screen_sheets=$(find "$WEB" -name '*.css' | grep -v "^$FRAME/" | sort)
[ -n "$screen_sheets" ] || {
	echo "check-web-css-scope.sh: no screen stylesheet under $WEB" >&2
	exit 1
}

# The selectors of a sheet, one per line, comments gone and at-rule preludes
# dropped. Declarations hold no brace, so the text before every { is either a
# prelude or a list of selectors, and the rules inside a media block are read
# the same way as the rules outside one.
selectors() {
	awk '
		BEGIN { RS = "\0" }
		{
			s = $0
			while ((i = index(s, "/*")) > 0) {
				rest = substr(s, i + 2)
				j = index(rest, "*/")
				if (j == 0) { s = substr(s, 1, i - 1); break }
				s = substr(s, 1, i - 1) " " substr(rest, j + 2)
			}
			gsub(/[\n\t]+/, " ", s)
			n = split(s, block, "{")
			for (k = 1; k <= n - 1; k++) {
				sel = block[k]
				if ((p = index(sel, "}")) > 0)
					sel = substr(sel, p + 1)
				gsub(/^ +| +$/, "", sel)
				if (sel == "" || substr(sel, 1, 1) == "@")
					continue
				print sel
			}
		}
	' "$1"
}

# Every class name the frame spells, wherever it spells it.
owned=$(for f in $frame_sheets; do selectors "$f"; done \
	| grep -oE '\.[A-Za-z_][-A-Za-z0-9_]*' | sed 's/^\.//' | sort -u)
[ -n "$owned" ] || {
	echo "check-web-css-scope.sh: the frame's sheets name no class at all," >&2
	echo "  so this would pass whatever a screen wrote" >&2
	exit 1
}

owned_list=$(printf '%s\n' "$owned" | tr '\n' ' ')

found=0
sheets=0
rules=0
for f in $screen_sheets; do
	# A sheet this cannot read is a sheet it would pass whatever was in it, so
	# an empty reading is a failure and not a clean one.
	here=$(selectors "$f" | wc -l | tr -d ' ')
	[ "$here" -gt 0 ] || {
		echo "check-web-css-scope.sh: read no rule at all out of $f" >&2
		exit 1
	}
	rules=$((rules + here))
	bad=$(selectors "$f" | awk -v owned="$owned_list" '
		BEGIN {
			n = split(owned, o, " ")
			for (i = 1; i <= n; i++)
				if (o[i] != "") mine[o[i]] = 1
		}
		{
			count = split($0, list, ",")
			for (c = 1; c <= count; c++) {
				sel = list[c]
				gsub(/^ +| +$/, "", sel)
				if (sel == "") continue
				# The subject is the last compound; what stands before it is
				# what scopes the rule.
				flat = sel
				gsub(/[>+~]/, " ", flat)
				parts = split(flat, piece, " ")
				subject = piece[parts]
				scoped = 0
				for (p = 1; p < parts; p++)
					if (index(piece[p], ".") > 0) scoped = 1
				if (scoped) continue
				# A subject that is exactly one class, with nothing but
				# pseudo-classes behind it: anything else already says which
				# element or which state it means.
				if (subject !~ /^\.[A-Za-z_][-A-Za-z0-9_]*(:{1,2}[A-Za-z-]+(\([^)]*\))?)*$/)
					continue
				name = subject
				sub(/^\./, "", name)
				sub(/:.*$/, "", name)
				if (name in mine)
					print "    " sel
			}
		}
	')
	sheets=$((sheets + 1))
	if [ -n "$bad" ]; then
		found=1
		echo "check-web-css-scope.sh: $f restyles a class the frame owns:" >&2
		printf '%s\n' "$bad" >&2
	fi
done

if [ "$found" != 0 ]; then
	echo "  Give the rule a subject of this screen's own, or scope it under one." >&2
	exit 1
fi

count=$(printf '%s\n' "$owned" | wc -l | tr -d ' ')
echo "check-web-css-scope.sh: $rules rules in $sheets screen stylesheets, none of them a class of the $count the frame owns"

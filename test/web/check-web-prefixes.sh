#!/bin/sh
# The areas the server answers with the page and the areas the page navigates to
# are one list, stated twice.
#
# The server has to carry the list, because a fallback on everything answers a
# mistyped module name with a document and turns a missing file into a syntax
# error on line 1 (src/httpd/apppaths.h). The page has to carry it, because it is
# what a person clicks. Neither can read the other: one is compiled into the
# program, the other is loaded by a browser.
#
# Two statements of one list drift in the direction nothing reports. An eighth area
# added to the page alone is a destination that reloads into a 404, and only after
# a reload: the first visit works, because the page got there without asking this
# server for that path. An area renamed in the server alone takes a working address
# away from every bookmark. Both are held here, in both directions.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-prefixes.sh <top source directory> [build directory]" >&2
	exit 2
}

AREAS="$SRC/src/httpd/apppaths.cpp"
NAV="$SRC/data/ni-web/app/nav.js"
[ -r "$AREAS" ] || { echo "check-web-prefixes.sh: cannot read $AREAS" >&2; exit 1; }

# The list in the server, read off the one array that states it. Everything
# between the opening brace and the closing one, so that a name on a line of its
# own counts the same as one beside its neighbours.
served=$(sed -n '/^const char \*const kAreas\[\] = {/,/^};/p' "$AREAS" \
	| grep -oE '"[^"]+"' | tr -d '"' | sort)
[ -n "$served" ] || {
	echo "check-web-prefixes.sh: no kAreas array in $AREAS," >&2
	echo "  so nothing here can say which paths this server answers with the page" >&2
	exit 1
}

# A name that is not a bare first segment would be compared against something
# the page cannot spell, and the rule in the server reads exactly one segment.
for a in $served; do
	case "$a" in
	*/* | "" | .* )
		echo "check-web-prefixes.sh: '$a' in $AREAS is not a first path segment" >&2
		exit 1;;
	esac
done

[ -r "$NAV" ] || {
	echo "check-web-prefixes.sh: $NAV is not in the tree" >&2
	exit 1
}

# The list in the page, read off the one line that states it flat. Not gathered
# from the seven tables: nav.js writes it out separately and says why, and the
# reason is this scan. A list that exists only as the sum of seven files cannot
# be read without running the page, and nothing here runs a page.
navigated=$(sed -n "s/^export const ids = \\[\\(.*\\)\\];*$/\\1/p" "$NAV" \
	| grep -oE "['\"][^'\"]+['\"]" | tr -d "'\"" | sort)
[ -n "$navigated" ] || {
	echo "check-web-prefixes.sh: $NAV states no 'export const ids' list," >&2
	echo "  so nothing here can say which paths the page navigates to" >&2
	exit 1
}

# Compared through two files rather than through a shell that can substitute a
# process, because this runs under whatever /bin/sh the host has.
tmp=$(mktemp -d) || { echo "check-web-prefixes.sh: no temporary directory" >&2; exit 1; }
trap 'rm -rf "$tmp"' EXIT INT TERM
printf '%s\n' "$served" > "$tmp/served"
printf '%s\n' "$navigated" > "$tmp/navigated"

only_served=$(comm -23 "$tmp/served" "$tmp/navigated")
only_page=$(comm -13 "$tmp/served" "$tmp/navigated")

bad=0
[ -z "$only_served" ] || {
	echo "check-web-prefixes.sh: this server answers these with the page and the" >&2
	echo "  page navigates to none of them:" >&2
	printf '  %s\n' $only_served >&2
	bad=1
}
[ -z "$only_page" ] || {
	echo "check-web-prefixes.sh: the page navigates to these and this server" >&2
	echo "  answers none of them with the page, so each reloads into a 404:" >&2
	printf '  %s\n' $only_page >&2
	bad=1
}
[ "$bad" = 0 ] || exit 1

exit 0

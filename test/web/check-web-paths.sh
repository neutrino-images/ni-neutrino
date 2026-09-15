#!/bin/sh
# Every address the web interface asks for, held to the document this server
# writes about itself.
#
# The interface is the one consumer of this API that nothing else checks: a route
# renamed in src/httpd/ep/ep_*.cpp breaks a compile for every caller inside this tree
# and breaks nothing at all for a page that spells the path in a string. What such
# a page does instead is answer 404 in a browser nobody is looking at. So the
# paths are read out of the source and compared with test/web/openapi.json, which
# is built out of the very tables the server routes on.
#
# WHAT IS CHECKED, AND WHY IT IS THESE THREE THINGS.
#
# Every pair of a method and a path: the method has to be one the document answers
# on that path. This catches a typo and a route another stream renamed.
#
# Every remaining "/api/..." string: it has to be at least the beginning of a path
# the document knows. These are the prefixes an event invalidates and the prefixes
# a write says it touches, and a typo in one of them is a screen that silently
# stops refreshing.
#
# Nothing speaks to the old surface, and nothing outside api.js reaches the
# network by itself. Both are rules the application is built on: the second token,
# the one retry and the problem shape all live in that one file.
#
# WHAT THIS CANNOT SEE, and what the application therefore does not do: a path
# that is not a literal. api('DELETE', '/api/v1/timers/' + id) leaves this scan a
# path of "/api/v1/timers/", which is not a path the document has, so it is
# refused here; the spelling that works is the document's own template with the
# value beside it. That is the whole reason the client takes a template.
#
# Usage: check-web-paths.sh SRCDIR [OPENAPI]
#
# OPENAPI defaults to openapi.json in the working directory, which is where the
# half's own rule builds it (test/web/Makefile.am), and falls back to the one
# beside this script.
set -e

# Read as bytes. A name in a catalogue or a comment can carry a byte that is
# not a character in the caller's locale, and every pattern below is ASCII.
LC_ALL=C
export LC_ALL

SRCDIR="$1"
[ -n "$SRCDIR" ] || { echo "usage: check-web-paths.sh SRCDIR [OPENAPI]" >&2; exit 1; }
[ -d "$SRCDIR" ] || { echo "check-web-paths: $SRCDIR fehlt" >&2; exit 1; }

HERE=`dirname "$0"`

# A directory, because the loop in test/web/Makefile.am hands every check of this
# family the same two arguments and the second one is the build directory. The
# document is built there under its own name, so a directory is an answer to
# where it is and not a wrong kind of argument.
DOC="$2"
if [ -d "$DOC" ]; then
	DOC="$DOC/openapi.json"
fi
if [ -z "$DOC" ]; then
	if [ -r openapi.json ]; then
		DOC=openapi.json
	else
		DOC="$HERE/openapi.json"
	fi
fi
[ -r "$DOC" ] || { echo "check-web-paths: $DOC fehlt (wird von test/web/Makefile.am gebaut)" >&2; exit 1; }

APP="$SRCDIR/data/ni-web"
if [ ! -d "$APP/app" ]; then
	echo "check-web-paths: $APP/app fehlt" >&2
	exit 1
fi

# Every module this tree writes, which is the application and the display at
# /info beside it. The runtime under vendor/ and the reader under swagger/ come
# out of published archives and are not ours to hold to this document.
FILES=`find "$APP" -name '*.js' \
	| grep -v "^$APP/vendor/" \
	| grep -v "^$APP/swagger/" \
	| sort`
if [ -z "$FILES" ]; then
	# Not a pass dressed up as one: the directory is there and carries no
	# application yet, which is the state between the delivery stream landing
	# and the first screen landing. Said out loud so that a run nobody looked
	# at cannot be mistaken for a run that checked something.
	echo "web paths: keine Anwendungsdateien unter $APP, nichts zu pruefen"
	exit 0
fi

WORK=`mktemp -d`
trap 'rm -rf "$WORK"' EXIT

# ---------------------------------------------------------------- the source
#
# Comments come out first, keeping one output line per input line so a path named
# in a comment is not held to the document and a line number still means
# something. One pass over the characters rather than a pass per form, because
# whichever of a comment opener and a quote comes first swallows the other and two
# passes get that backwards.
#
# A regular expression literal is read here as ordinary characters, which is right
# for every one in this application and would be wrong for one carrying a quote or
# a pair of slashes inside it. The application writes none, and this is the reason
# it writes none.
strip_comments() {
	awk '
		{
			line = $0; out = ""; n = length(line); i = 1
			while (i <= n) {
				c = substr(line, i, 1)
				d = substr(line, i, 2)
				if (inblock) {
					if (d == "*/") { inblock = 0; i += 2 } else { i++ }
					continue
				}
				if (instr) {
					if (c == "\\") { out = out substr(line, i, 2); i += 2; continue }
					if (c == q) instr = 0
					out = out c
					i++
					continue
				}
				if (d == "//") break
				if (d == "/*") { inblock = 1; i += 2; continue }
				if (c == "\"" || c == "'"'"'" || c == "`") { instr = 1; q = c; out = out c; i++; continue }
				out = out c
				i++
			}
			instr = 0
			print out
		}
	' "$1"
}

: > "$WORK/pairs"
: > "$WORK/literals"
: > "$WORK/bare"

for f in $FILES; do
	base=`basename "$f"`
	strip_comments "$f" > "$WORK/code"

	# The old surface, anywhere at all.
	if grep -q '/control/' "$WORK/code"; then
		echo "check-web-paths: $f spricht /control/" >&2
		exit 1
	fi

	# The network, from anywhere but the one file that is allowed to reach it.
	if [ "$base" != "api.js" ] && grep -q 'fetch[[:space:]]*(' "$WORK/code"; then
		echo "check-web-paths: $f ruft fetch; das gehoert in api.js" >&2
		exit 1
	fi
	if grep -q 'XMLHttpRequest' "$WORK/code"; then
		echo "check-web-paths: $f benutzt XMLHttpRequest" >&2
		exit 1
	fi
	# The stream is one stream for the whole page, and it is opened in one
	# place for the same reason every other call is made in one place.
	if [ "$base" != "events.js" ] && grep -q 'EventSource' "$WORK/code"; then
		echo "check-web-paths: $f oeffnet einen EventSource; das gehoert in events.js" >&2
		exit 1
	fi

	# A call is flattened first, because one spanning several lines is the
	# ordinary way to write one and the method is rarely on the line that
	# carries the path.
	tr '\n' ' ' < "$WORK/code" | tr -s '[:space:]' ' ' > "$WORK/flat"

	# The pairs: a call whose first two values are a method and a path.
	# Whatever the call is named, because the store relays for every screen and
	# the name at the call site is store.watch as often as it is api.
	grep -oE "\([[:space:]]*['\"](GET|HEAD|OPTIONS|POST|PUT|PATCH|DELETE)['\"][[:space:]]*,[[:space:]]*['\"]/[^'\"]*['\"]" "$WORK/flat" |
		sed -e "s/[()'\"]/ /g" -e 's/,/ /g' -e 's/^[[:space:]]*//' -e 's/[[:space:]]\{1,\}/ /g' |
		while read -r method path rest; do
			[ -n "$path" ] || continue
			echo "$method $path" >> "$WORK/pairs"
		done

	# And every address literal in the file, whether it was part of a pair or
	# stands on its own as a prefix.
	grep -oE "['\"]/api/[^'\"]*['\"]" "$WORK/flat" | tr -d "'\"" >> "$WORK/literals" || true
done

sort -u "$WORK/pairs" > "$WORK/pairs.u"
sort -u "$WORK/literals" > "$WORK/literals.u"

# --------------------------------------------------------------- the document
#
# The document is one line of compact JSON, so it is walked rather than read by
# lines: a key at the first level under "paths" is a path, a key at the level
# under that is a method it answers, and everything deeper belongs to the
# operation and is not looked at. Strings are walked with their escapes so that
# a brace inside a description cannot move the depth.
awk '
	function emit(path, method) { print path " " toupper(method) }
	{
		s = $0
		n = length(s)
		start = index(s, "\"paths\":")
		if (start == 0) { exit 1 }
		i = start + 8
		while (i <= n && substr(s, i, 1) != "{") i++
		depth = 0
		while (i <= n) {
			c = substr(s, i, 1)
			if (c == "\"") {
				value = ""
				i++
				while (i <= n) {
					c = substr(s, i, 1)
					if (c == "\\") { i += 2; value = value "?"; continue }
					if (c == "\"") { i++; break }
					value = value c
					i++
				}
				j = i
				while (j <= n && substr(s, j, 1) == " ") j++
				if (substr(s, j, 1) == ":") {
					if (depth == 1) { path = value }
					else if (depth == 2 && path != "") { emit(path, value) }
				}
				continue
			}
			if (c == "{") { depth++; i++; continue }
			if (c == "}") { depth--; if (depth == 0) break; i++; continue }
			i++
		}
	}
' "$DOC" | sort -u > "$WORK/routes"

if [ ! -s "$WORK/routes" ]; then
	echo "check-web-paths: aus $DOC kam keine einzige Route" >&2
	echo "  Der Scan ist kaputt, nicht der Baum." >&2
	exit 1
fi

cut -d' ' -f1 "$WORK/routes" | sort -u > "$WORK/paths"

# ------------------------------------------------- what is written ahead of it
#
# ONE ADDRESS THIS TREE ASKS FOR AND DOES NOT YET ANSWER.
#
# The display at /info draws the two marks the page it replaces called ECM and
# CI+, and what says whether either is on is a route being written in another
# branch of the same round. The contract is settled, so the page is written
# against it rather than merged half finished; a box without the route refuses the
# address and the page draws both marks grey.
#
# AN IDENTITY AND NOT A TOLERANCE, which is why this is allowed to exist. A line
# here whose address the document has started answering is a fault of its own,
# reported below, so the merge that brings the route in cannot leave the exception
# standing. Empty is where this list belongs.
AHEAD=''

printf '%s\n' "$AHEAD" | grep -v '^$' | sort -u > "$WORK/ahead"

# ------------------------------------------------------------- the comparison
bad=0

while read -r method path; do
	[ -n "$method" ] || continue
	if ! grep -Fqx "$path $method" "$WORK/routes"; then
		if grep -Fqx "$method $path" "$WORK/ahead"; then
			continue
		fi
		if grep -Fqx "$path" "$WORK/paths"; then
			echo "check-web-paths: $method $path - das Dokument kennt den Pfad, aber nicht diese Methode" >&2
		else
			echo "check-web-paths: $method $path - diesen Pfad kennt das Dokument nicht" >&2
		fi
		bad=1
	fi
done < "$WORK/pairs.u"

# And the other direction, which is what keeps the list above from rotting.
while read -r method path; do
	[ -n "$method" ] || continue
	if grep -Fqx "$path $method" "$WORK/routes"; then
		echo "check-web-paths: $method $path - das Dokument beantwortet die Adresse jetzt; nimm sie aus AHEAD heraus" >&2
		bad=1
	fi
done < "$WORK/ahead"

# What is left over is a prefix: what an event invalidates, what a write says
# it touches. It has to be the beginning of at least one path the document
# knows, or it names nothing and nothing will ever be thrown away for it.
cut -d' ' -f2 "$WORK/pairs.u" | sort -u > "$WORK/pairpaths"
while read -r literal; do
	[ -n "$literal" ] || continue
	if grep -Fqx "$literal" "$WORK/pairpaths"; then
		continue
	fi
	if awk -v want="$literal" 'index($0, want) == 1 { found = 1 } END { exit found ? 0 : 1 }' "$WORK/paths"; then
		continue
	fi
	echo "check-web-paths: '$literal' ist weder ein Pfad noch der Anfang eines Pfades des Dokuments" >&2
	bad=1
done < "$WORK/literals.u"

[ "$bad" = 0 ] || exit 1

calls=`wc -l < "$WORK/pairs.u" | tr -d ' '`
routes=`wc -l < "$WORK/routes" | tr -d ' '`
prefixes=`wc -l < "$WORK/literals.u" | tr -d ' '`
ahead=`wc -l < "$WORK/ahead" | tr -d ' '`
echo "web paths: $calls Aufrufe und $prefixes Adressliterale gegen $routes Routen geprueft, $ahead davon vorgeschrieben"

#!/bin/sh
# The module graph of the web pages, and the document that starts it.
#
# Four faults, each of which shows in a browser as a line in the console and
# nowhere else, and three of which only show on the one address nobody opened
# while writing the screen.
#
# THE BARE NAME. The runtime is loaded as modules straight from the server, so a
# specifier is resolved by the browser and not by a bundler. "preact" is not an
# address and nothing answers it. The four files the pages load are rewritten when
# they are installed, and this holds everything written here to the same rule.
#
# THE ADDRESS THAT IS NOT THERE. A relative specifier that names a file nobody
# wrote leaves a screen that loads on every address except the one that opens it.
#
# THE RELATIVE ADDRESS IN THE DOCUMENT. The document is answered at / and at
# /channels/12ab alike, because the server hands the pages out for a path the
# history wrote. A src of ./app/main.js is therefore /app/main.js on one address
# and /channels/app/main.js on another, and the second is a 404.
#
# THE SCRIPT OR THE STYLE IN THE DOCUMENT. The page states its own content policy
# in a meta element, with script-src 'self' and no hash and no nonce, which is
# worth stating only while there is nothing inline to allow. That is also why the
# runtime is loaded by address rather than through an import map, since a map is
# an inline script.
#
# The one address in the tree that is allowed to be root absolute is the runtime
# itself, and only in the file that names it: those four files are delivered by
# the buildsystem and are not in this tree. Everything else reaches them through
# that file, which is what keeps a version in one place.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-imports.sh <top source directory>" >&2
	exit 2
}

ROOT="$SRC/data/ni-web"
# The documents this tree writes: the application, and the display at /info.
# Both state their own content policy and both are read the same way, because
# the four faults above are the same in both.
DOCS="index.html info/index.html"
RUNTIME="app/runtime.js"

[ -d "$ROOT" ] || { echo "check-web-imports.sh: cannot read $ROOT" >&2; exit 1; }
for one in $DOCS; do
	[ -r "$ROOT/$one" ] || { echo "check-web-imports.sh: cannot read $ROOT/$one" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# The five the buildsystem delivers, spelled as the rewritten bytes spell them.
# The fifth is not loaded with the page: runtime.js reaches it through an
# address inside a call, so it is fetched by the one screen that plays a
# channel from an address somewhere else and by nothing else.
cat > "$tmp/vendor" <<'EOF'
/vendor/preact.module.js
/vendor/hooks.module.js
/vendor/htm.module.js
/vendor/preact-router.module.js
/vendor/hls.module.js
EOF

# Every module of the application, which is everything under the pages except
# what the buildsystem puts there.
find "$ROOT" -name '*.js' -print \
	| grep -v "^$ROOT/vendor/" \
	| grep -v "^$ROOT/swagger/" \
	| LC_ALL=C sort > "$tmp/modules"

modules=`wc -l < "$tmp/modules" | tr -d ' '`
[ "$modules" -gt 0 ] || {
	echo "check-web-imports.sh: no module under $ROOT, and the pages are modules" >&2
	exit 1
}

# One line per specifier: the file it is in, the line it is on, and the address it
# names. Both spellings of a string and both ways of asking for a module.
#
# The comments come out first, and each module is read on its own so the name
# printed is its own and not that of a stripped copy: the check below resolves a
# relative address against the module it stands in, and a temporary path resolves
# to nothing. Literals stay, because what is read here is the text of one.
#
# A prose comment carries the word "from" beside a quotation often enough that a
# scan of the raw bytes reads a sentence as a module. One did.
#
# The comment stripper is shared with the suite next door and is reached from here
# rather than kept in two copies.
HERE=`dirname "$0"`
STRIP="$HERE/../unit/scan/strip-comments.awk"
cat > "$tmp/specs.awk" <<'AWKEOF'

	{
		line = $0
		while (match(line, /(from|import)[ \t]*\(?[ \t]*("[^"]*"|'[^']*')/)) {
			piece = substr(line, RSTART, RLENGTH)
			line = substr(line, RSTART + RLENGTH)
			if (match(piece, /("[^"]*"|'[^']*')/)) {
				spec = substr(piece, RSTART + 1, RLENGTH - 2)
				printf "%s\t%d\t%s\n", name, FNR, spec
			}
		}
	}
AWKEOF

: > "$tmp/specs"
while read -r m; do
	awk -v keepstrings=1 -f "$STRIP" "$m" \
		| awk -v name="$m" -f "$tmp/specs.awk" >> "$tmp/specs"
done < "$tmp/modules"

specs=`wc -l < "$tmp/specs" | tr -d ' '`
[ "$specs" -gt 0 ] || {
	echo "check-web-imports.sh: not one specifier in $modules modules, so this read nothing" >&2
	exit 1
}

bad=0

# A relative address, resolved the way a browser resolves it: against the
# address of the module it is written in, and not against the document.
resolve() {
	dir=`dirname "$1"`
	awk -v dir="$dir" -v spec="$2" '
		BEGIN {
			n = split(dir "/" spec, part, "/")
			top = 0
			for (i = 1; i <= n; i++) {
				if (part[i] == "" || part[i] == ".")
					continue
				if (part[i] == "..") {
					if (top > 0)
						top--
					continue
				}
				stack[++top] = part[i]
			}
			out = ""
			for (i = 1; i <= top; i++)
				out = out "/" stack[i]
			print out
		}'
}

while IFS='	' read -r file line spec; do
	short=`echo "$file" | sed "s|^$SRC/||"`
	case "$spec" in
		./*|../*)
			target=`resolve "$file" "$spec"`
			[ -f "$target" ] || {
				echo "$short:$line: $spec names a file that is not there" >&2
				bad=`expr $bad + 1`
			}
			;;
		/*)
			if ! grep -qx -- "$spec" "$tmp/vendor"; then
				echo "$short:$line: $spec is root absolute and is not one of the four the buildsystem delivers" >&2
				bad=`expr $bad + 1`
			elif [ "$short" != "data/ni-web/$RUNTIME" ]; then
				echo "$short:$line: $spec is the runtime, and the runtime is named in $RUNTIME and reached from there" >&2
				bad=`expr $bad + 1`
			fi
			;;
		*)
			echo "$short:$line: $spec is a bare name, and a browser resolves no such thing" >&2
			bad=`expr $bad + 1`
			;;
	esac
done < "$tmp/specs"

# The documents. Every address one holds is root absolute, and nothing in one
# is a script or a style.
for one in $DOCS; do
	DOC="$ROOT/$one"
	addresses=`grep -oE '(src|href)="[^"]*"' "$DOC" | sed 's/^[a-z]*="//; s/"$//' || true`
	[ -n "$addresses" ] || {
		echo "check-web-imports.sh: no address in $DOC, so this read nothing" >&2
		exit 1
	}

	for address in $addresses; do
		case "$address" in
			/*) ;;
			*)
				echo "data/ni-web/$one: $address does not begin with a slash, and this document is answered under more than one path" >&2
				bad=`expr $bad + 1`
				;;
		esac
	done

	# A script element with anything between its tags. The one each document has
	# carries an address and no body.
	if awk '
		BEGIN { RS = "</script>"; found = 0 }
		/<script/ {
			body = substr($0, index($0, "<script"))
			sub(/<script[^>]*>/, "", body)
			gsub(/[ \t\r\n]/, "", body)
			if (body != "")
				found = 1
		}
		END { exit found ? 0 : 1 }
	' "$DOC"; then
		echo "data/ni-web/$one: a script element carries a body, and the policy this page states allows none" >&2
		bad=`expr $bad + 1`
	fi

	if grep -qE '<style|style="|style='"'" "$DOC"; then
		echo "data/ni-web/$one: a style element or a style attribute, and the policy this page states allows neither" >&2
		bad=`expr $bad + 1`
	fi

	if grep -qE '<[^>]+[ \t]on[a-z]+[ \t]*=' "$DOC"; then
		echo "data/ni-web/$one: an event attribute, which is a script in an attribute" >&2
		bad=`expr $bad + 1`
	fi
done

[ "$bad" -eq 0 ] || {
	echo "check-web-imports.sh: $bad faults" >&2
	exit 1
}

docs=`printf '%s\n' $DOCS | wc -l | tr -d ' '`
echo "check-web-imports.sh: $specs specifiers in $modules modules resolve, and $docs documents are root absolute and free of script and style"

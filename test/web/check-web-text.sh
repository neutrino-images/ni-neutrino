#!/bin/sh
# Every catalogue the web pages carry, held to the two faults the old one has.
#
# The pages keep their words one file per part, so ten of them can be written at once
# without anybody editing a file somebody else is in. The price is that nothing holds
# the files to each other, and both faults below are ones the old catalogue has today
# and nothing ever reported.
#
# The first is a key in one language and not in the other. data/y-web's two
# catalogues have 498 keys each and disagree about two of them, which on screen is a
# word in the wrong language for whoever picked that language.
#
# The second is an HTML entity in a value. 53 lines of the German y-web catalogue
# carry one, which was right when a value went into a page as markup and is wrong
# here: these pages draw text as text, so &uuml; reaches the screen as the six
# characters it is spelled with.
#
# The shape of a catalogue is the contract, and this reads it rather than a person:
# one object per language, one key per line, the value a whole sentence with {name}
# in it. A line inside a language that is not a key and a value is refused rather
# than skipped, because a parser that skips what it does not understand is one that
# reports a file it never read as clean.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-text.sh <top source directory>" >&2
	exit 2
}

# The whole of what this tree writes: the application, and the display at /info
# beside it. Both keep their words the same way, so both are held to it. The
# runtime under vendor/ and the reader under swagger/ come out of published
# archives and carry no catalogue of ours.
APP="$SRC/data/ni-web"
[ -d "$APP/app" ] || { echo "check-web-text.sh: cannot read $APP/app" >&2; exit 1; }

ours() {
	find "$APP" "$@" -print \
		| grep -v "^$APP/vendor/" \
		| grep -v "^$APP/swagger/"
}

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

ours -name '*.text.js' | LC_ALL=C sort > "$tmp/files"
files=`wc -l < "$tmp/files" | tr -d " "`
[ "$files" -gt 0 ] || {
	echo "check-web-text.sh: no catalogue under $APP, and the frame has one" >&2
	exit 1
}

pairs=0
bad=0

while read -r file; do
	short=`echo "$file" | sed "s|^$SRC/||"`

	# One record per key: the language, the key, and the value it was given.
	# The language block is entered on its own line and left on its own line,
	# and everything between the two has to be one key.
	awk -v file="$short" '
		/^\t(de|en): \{$/ {
			lang = $1
			sub(":", "", lang)
			next
		}
		/^\t\},?$/ { lang = ""; next }
		lang == "" { next }
		/^[ \t]*(\/\/|\/\*|\*)/ { next }
		/^[ \t]*$/ { next }
		{
			if ($0 !~ /^\t\t'"'"'[^'"'"']+'"'"': '"'"'.*'"'"',?$/) {
				printf "%s:%d: not a key and a value, and this check reads the shape: %s\n", file, NR, $0 > "/dev/stderr"
				exit 3
			}
			key = $0
			sub(/^\t\t'"'"'/, "", key)
			sub(/'"'"'.*$/, "", key)
			value = substr($0, index($0, "'"'"': '"'"'") + 4)
			sub(/'"'"',?$/, "", value)
			printf "%s\t%s\t%s\n", lang, key, value
		}
	' "$file" > "$tmp/records" || exit 1

	for lang in de en; do
		awk -F'\t' -v want="$lang" '$1 == want { print $2 }' "$tmp/records" | LC_ALL=C sort > "$tmp/$lang"
		n=`wc -l < "$tmp/$lang" | tr -d " "`
		[ "$n" -gt 0 ] || {
			echo "$short: no $lang, and a catalogue states both" >&2
			bad=`expr $bad + 1`
		}
		u=`LC_ALL=C sort -u < "$tmp/$lang" | wc -l | tr -d " "`
		[ "$u" -eq "$n" ] || {
			echo "$short: $lang names a key twice, so one of the two is dead" >&2
			bad=`expr $bad + 1`
		}
	done

	comm -23 "$tmp/de" "$tmp/en" > "$tmp/onlyde"
	comm -13 "$tmp/de" "$tmp/en" > "$tmp/onlyen"

	while read -r key; do
		[ -n "$key" ] || continue
		echo "$short: $key is in de and not in en" >&2
		bad=`expr $bad + 1`
	done < "$tmp/onlyde"

	while read -r key; do
		[ -n "$key" ] || continue
		echo "$short: $key is in en and not in de" >&2
		bad=`expr $bad + 1`
	done < "$tmp/onlyen"

	# An entity in a value. Named with the key it is in, because a file with
	# four hundred lines and a count is a file somebody has to search.
	while IFS='	' read -r lang key value; do
		case "$value" in
			*'&'*';'*)
				if echo "$value" | grep -qE '&[A-Za-z][A-Za-z0-9]*;'; then
					echo "$short: $lang $key carries an HTML entity, and a value is drawn as text: $value" >&2
					bad=`expr $bad + 1`
				fi
				;;
		esac
	done < "$tmp/records"

	# Kept for the scan below, in the language both are held to carry.
	awk -F'\t' '$1 == "de" { printf "%s\t%s\n", file, $2 }' file="$short" "$tmp/records" >> "$tmp/declared"

	here=`wc -l < "$tmp/records" | tr -d " "`
	pairs=`expr $pairs + $here`
done < "$tmp/files"

# ---------------------------------------------------------------------------
# And the third fault, which is the one neither of the two above can see: a key that
# is in both languages, carries no entity, and no screen ever asks for it.
#
# Sixteen of them had been written and translated, thirty two values that never reach
# a screen, and two more were the words for a session that has run out with nothing
# drawing them. Held against each other the catalogues were perfect. That is the same
# halving the check next door had, where a name went into a query and nothing read
# it.
#
# ONE DIRECTION AND NOT THE OTHER. A key that nothing asks for is dead weight and is
# reported. A key asked for that no catalogue declares is not reported here, because
# it cannot be told from the many strings on these pages that are not keys at all,
# and because the page already draws such a key as itself, on screen.
#
# WHAT COUNTS AS ASKING, and why it is not the call. A key reaches t() or word()
# spelled out, out of a table of them, out of a field of a navigation entry, or as a
# variable the caller was handed. Every one of those spellings starts as the key
# written down somewhere in these pages, so that is what is looked for: the key as a
# literal anywhere under app/ outside the catalogues. Reading the call sites instead
# would report as dead every key that reaches one through a variable.
#
# And the form is held, so that stays true. A key handed over may be written out,
# referred to, or a prefix with something appended; a call there is not allowed,
# because a key a function builds out of pieces is a key that appears nowhere whole.
APPFILES="$tmp/appfiles"
ours -type f -name '*.js' ! -name '*.text.js' | LC_ALL=C sort > "$APPFILES"
[ -s "$APPFILES" ] || {
	echo "check-web-text.sh: no page under $APP asks for a word, and fifteen catalogues stand there" >&2
	exit 1
}

STRIP="$SRC/test/unit/scan/strip-comments.awk"
[ -r "$STRIP" ] || { echo "check-web-text.sh: cannot read $STRIP" >&2; exit 1; }

# Comments first. A key named in the sentence explaining why it went would
# otherwise keep itself alive. keepstrings=1 because the keys are the literals.
xargs awk -v keepstrings=1 -f "$STRIP" < "$APPFILES" > "$tmp/pages"

# A call where a key belongs. Named with the file and the line, because this is
# about a line somebody has to rewrite and not about a count.
xargs grep -nE "[^A-Za-z0-9_.$]t\([A-Za-z_][A-Za-z0-9_]*, *[A-Za-z_][A-Za-z0-9_.]*\(|[^A-Za-z0-9_.$]word\( *[A-Za-z_][A-Za-z0-9_.]*\(" \
	< "$APPFILES" > "$tmp/built" 2>/dev/null || true
if [ -s "$tmp/built" ]; then
	while read -r line; do
		echo "check-web-text.sh: a word is asked for under a key a call builds, and a key built out of pieces stands in no catalogue to be found: ${line#$SRC/}" >&2
	done < "$tmp/built"
	echo "check-web-text.sh: write the key out, or append to one that is written out" >&2
	exit 1
fi

# Every key spelled anywhere on a page, and every prefix something is appended
# to. A prefix covers the keys under it: which of them is asked for is decided
# by a value the box sends, and there is no reading that off the page.
grep -oE "'[A-Za-z][A-Za-z0-9_.]*'" "$tmp/pages" | tr -d "'" | LC_ALL=C sort -u > "$tmp/spelled"
grep -oE "'[A-Za-z][A-Za-z0-9_.]*\.' *\+" "$tmp/pages" | sed "s/' *+.*//; s/^'//" | LC_ALL=C sort -u > "$tmp/heads"

LC_ALL=C sort -u "$tmp/declared" > "$tmp/declared.sorted"
[ -s "$tmp/declared.sorted" ] || {
	echo "check-web-text.sh: no key was read out of any catalogue, so this held nothing" >&2
	exit 1
}
[ -s "$tmp/spelled" ] || {
	echo "check-web-text.sh: no page spells a key at all, and every screen draws words" >&2
	exit 1
}
awk -F'\t' -v spelledfile="$tmp/spelled" -v headsfile="$tmp/heads" '
	BEGIN {
		while ((getline line < spelledfile) > 0)
			spelled[line] = 1
		while ((getline line < headsfile) > 0)
			heads[++nheads] = line
	}
	{
		if ($2 in spelled)
			next
		for (i = 1; i <= nheads; i++)
			if (index($2, heads[i]) == 1)
				next
		printf "%s\t%s\n", $1, $2
	}
' "$tmp/declared.sorted" > "$tmp/unasked"

if [ -s "$tmp/unasked" ]; then
	while IFS='	' read -r file key; do
		echo "$file: $key is translated into both languages and no screen asks for it" >&2
		bad=`expr $bad + 1`
	done < "$tmp/unasked"
fi

[ "$bad" -eq 0 ] || {
	echo "check-web-text.sh: $bad faults in $files catalogues" >&2
	exit 1
}

asked=`wc -l < "$tmp/declared.sorted" | tr -d " "`
heads=`wc -l < "$tmp/heads" | tr -d " "`
echo "check-web-text.sh: $pairs values in $files catalogues, both languages, no entity; $asked keys, each spelled on a page or under one of $heads prefixes"

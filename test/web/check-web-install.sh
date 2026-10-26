#!/bin/sh
# Every file of the page reaches the box, and the rule that carries them there is
# a walk and not a list.
#
# This is the one fault in this round a build cannot report. A screen is a handful
# of files a stream adds at once; if the rule that installs them named them one by
# one, a file left out of that list compiles nothing, breaks nothing and fails
# nothing. It is simply not on the box, and the first person to find out is
# whoever opens the page and gets a blank area.
#
# So the rule is held to being a walk of the directory, and the directory is held
# to holding nothing the walk would ship as something it is not.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-install.sh <top source directory> [build directory]" >&2
	exit 2
}

WEB="$SRC/data/ni-web"
WEBMK="$WEB/Makefile.am"
# The directories here that install themselves by walking.
WALKED="app info ai"
[ -r "$WEBMK" ] || { echo "check-web-install.sh: cannot read $WEBMK" >&2; exit 1; }
for one in $WALKED; do
	[ -r "$WEB/$one/Makefile.am" ] || {
		echo "check-web-install.sh: cannot read $WEB/$one/Makefile.am" >&2
		exit 1
	}
done

# The document itself, installed by the directory above and compressed in
# place. It is what the history fallback answers with, so it is the one file
# whose absence takes every deep address of the page with it.
# Among what that line installs and not the whole of it: a rule written to be
# the only word on the line is a rule the next file added there breaks without
# anything being wrong.
grep -qE '^install_DATA[[:blank:]]*=.*\<index\.html\>' "$WEBMK" || {
	echo "check-web-install.sh: $WEBMK does not install index.html," >&2
	echo "  which is the document every address of the page is answered with" >&2
	exit 1
}
grep -qE 'gzip -9 -nf .*\<index\.html\>' "$WEBMK" || {
	echo "check-web-install.sh: $WEBMK does not store index.html compressed" >&2
	exit 1
}
# The licence is in the tree and not on the box, so a makefile that installs it
# is a makefile that grew seven kilobytes of flash nobody opens.
grep -qE '^install_DATA[[:blank:]]*=.*\<LICENSE\>' "$WEBMK" && {
	echo "check-web-install.sh: $WEBMK installs LICENSE, which belongs in the" >&2
	echo "  tree beside the files it covers and not in the flash of a box" >&2
	exit 1
}
for one in $WALKED; do
	grep -qE "^[[:blank:]]*$one[[:blank:]]*\\\\?$" "$WEBMK" || {
		echo "check-web-install.sh: $WEBMK does not descend into $one/," >&2
		echo "  so nothing under it is installed at all" >&2
		exit 1
	}
done

# ai is the MCP pages, so every line that adds it has to sit in the
# then-branch of if ENABLE_MCP, read as a word of SUBDIRS and not a spelling.
awk '
	{
		line = $0
		sub(/#.*/, "", line)
		sub(/[[:blank:]]+$/, "", line)
	}
	line ~ /^if[[:blank:]]/ {
		depth++
		split(line, f, /[[:blank:]]+/)
		cond[depth] = f[2]
		branch[depth] = "then"
		in_subdirs = 0
		next
	}
	line ~ /^else([[:blank:]]|$)/ { branch[depth] = "else"; in_subdirs = 0; next }
	line ~ /^endif([[:blank:]]|$)/ { depth--; in_subdirs = 0; next }
	{
		trimmed = line
		sub(/^[[:blank:]]+/, "", trimmed)
		if (trimmed ~ /^SUBDIRS[[:blank:]]*\+?=/) {
			in_subdirs = 1
			sub(/^SUBDIRS[[:blank:]]*\+?=/, "", trimmed)
		} else if (!in_subdirs) {
			next
		}
		cont = (trimmed ~ /\\[[:blank:]]*$/)
		sub(/\\[[:blank:]]*$/, "", trimmed)
		n = split(trimmed, words, /[[:blank:]]+/)
		for (w = 1; w <= n; w++) {
			if (words[w] == "ai") {
				ok = 0
				for (i = 1; i <= depth; i++)
					if (cond[i] == "ENABLE_MCP" && branch[i] == "then") ok = 1
				if (ok) good = 1
				else bad = 1
			}
		}
		if (!cont) in_subdirs = 0
	}
	END { if (bad) exit 2; if (!good) exit 3; exit 0 }
' "$WEBMK" && mcp_rc=0 || mcp_rc=$?
case $mcp_rc in
2)
	echo "check-web-install.sh: $WEBMK adds ai to SUBDIRS outside the" >&2
	echo "  then-branch of if ENABLE_MCP, so a build without the switch" >&2
	echo "  would ship the MCP pages anyway" >&2
	exit 1
	;;
3)
	echo "check-web-install.sh: $WEBMK does not add ai to SUBDIRS inside" >&2
	echo "  the then-branch of if ENABLE_MCP, so a build without the switch" >&2
	echo "  would ship the MCP pages anyway" >&2
	exit 1
	;;
esac

# The walk. Named by the automake hook it is written as, so that a rule which
# stopped being a hook is a failure rather than a rule nothing calls.
for one in $WALKED; do
	MK="$WEB/$one/Makefile.am"
	DIR="$WEB/$one"

	grep -qE '^install-data-local:' "$MK" || {
		echo "check-web-install.sh: $MK has no install-data-local rule" >&2
		exit 1
	}
	# The recipe of that rule as one line per command, the continuations joined,
	# because where a walk breaks its line is the author's business and a check
	# that reads one physical line holds the file to a layout rather than to what
	# it does.
	walk=$(awk '
		/^install-data-local:/ { on = 1; next }
		on && /^\t/ {
			line = $0
			sub(/^\t/, "", line)
			buf = cont ? buf " " line : line
			cont = (buf ~ /\\$/)
			if (cont)
				sub(/\\$/, "", buf)
			else {
				print buf
				buf = ""
			}
			next
		}
		on { exit }
	' "$MK")
	printf '%s\n' "$walk" | grep -qE 'cd \$\(srcdir\) &&.*find \. ' || {
		echo "check-web-install.sh: the rule in $MK does not walk \$(srcdir)," >&2
		echo "  so a build outside the source tree installs a different set of files" >&2
		exit 1
	}
	# Read as a set and not as a spelling: which order the tests of the walk are
	# written in is the author's business, and a check that reads one order holds
	# the file to a habit rather than to what it does.
	printf '%s\n' "$walk" | grep -qE 'find \. [^|]*-type f' || {
		echo "check-web-install.sh: the walk in $MK does not ask for files," >&2
		echo "  so it would hand a directory to the compressor" >&2
		exit 1
	}

	# And it is a walk and nothing else. A hand written list here would be the
	# one file every stream of this round has to edit, and the file left out of
	# it is the fault above.
	listed=$(grep -nE '^(nobase_)?(install|dist)[a-zA-Z_]*_(DATA|SCRIPTS)[[:blank:]]*=' "$MK" || true)
	[ -z "$listed" ] || {
		echo "check-web-install.sh: $MK names files one by one:" >&2
		printf '%s\n' "$listed" >&2
		echo "  the rule has to be the walk, or a file added to a screen is a file" >&2
		echo "  nobody ships and no build reports" >&2
		exit 1
	}

	# What the walk passes over: names written into the walk itself, and the
	# shared list in ../exclude.am where the walk reads that. The list reaches
	# find as patterns only with globbing off, or the shell expands *.orig against
	# the directory the walk stands in, and it is applied only where the find is
	# handed what the loop built.
	skipped=$(printf '%s\n' "$walk" | grep -oE "! -name '[^']*'" | sed "s/.*! -name '//; s/'$//" || true)
	case "$walk" in
	*'$(exclude_files)'*)
		grep -qE '^include[[:blank:]]+\$\(srcdir\)/\.\./exclude\.am[[:blank:]]*$' "$MK" || {
			echo "check-web-install.sh: the walk in $MK reads \$(exclude_files)" >&2
			echo "  but $MK does not include \$(srcdir)/../exclude.am, so it passes over nothing" >&2
			exit 1
		}
		printf '%s\n' "$walk" | grep -qE 'set -f &&.*for [a-z]+ in \$\(exclude_files\)' || {
			echo "check-web-install.sh: the walk in $MK reads \$(exclude_files) with globbing on," >&2
			echo "  so the shell expands the patterns before find sees them" >&2
			exit 1
		}
		printf '%s\n' "$walk" | grep -qE 'find \. [^|]*\$\$ex' || {
			echo "check-web-install.sh: the walk in $MK builds its exclusions" >&2
			echo "  and does not hand them to find" >&2
			exit 1
		}
		EXCLUDE="$WEB/exclude.am"
		[ -r "$EXCLUDE" ] || { echo "check-web-install.sh: cannot read $EXCLUDE" >&2; exit 1; }
		shared=$(awk '
			{ sub(/#.*/, "") }
			/^exclude_files[[:blank:]]*=/ { on = 1; sub(/^exclude_files[[:blank:]]*=/, "") }
			on {
				more = /\\[[:blank:]]*$/
				sub(/\\[[:blank:]]*$/, "")
				print
				if (!more) exit
			}
		' "$EXCLUDE" | tr -s ' \t' '\n\n' | grep -v '^$' || true)
		[ -n "$shared" ] || {
			echo "check-web-install.sh: $EXCLUDE sets no exclude_files" >&2
			exit 1
		}
		skipped="$skipped
$shared"
		;;
	esac

	# Only names that are never part of the page may be passed over: the
	# makefiles of this directory, and what a patch that did not apply leaves
	# beside its target. Everything else under it is the page.
	set -f
	for name in $skipped; do
		case "$name" in
		Makefile*|'*.orig'|'*.rej') ;;
		*)
			echo "check-web-install.sh: the walk in $MK passes over '$name'," >&2
			echo "  which is neither a makefile of this directory nor a patch's leftover" >&2
			exit 1;;
		esac
	done
	set +f

	# The stored form is what the rule makes, so a source file already in it
	# would be stored twice and answered as a script that is a gzip stream.
	stored=$(find "$DIR" -type f -name '*.gz' -print | sed "s|^$SRC/||")
	[ -z "$stored" ] || {
		echo "check-web-install.sh: files under data/ni-web/$one are already stored:" >&2
		printf '%s\n' "$stored" >&2
		echo "  the install rule compresses what it finds, so these go out twice" >&2
		exit 1
	}

	# A file the walk would not find is a file that is written, read in the
	# browser through a relative import, and missing on the box. The walk finds
	# every regular file, so what is checked is that nothing under here is
	# something else: a link resolves on this machine and is copied as a name on
	# the box, and a directory nobody may enter stops the walk where it stands.
	odd=$(find "$DIR" ! -type f ! -type d -print | sed "s|^$SRC/||")
	[ -z "$odd" ] || {
		echo "check-web-install.sh: data/ni-web/$one holds names that are neither" >&2
		echo "  a file nor a directory, and the walk ships only files:" >&2
		printf '%s\n' "$odd" >&2
		exit 1
	}
done


exit 0

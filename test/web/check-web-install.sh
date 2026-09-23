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
# The directories under here that carry a page of their own and install
# themselves by walking: the application, and the display at /info. Both are
# held to the same rule, because the fault below is the same in both.
WALKED="app info"
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

# The walk. Named by the automake hook it is written as, so that a rule which
# stopped being a hook is a failure rather than a rule nothing calls.
for one in $WALKED; do
	MK="$WEB/$one/Makefile.am"
	DIR="$WEB/$one"

	grep -qE '^install-data-local:' "$MK" || {
		echo "check-web-install.sh: $MK has no install-data-local rule" >&2
		exit 1
	}
	walk=$(grep -E '^[[:blank:]]*@?cd \$\(srcdir\) && find \.' "$MK" || true)
	[ -n "$walk" ] || {
		echo "check-web-install.sh: the rule in $MK does not walk \$(srcdir)," >&2
		echo "  so a build outside the source tree installs a different set of files" >&2
		exit 1
	}
	# Read as a set and not as a spelling: which order the tests of the walk are
	# written in is the author's business, and a check that reads one order holds
	# the file to a habit rather than to what it does.
	printf '%s\n' "$walk" | grep -q -- '-type f' || {
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

	# What the walk passes over. Only the makefiles of this directory may be
	# passed over, because everything else under it is the page.
	skipped=$(grep -oE "! -name '[^']*'" "$MK" | sort -u)
	for name in $(printf '%s\n' "$skipped" | sed "s/.*! -name '//; s/'$//"); do
		case "$name" in
		Makefile*) ;;
		*)
			echo "check-web-install.sh: the walk in $MK passes over '$name'," >&2
			echo "  which is not one of the makefiles of this directory" >&2
			exit 1;;
		esac
	done

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

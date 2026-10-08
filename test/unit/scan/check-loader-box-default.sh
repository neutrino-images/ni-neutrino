#!/bin/sh
# A row whose default the box decides while it runs (which file systems it can make,
# which panel it has) only gives that default where the store answers NotFound. The
# running store reads the program's settings, which the loader always fills, so a
# loader that falls back to a plain number keeps that number on every box that has
# no line for the key, and the row's default never reaches it.
#
# Checked as text because the loader is the application and stands in no LDADD.
#
# For every such row the loader's fallback is not a bare number, and the box source
# the default asks is installed before the startup load.
#
# Only getInt32("key", N) with a literal N is caught. A fallback through a macro or a
# constant, or a box-decided row the loader reads with getBool or getString, passes;
# no such row exists today.
#
# An optional second argument names one file to read as the loader, for the self test.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-loader-box-default.sh <top source directory> [loader file]" >&2
	exit 2
}

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
[ -r "$STRIP" ] || { echo "check-loader-box-default.sh: cannot read $STRIP" >&2; exit 1; }

LOADER="${2:-$SRC/src/neutrino.cpp}"
[ -r "$LOADER" ] || { echo "check-loader-box-default.sh: cannot read $LOADER" >&2; exit 1; }

TABLES=`ls "$SRC"/src/coreapi/settings/settingstable_*.cpp 2>/dev/null`
[ -n "$TABLES" ] || { echo "check-loader-box-default.sh: no settings tables under $SRC" >&2; exit 1; }

# Each key with a default function that asks the box, not only the build's model flags.
KEYS=`for f in $TABLES; do awk -v keepstrings=1 -f "$STRIP" "$f"; done | awk '
	match($0, /^long [A-Za-z0-9_]+\(\)/) {
		fn = substr($0, 6, RLENGTH - 7); body[fn] = ""; infn = fn; next
	}
	infn != "" { body[infn] = body[infn] $0; if ($0 ~ /^}/) infn = "" }
	match($0, /[a-zA-Z]+Row\("[^"]+"\)/) {
		s = substr($0, RSTART, RLENGTH); sub(/^[a-zA-Z]+Row\("/, "", s); sub(/"\)$/, "", s); cur = s
	}
	match($0, /\.defaultFrom\([A-Za-z0-9_]+\)/) {
		s = substr($0, RSTART + 13, RLENGTH - 14); row[cur] = s
	}
	END {
		for (k in row)
			if (row[k] in body && body[row[k]] !~ /boxdefault::/)
				print k
	}
' | sort -u`
[ -n "$KEYS" ] || { echo "check-loader-box-default.sh: no row computes its default per box" >&2; exit 1; }

STRIPPED=`awk -v keepstrings=1 -f "$STRIP" "$LOADER"`

BAD=""
for k in $KEYS; do
	LINES=`printf '%s\n' "$STRIPPED" | grep -F "getInt32(\"$k\"," || true`
	[ -n "$LINES" ] || continue
	if printf '%s\n' "$LINES" | grep -Eq "getInt32\(\"$k\",[ 	]*-?[0-9]+[ 	]*\)"; then
		BAD="$BAD
    $k: the loader falls back to a plain number"
	fi
done

ORDER=`printf '%s\n' "$STRIPPED" | awk '
	/installRealSystemSource[ \t]*\([ \t]*\)/ { if (!load) installed = 1 }
	/loadSettingsErg[ \t]*=[ \t]*loadSetup[ \t]*\(/ { load = 1; if (!installed) print "late" }
	END { if (!load) print "noload" }
'`
case "$ORDER" in
	*late*) BAD="$BAD
    the box source is installed after the startup load" ;;
	*noload*) echo "check-loader-box-default.sh: no startup load in $LOADER" >&2; exit 1 ;;
esac

if [ -n "$BAD" ]; then
	echo "check-loader-box-default.sh: a default the box decides does not reach the loader:$BAD" >&2
	exit 1
fi

exit 0

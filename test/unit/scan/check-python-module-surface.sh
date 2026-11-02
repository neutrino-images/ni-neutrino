#!/bin/sh
# What a python plugin imports from the module, held to what the module carries.
# A plugin's first line is "from neutrino import ...", and every name there is
# resolved out of the module built in src/gui/python/pyinstance.cpp: a submodule
# each binding adds under a name of its own, the menu under both the name it is
# added by and the "menue" the lua bindings call it, and the constant tables the
# module sets. Nothing links the two sides: a name renamed in a binding, or asked
# for by a plugin that no binding answers, compiles and builds, and the box
# answers
#
#   ModuleNotFoundError: No module named 'neutrino'
#
# at the plugin's first line, which is where a plugin that cannot run at all
# stops. Both sides are read as text, so a name is named rather than counted.
#
# The module is built in two places and both are wanted: PythonInstModule builds
# it for a caller that asks, and runScript builds it before it runs a script,
# which is the one place that must. A build that leaves runScript's out leaves
# every plugin failing on its first line while everything else about it looks
# right, so the two are counted here.
#
# An optional second argument names one plugin file to read instead of the
# shipped ones, for the self test.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-python-module-surface.sh <top source directory> [plugin]" >&2
	exit 2
}

HERE=`dirname "$0"`
PYDIR="$SRC/src/gui/python"
[ -d "$PYDIR" ] || { echo "check-python-module-surface.sh: cannot read $PYDIR" >&2; exit 1; }

PLUGIN="${2:-}"
if [ -n "$PLUGIN" ]; then
	[ -r "$PLUGIN" ] || { echo "check-python-module-surface.sh: cannot read $PLUGIN" >&2; exit 1; }
	set -- "$PLUGIN"
else
	set -- "$SRC"/data/pythonplugins/*.py
	for f in "$@"; do
		[ -r "$f" ] || { echo "check-python-module-surface.sh: no python plugin to read in $SRC/data/pythonplugins" >&2; exit 1; }
	done
fi

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# What the module carries: the name a binding adds its submodule under, the name
# the menu adds its alias under, and the name of every constant table.
{
	grep -h -o 'PythonInstAddSubmodule(module, "[A-Za-z_][A-Za-z0-9_]*"' "$PYDIR"/*.cpp
	grep -h -o 'PyModule_AddObject(module, "[A-Za-z_][A-Za-z0-9_]*"' "$PYDIR"/*.cpp
	grep -h -o 'addConstTable[A-Za-z]*(module, "[A-Za-z_][A-Za-z0-9_]*"' "$PYDIR"/*.cpp
} | sed 's/^[^"]*"//; s/"$//' | LC_ALL=C sort -u > "$tmp/registered"

rn=`wc -l < "$tmp/registered"`
# Well under what the bindings carry today, so a scan that has stopped matching
# the shape reads as a fault here rather than as a quiet pass over nothing.
if [ "$rn" -lt 15 ]; then
	echo "check-python-module-surface.sh: $rn registered names read out of $PYDIR, the scan has stopped matching that shape" >&2
	exit 1
fi

# One name per line, with the line it was read on, out of every "from neutrino
# import" a plugin carries. A plugin file is read as the tree holds it, so a CR
# before the newline is taken off the name it would otherwise sit at the end of.
awk '
	/^[ \t]*from[ \t]+neutrino[ \t]+import[ \t]/ {
		line = $0
		sub(/^[ \t]*from[ \t]+neutrino[ \t]+import[ \t]+/, "", line)
		sub(/[ \t\r]*(#.*)?$/, "", line)
		n = split(line, parts, /[ \t]*,[ \t]*/)
		for (i = 1; i <= n; i++)
			if (parts[i] != "")
				printf "%s\t%s:%d\n", parts[i], FILENAME, FNR
	}
' "$@" > "$tmp/imported"

if [ -z "$PLUGIN" ]; then
	im=`wc -l < "$tmp/imported"`
	if [ "$im" -lt 2 ]; then
		echo "check-python-module-surface.sh: $im imported names read out of the shipped plugins, the scan has stopped matching that shape" >&2
		exit 1
	fi
fi

BAD=
while read -r name where; do
	[ -n "$name" ] || continue
	grep -qx "$name" "$tmp/registered" || BAD="$BAD
    $where: $name"
done < "$tmp/imported"

# runScript's own build, beside PythonInstModule's.
builds=`grep -c 'neutrino_module = buildModule();' "$PYDIR/pyinstance.cpp" || true`
if [ "$builds" -lt 2 ]; then
	BAD="$BAD
    $PYDIR/pyinstance.cpp: the module is built in $builds place(s), so a script reaches an import that was never made"
fi

if [ -n "$BAD" ]; then
	echo "check-python-module-surface.sh: a name a python plugin imports that the module does not carry, or a module a script never builds:" >&2
	printf '%s\n' "$BAD" | sed '/^$/d' >&2
	exit 1
fi

in=`wc -l < "$tmp/imported"`
echo "plugin import names held to the module's own names              $in"
echo "  of which the module carries                                   $rn"
exit 0

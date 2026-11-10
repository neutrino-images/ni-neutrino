#!/bin/sh
#
# Is everything a script is given in the script API, and no longer in the
# bindings?
#
# The script API is src/gui/scriptapi/. A binding reads its own syntax and hands
# the parameters over: the widget itself, the rule that resolves a colour a
# script names as one of the box's own, and the magic that marks such a colour
# are built in one place. This script holds that.
#
# It derives what it checks from the layer itself, so that it cannot go stale
# when something is added to it: every class the script API holds has to be the
# one the bindings build their handle on - in both of them, or one of them is
# not migrated yet. Whole modules the two bindings carry as a pair are looked at
# the same way, so that a module migrated on one side only cannot slip through.
# Where something must hold it fails; what a reader should look at, or what is
# still to come, it only reports. Two kinds of pair are not work left over, and
# both are written down below rather than migrated for the sake of the count:
# one whose work already stands in a class of the box, and one that is the
# language itself.
#
# This script lies in the layer it watches: src/gui/scriptapi/check-scriptapi.sh.
# Or with the top of the tree named, as the build calls it; a call without a
# name reads the top from where this file lies, three levels up:
#	sh src/gui/scriptapi/check-scriptapi.sh /path/to/tangos-ni
# The build calls it the second way, as src/coreapi/Makefile.am calls its own
# check-layering.sh.
#
# It is the brother of src/coreapi/check-layering.sh, which watches the layers
# of the tree; this one watches the split between the bindings and the layer
# both of them read.

# Named, or read from this file's own place: three levels up from scriptapi/ is
# the top of the tree, and both a call from inside the tree and one from a build
# outside it land on the same tree that way.
here=$(dirname "$0")

top=${1:-$here/../../..}
status=0
lua=$top/src/gui/lua
python=$top/src/gui/python
layer=$top/src/gui/scriptapi

# How many handles one binding builds for a class of the script API: it either
# derives from it, or it holds one. Only the headers are looked at, because the
# handle of a script's widget lives in one.
uses_in()
{
	dir=$1
	class=$2
	{
		grep -rl "public $class" $dir/*.h 2>/dev/null
		grep -rlF "$class *" $dir/*.h 2>/dev/null
	grep -rl "$class::" $dir 2>/dev/null
	} | sort -u | wc -l
}

# Is a module of a binding read from the script API? Its handle header, or the
# file itself, names something of the layer then.
on_the_layer()
{
	grep -q 'scriptapi/' "$@" 2>/dev/null
}

# ---------------------------------------------------------------------------
# The two kinds of pair that are not open work.
#
# A pair whose work already stands in a class of the box: both bindings read
# that class and hold nothing but their own syntax - the handle of a script's
# config file, the argument reading, the answer in the shape of the language.
# There is nothing left to move into the script API, and a wrapper over the
# core would be forwarding and nothing else. The class after the colon must be
# read by both sides and must stand outside the bindings, or this note has gone stale and it fails.
#
# A pair that is the language itself: the two sides hold different things -
# one a lua_State it creates and copies values between, the other python
# objects and their GIL - and what looks alike is the syntax of that language.
# What both do the same way is holding a thread and its five operations, and
# that would be a step of its own, not a binding migration.
# ---------------------------------------------------------------------------

ALREADY_SHARED="configfile:CConfigFile filehelpers:CFileHelpers video:CMoviePlayerGui"
LANGUAGE_ONLY="threads"

# The class of the box a module of a binding is said to rest on, if it is.
shared_core()
{
	for l_entry in $ALREADY_SHARED; do
		if [ "$(echo $l_entry | cut -d: -f1)" = "$1" ]; then
			echo "$(echo $l_entry | cut -d: -f2)"
			return 0
		fi
	done
	return 1
}

# Does such a note still hold? Both bindings have to read the class it names.
note_holds()
{
	core=$(shared_core $1) || return 1
	for l_side in $lua/lua_$1.cpp $python/py_$1.cpp; do
		grep -q "$core" $l_side 2>/dev/null || return 1
	done
	# And the class itself has to stand outside the bindings: one both of them
	# only write for themselves is work written twice, not a core - the download
	# and its callbacks of the curl pair were exactly that.
	if grep -rq "::$core(" $lua $python 2>/dev/null; then
		return 1
	fi
	return 0
}

is_language_only()
{
	for l_only in $LANGUAGE_ONLY; do
		[ "$l_only" = "$1" ] && return 0
	done
	return 1
}

# ---------------------------------------------------------------------------
# Every class of the script API, in both bindings.
# ---------------------------------------------------------------------------

classes=$(grep -h '^class Script' $layer/*.h 2>/dev/null | sed 's/^class \([A-Za-z]*\).*/\1/' | sort -u)

if [ -z "$classes" ]; then
	echo "FAIL: the script API holds no class of a script's widget"
	status=1
fi

count=0
on_api=0
for class in $classes; do
	count=$((count + 1))
	in_lua=$(uses_in $lua $class)
	in_python=$(uses_in $python $class)
	if [ "$in_lua" != "0" ] && [ "$in_python" != "0" ]; then
		on_api=$((on_api + 1))
		echo "  on the script API: $class"
	else
		if [ "$in_lua" = "0" ]; then
			echo "FAIL: no lua binding builds a handle on $class"
		fi
		if [ "$in_python" = "0" ]; then
			echo "FAIL: no python binding builds a handle on $class"
		fi
		status=1
	fi
done
echo "  $on_api of $count"

# ---------------------------------------------------------------------------
# Every module the two bindings carry as a pair - a lua_<name> and a py_<name> -
# has to be read from the script API on both sides, or on neither. The one is a
# half-done migration and a fault; the other is work not begun and a note.
# ---------------------------------------------------------------------------

todo=""
shared=""
language=""
answers=""
pairs=0
for f in $lua/lua_*.cpp; do
	name=$(basename $f .cpp | sed 's/^lua_//')
	twin=$python/py_$name.cpp
	[ -f "$twin" ] || continue

	pairs=$((pairs + 1))
	in_lua=no
	in_python=no
	on_the_layer $f $lua/lua_$name.h && in_lua=yes
	on_the_layer $twin $python/py_$name.h && in_python=yes

	# What this pair answers in the layer, and whether it is there. The file is
	# read out of the pair, not guessed from the name, so a pair that reads
	# script_window.h has to find it - a name that no longer answers is a fault.
	# The header of a class needs its source beside it, or it is a promise and
	# not an implementation. A pair that answers nothing is said below: either
	# the work is still to come, or the pair rests on a class of the box.
	answers_of=$(grep -ho "scriptapi/script_[a-z_]*\.h" $f $lua/lua_$name.h $twin $python/py_$name.h 2>/dev/null | sed 's|scriptapi/||' | sort -u)
	if [ -z "$answers_of" ]; then
		answers="$answers $name:"
	else
		for l_file in $answers_of; do
			if [ ! -f "$layer/$l_file" ]; then
				echo "FAIL: $name reads $l_file, and the script API does not hold it"
				status=1
			elif grep -q '^class Script' "$layer/$l_file" 2>/dev/null; then
				l_stem=$(echo $l_file | sed 's/\.h$//')
				if [ ! -f "$layer/$l_stem.cpp" ]; then
					echo "FAIL: $l_file holds a class, and $l_stem.cpp is not beside it"
					status=1
				fi
			fi
		done
		answers="$answers $name:$(echo $answers_of | tr ' ' ',')"
	fi

	if [ "$in_lua" != "$in_python" ]; then
		echo "FAIL: $name is read from the script API on one side only (lua: $in_lua, python: $in_python)"
		status=1
	elif [ "$in_lua" = "no" ]; then
		if is_language_only $name; then
			language="$language $name"
		elif core=$(shared_core $name); then
			if note_holds $name; then
				shared="$shared $name($core)"
			else
				echo "FAIL: $name is said to rest on $core, and a binding does not read it"
				status=1
			fi
		else
			todo="$todo $name"
		fi
	fi
done

if [ -n "$todo" ]; then
	echo "  a pair of both bindings, still to come, not on the script API yet ($pairs pairs looked at):"
	for name in $todo; do
		echo "    $name"
	done
fi

if [ -n "$shared" ]; then
	echo "  a pair of both bindings whose work already stands in a class of the box ($pairs pairs looked at):"
	for entry in $shared; do
		echo "    $entry"
	done
	echo "  what this script sees of them is only that both bindings name that class. What"
	echo "  they do around the call was looked at by the eye and not here: filehelpers and"
	echo "  configfile are wiring, one call of the box class each, and video is too - but"
	echo "  for one short sequence both write out themselves, blocked from plugin, SetFile,"
	echo "  exec, getKeyPressed. To move that, the layer would need a way back into the"
	echo "  binding for the info callback, which is why it is named here and not half done."
fi

if [ -n "$language" ]; then
	echo "  a pair of both bindings that is the language itself, and no work left over:"
	for name in $language; do
		echo "    $name - its two sides hold different things, which is the point"
	done
fi

# ---------------------------------------------------------------------------
# The correspondence itself, in one column: what a pair answers in the script
# API, and what of it is read by nobody. A pair whose class stands on the layer
# is held to it by the first check, which wants both bindings to build on it;
# this one shows that the file it reads is the file it answers, and that no
# file of the layer has quietly stopped being read.
# ---------------------------------------------------------------------------

if [ -n "$answers" ]; then
	echo "  what a pair of the two bindings answers in the script API ($pairs pairs looked at):"
	for entry in $answers; do
		echo "    $(echo $entry | sed 's/:$/: nothing yet/;s/:/ -> /')"
	done
fi

never_read=""
for l_file in $layer/script_*.h; do
	[ -f "$l_file" ] || continue
	l_base=$(basename $l_file)
	grep -rq "scriptapi/$l_base" $lua $python 2>/dev/null || never_read="$never_read $l_base"
done
if [ -n "$never_read" ]; then
	echo "  a header of the script API that no binding reads, so it answers nobody:"
	for l_base in $never_read; do
		echo "    $l_base"
	done
fi

# ---------------------------------------------------------------------------
# What must hold: the magic a colour of a script carries, and the rule that
# resolves it, stand in the script API only.
# ---------------------------------------------------------------------------

for name in MAGIC_COLOR MAGIC_MASK; do
	outside=$(grep -rl "define $name" $lua $python 2>/dev/null || true)
	if [ -n "$outside" ]; then
		echo "FAIL: $name is written outside the script API:"
		echo "$outside" | sed 's/^/    /'
		status=1
	fi
done

rule=$(grep -rl 'realcolor\[' $lua $python 2>/dev/null || true)
if [ -n "$rule" ]; then
	echo "FAIL: a colour of a script is resolved outside the script API:"
	echo "$rule" | sed 's/^/    /'
	status=1
fi

# ---------------------------------------------------------------------------
# What to look at: a binding may still build a widget of a kind the script API
# builds, for something that is not a script's own - a window building the
# header it carries. That is no fault by itself, so it is only listed.
# ---------------------------------------------------------------------------

built=$(grep -ho 'new C[A-Za-z]*(' $layer/*.cpp 2>/dev/null | sed 's/^new \(.*\)(/\1/' | sort -u)
for class in $built; do
	hits=$(grep -rn "new $class(" $lua $python 2>/dev/null || true)
	if [ -n "$hits" ]; then
		echo "  a $class is built in a binding as well, for something of its own:"
		echo "$hits" | sed 's/^/    /'
	fi
done

exit $status

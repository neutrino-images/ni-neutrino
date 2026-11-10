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
# when a widget is added to it: every class the script API holds has to be the
# one the bindings build their handle on - in both of them, or one of them is
# not migrated yet. Where something must hold it fails; what a reader should
# look at, it only reports.
#
# Run from the top of the tree:  sh src/gui/check-scriptapi.sh
#
# It is the brother of src/coreapi/check-layering.sh, which watches the layers
# of the tree; this one watches the split between the bindings and the layer
# both of them read.

status=0
lua=src/gui/lua
python=src/gui/python
layer=src/gui/scriptapi

# How many handles one binding builds for a class of the script API: it either
# derives from it, or it holds one. Only the headers are looked at, because the
# handle of a script's widget lives in one.
handles_in()
{
	dir=$1
	class=$2
	{
		grep -rl "public $class" $dir/*.h 2>/dev/null
		grep -rlF "$class *" $dir/*.h 2>/dev/null
	} | sort -u | wc -l
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
	in_lua=$(handles_in $lua $class)
	in_python=$(handles_in $python $class)
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

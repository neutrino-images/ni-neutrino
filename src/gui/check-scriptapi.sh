#!/bin/sh
#
# Sind die Fenster, Menüs und die anderen Widgets eines Skripts in der Script-API
# gebaut, und nicht mehr in den Bindings?
#
# Die Script-API liegt in src/gui/scriptapi/ und wird von beiden Bindings
# gelesen (siehe scriptwindow.h, scriptmenu.h, scripthintbox.h,
# scriptprogresswindow.h). Dieses Skript hält fest, was davon schon gilt und was
# noch offen ist: es scheitert nur an dem, was gelten *muss*, und berichtet, was
# noch nicht umgestellt ist.
#
# Aufruf aus der Wurzel des Baums:  sh src/gui/check-scriptapi.sh
#
# Es ist der Bruder von src/coreapi/check-layering.sh, das die Schichtung
# bewacht; hier geht es um die Aufteilung zwischen Bindings und Script-API.

status=0

# ---------------------------------------------------------------------------
# Was gelten muss: ein Widget eines Skripts wird an einer Stelle gemacht, in der
# Script-API, und nicht mehr in einem Binding.
# ---------------------------------------------------------------------------

must_not_make()
{
	file="$1"
	widget="$2"
	if grep -q "new $widget(" "$file" 2>/dev/null; then
		echo "FAIL: $file makes a $widget; the script API makes it"
		grep -n "new $widget(" "$file"
		status=1
	fi
}

must_not_make src/gui/lua/lua_cc_window.cpp		CComponentsWindow
must_not_make src/gui/python/py_cc_window.cpp		CComponentsWindow
must_not_make src/gui/lua/lua_menue.cpp			CMenuWidget
must_not_make src/gui/python/py_menue.cpp		CMenuWidget
must_not_make src/gui/lua/lua_hintbox.cpp		CHintBox
must_not_make src/gui/python/py_hintbox.cpp		CHintBox
must_not_make src/gui/lua/lua_progresswindow.cpp	CProgressWindow
must_not_make src/gui/python/py_progresswindow.cpp	CProgressWindow

# Und die Farbregel eines Skripts steht an einer Stelle: die Auflösung, die vorher
# jede Bindung selbst hatte, liegt in scriptcolor.h.
other_realcolor=$(grep -rln 'realcolor\[' src/gui/lua src/gui/python 2>/dev/null | grep -v '^src/gui/scriptapi/scriptcolor.h$' || true)
if [ -n "$other_realcolor" ]; then
	echo "FAIL: a colour of a script is resolved outside the script API:"
	echo "$other_realcolor"
	status=1
fi

# ---------------------------------------------------------------------------
# Was noch offen ist: welche Widgets eines Skripts noch ein Binding für sich
# baut, und welcher Name noch doppelt dasteht.
# ---------------------------------------------------------------------------

on_api=0
count=0

pair()
{
	name="$1"
	header="$2"
	lua_file="$3"
	py_file="$4"
	count=$((count + 1))
	if grep -q "scriptapi/$header" "$lua_file" 2>/dev/null &&
	   grep -q "scriptapi/$header" "$py_file" 2>/dev/null; then
		on_api=$((on_api + 1))
		echo "  on the script API: $name"
	else
		echo "  still in the bindings: $name"
	fi
}

echo "widgets of a script, and where they are built:"
pair window		scriptwindow.h		src/gui/lua/lua_cc_window.h		src/gui/python/py_cc_window.h
pair menu		scriptmenu.h		src/gui/lua/lua_menue.h			src/gui/python/py_menue.h
pair hintbox		scripthintbox.h		src/gui/lua/lua_hintbox.h		src/gui/python/py_hintbox.h
pair progresswindow	scriptprogresswindow.h	src/gui/lua/lua_progresswindow.h	src/gui/python/py_progresswindow.h
pair text		scripttext.h		src/gui/lua/lua_cc_text.h		src/gui/python/py_cc_text.h
pair picture		scriptpicture.h		src/gui/lua/lua_cc_picture.h		src/gui/python/py_cc_picture.h
pair header		scriptheader.h		src/gui/lua/lua_cc_header.h		src/gui/python/py_cc_header.h
pair signalbox		scriptsignalbox.h	src/gui/lua/lua_cc_signalbox.h		src/gui/python/py_cc_signalbox.h
pair hourglass		scripthourglass.h	src/gui/lua/lua_hourglass.h		src/gui/python/py_hourglass.h

echo "  $on_api of $count"

# Der Name einer Farbe und der Name, unter dem eine Bindung die Auflösung kennt,
# tragen die Sprache noch in sich; sie fallen, wenn das letzte Widget umgestellt
# ist.
for what in 'define MAGIC_COLOR' 'setLuaFooter' 'showValue'; do
	echo "  a name that still carries a language: $what"
	grep -rln "$what" src/gui/lua src/gui/python src/gui/widget 2>/dev/null | sed 's/^/    /'
done

# Und wo eine Bindung die Regel doch selbst schreibt, ist etwas nicht
# umgestellt: die Aufloesung einer Skript-Farbe steht in der Schicht.
other=$(grep -rl 'realcolor\[' src/gui/lua src/gui/python 2>/dev/null || true)
if [ -n "$other" ]; then
	echo "note: a colour of a script is resolved outside the script API here:"
	echo "$other"
fi

exit $status

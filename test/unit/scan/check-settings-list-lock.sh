#!/bin/sh
# The lists and the lists of records the settings struct keeps, and the two texts of
# the display theme, are read on the web server's threads and changed on the loop's,
# and a change that frees what a reader is copying is a fault in the reader.
#
# The five containers are webtv_xml, webradio_xml, xmltv_xml, usermenu and
# timer_remotebox_ip. The settings layer copies each under CSettingsTextGuard, so
# every statement in the tree that empties one, adds to it, takes from it, assigns
# it, swaps it, frees one of the records it points to or assigns a member of one of
# them has to stand in a scope that holds that guard. check-list-reader-lock.sh is the
# precedent for the class of fault and check-settings-text-lock.sh for the guard.
#
# The two texts, glcd_font and glcd_background_image, are members of a struct the
# screens reach through a reference of their own, which the scan for direct writes
# to g_settings does not see. They are assigned through setSettingsText and in no
# other way, wherever they are spelt.
#
# The same containers are read as well as changed. A walk of one on another thread than
# the one that writes it reads memory a write is free to release, so every read has to be
# a copy taken under the guard (settingsCopy) or stand in a scope that holds it. Two
# readers are exempt, each named below:
#   * a pointer to a list handed to another part (&g_settings.webtv_xml): nothing is read
#     there, and the code that walks it is held by the zapit rule below;
#   * the reads of usermenu and timer_remotebox_ip in the files that run on the box's own
#     loop (the screens, and neutrino.cpp's load and save). Those two are written only on
#     the loop, since a write through the settings layer is carried in by it, so the
#     writer and the reader are one thread. The three lists are not exempt anywhere: a
#     web thread writes them directly.
# In zapit's bouquets.cpp the list is reached through a pointer, so there a dereference of
# cfg has to be a copy as well.
#
# Headers are read as well as sources: a reader written inline in one is as much a reader.
#
# Comments and code under #if 0 are blanked first. With files named after the
# directory only those are read, which is how a case shows the scan going red.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-settings-list-lock.sh <top source directory> [file...]" >&2
	exit 2
}
shift

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
for f in "$STRIP" "$BLANK"; do
	[ -r "$f" ] || { echo "check-settings-list-lock.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# nhttpd is the removed server's source and is built by nothing.
if [ "$#" -gt 0 ]; then
	FLOOR=0
	for f in "$@"; do echo "$f"; done > "$tmp/files"
else
	FLOOR=20
	find "$SRC/src" \( -name '*.cpp' -o -name '*.h' \) -not -path "$SRC/src/nhttpd/*" | LC_ALL=C sort > "$tmp/files"
fi

: > "$tmp/found"
while read -r f; do
	awk -v keepstrings=0 -f "$STRIP" "$f" | awk -f "$BLANK" | awk -v file="$f" '
	function mutates(line,   m) {
		for (m in members) {
			q = "g_settings\\." members[m]
			if (line ~ ("delete[ \t]+" q)) return 1
			if (line ~ (q "[ \t]*\\.[ \t]*(clear|push_back|pop_back|erase|insert|assign|swap|emplace|emplace_back|sort|splice|remove|resize|reverse|unique)[ \t]*\\(")) return 1
			if (line ~ (q "[ \t]*=[^=]")) return 1
			if (line ~ (q "[ \t]*\\[[^]]*\\][ \t]*=[^=]")) return 1
			if (line ~ (q "[ \t]*\\[[^]]*\\][ \t]*(->|\\.)[ \t]*[A-Za-z_]+[ \t]*=[^=]")) return 1
		}
		return 0
	}
	function mentions(line,   m) {
		for (m in members)
			if (line ~ ("g_settings\\." members[m] "([^A-Za-z_0-9]|$)")) return members[m]
		return ""
	}
	BEGIN {
		split("webtv_xml webradio_xml xmltv_xml usermenu timer_remotebox_ip", members, " ")
		depth = 0
		# the files that run on the box loop, where the two loop-written containers are read
		split("src/neutrino.cpp src/gui/infoviewer_bb.cpp src/gui/keybind_setup.cpp src/gui/personalize.cpp src/gui/timerlist.cpp src/gui/user_menue.cpp src/gui/user_menue_setup.cpp", loopfiles, " ")
		onloop = 0
		for (k in loopfiles) if (index(file, loopfiles[k]) > 0 && index(file, "/gui/") + index(file, "neutrino.cpp") > 0) onloop = 1
		zapit = (index(file, "src/zapit/bouquets.cpp") > 0)
	}
	{
		line = $0
		held = 0
		for (d in guards) held = 1
		hasguard = (index(line, "CSettingsTextGuard") > 0)
		# the guard stands in the scope it is declared in, which is the depth at
		# the point it is met
		n = length(line)
		for (i = 1; i <= n; i++) {
			c = substr(line, i, 1)
			if (c == "{") depth++
			else if (c == "}") { delete guards[depth]; depth-- }
			else if (substr(line, i, 18) == "CSettingsTextGuard") { guards[depth] = 1; i += 17 }
		}
		if (mutates(line)) {
			if (held || hasguard) print "G\t" file ":" NR
			else print "U\t" file ":" NR ": " line
		}
		# the reads
		m = mentions(line)
		if (m != "" && !mutates(line)) {
			copied = (index(line, "settingsCopy(") > 0)
			handed = (line ~ ("&g_settings\\." m "([^A-Za-z_0-9]|$)"))
			loopowned = onloop && (m == "usermenu" || m == "timer_remotebox_ip")
			if (held || hasguard || copied || handed || loopowned) print "R\t" file ":" NR
			else print "U\t" file ":" NR ": " line
		}
		if (zapit && (line ~ /[(=,][ \t]*\*cfg([^A-Za-z_0-9]|$)/ || line ~ /cfg->/)) {
			if (held || hasguard || index(line, "settingsCopy(") > 0) print "R\t" file ":" NR
			else print "U\t" file ":" NR ": " line
		}
		# the texts: assigned through the setter and not otherwise
		if (line ~ /(glcd_font|glcd_background_image)[ \t]*=[^=]/)
			print "U\t" file ":" NR ": " line
	}
	' >> "$tmp/found"
done < "$tmp/files"

unguarded=`grep -c '^U' "$tmp/found"` || unguarded=0
guarded=`grep -c '^G' "$tmp/found"` || guarded=0
reads=`grep -c '^R' "$tmp/found"` || reads=0

if [ "$unguarded" -ne 0 ]; then
	echo "a list or list of records of the settings changed or walked outside CSettingsTextGuard, or a display theme text assigned without setSettingsText:" >&2
	grep '^U' "$tmp/found" | cut -f2- | sed 's/^/  /' >&2
	exit 1
fi
if [ "$guarded" -lt "$FLOOR" ]; then
	echo "check-settings-list-lock.sh: $guarded guarded changes found, below the floor of $FLOOR, the scan has stopped matching" >&2
	exit 1
fi

echo "changes of the settings lists, held under the guard   $guarded"
echo "reads of them, copied or on the writer's own thread        $reads"
exit 0

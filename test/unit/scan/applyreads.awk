# Every read of a settings member in a tree of sources, with what kind of place it
# is read in. Reads the sources behind strip-comments.awk (mark=@@file@@, literals
# replaced) and blank-if0.awk, so a comment or a dead block is no reader. Every
# other #if arm is read, taken or not: which arm a box builds is for the build.
#
# One line per read, tab between:  member  kind  file  function
#
# member is the name after g_settings., and for the two nested structs the name and
# the member of it joined by a dot, which is how a row names them.
#
# A read in a function that only builds a setup menu, or the text a menu shows for a
# value, is not a reader of the value either: it shows what is there and does
# nothing with it. A function the notifiers and startup both call to push the
# value on, listed in the helper names below, counts as applying; it is the code
# the group would hold. Both lists are of names this tree uses, and a new
# spelling shows up as a key that is missing from the scan's findings, never as
# one it wrongly asks a group for.
#
# kind is
#   apply  the read is what makes a change take effect: a change notifier, the
#          branch of an exec handler an action key selects, or the startup that
#          runs once. A key every read of which is one of these has nothing that
#          reads it where it is used, so a change reaches it only if something
#          applies it.
#   use    any other read.
#   skip   not a reader of the value: it is assigned and not read, its address is
#          taken to bind it to a widget, or it is the persistence that moves it
#          to and from the file.
#
# Functions are found the way this tree writes them: the signature starts in the
# first column and the body opens with a brace in the first column. A read in
# anything else, a method written inside its class, has no function and is a use.
function trim(s) { sub(/^[ \t]+/, "", s); sub(/[ \t]+$/, "", s); return s }

function funcname(line,   head, p) {
	p = index(line, "(")
	head = substr(line, 1, p - 1)
	sub(/[ \t]+$/, "", head)
	if (match(head, /[A-Za-z_0-9~:]+$/)) return substr(head, RSTART, RLENGTH)
	return ""
}

function builds_menu(base) {
	if (rel !~ /\/gui\//) return 0
	if (base ~ /^(show|init|Show)[A-Za-z]*(Setup|Settings|Menu|Menue)[A-Za-z]*$/) return 1
	return base ~ /SettingsText$/
}

function applies_for(base) {
	return base == "MakeSectionsdConfig" || base == "setCECSettings" || base == "SetupNeutrinoFonts"
}

function basename(f) { sub(/^.*::/, "", f); return f }

function emit(member, kind) { print member "\t" kind "\t" file "\t" (func == "" ? "-" : func) }

# One read, found at position i of the line, in the state the walk has reached.
function classify(rest, before, inaction,   base, r) {
	r = rest
	if (before ~ /&[ \t]*$/) return "skip"
	# the helper the settings file load assigns its text through
	if (before ~ /setSettingsText\([ \t]*$/) return "skip"
	sub(/^[ \t]*(\[[^]]*\]|\.[A-Za-z_0-9]+)*/, "", r)
	if (r ~ /^=([^=]|$)/) return "skip"
	base = basename(func)
	if (base == "saveSetup" || base == "upgradeSetup") return "skip"
	if (skipfile) return "skip"
	if (builds_menu(base)) return "skip"
	if (base == "changeNotify" || applies_for(base)) return "apply"
	if (base == "loadSetup" && func ~ /CNeutrinoApp/) return "apply"
	if (base == "run" && func ~ /CNeutrinoApp/) return "apply"
	if (base == "exec" && inaction) return "apply"
	return "use"
}

function reset() { func = ""; pending = ""; depth = 0; inact = 0; actpend = 0; actdepth = 0 }

FNR == 1 { reset() }
index($0, mark) == 1 {
	file = substr($0, length(mark) + 1)
	# judged by the place inside the tree it is given, not by where the tree is
	rel = file
	if (root != "" && index(rel, root) == 1) rel = substr(rel, length(root) + 1)
	rel = "/" rel
	reset()
	skipfile = 0
	if (rel ~ /\/nhttpd\// || rel ~ /\/coreapi\/settings\// || rel ~ /\/apply_[a-z0-9_]+\.cpp$/ ||
	    rel ~ /settingssource_real\.cpp$/ || rel ~ /settings_appliers\.cpp$/ ||
	    rel ~ /settings_manager/ || rel ~ /\/test\//)
		skipfile = 1
	next
}
{
	l = $0
	# a function starts at its signature and ends at the closing brace in the
	# first column
	if (func == "") {
		if (l ~ /^\}/ || l ~ /;[ \t]*$/) pending = ""
		else if (l ~ /^[A-Za-z_]/ && l ~ /\(/) pending = funcname(l)
		if (l ~ /^\{/ && pending != "") {
			func = pending; pending = ""; depth = 0; inact = 0; actpend = 0
		}
	}
	else if (l ~ /^\}/) {
		func = ""; pending = ""; inact = 0; actpend = 0; depth = 0
	}

	# where the reads and the action key test are on the line
	n = 0
	s = l; off = 0
	while (match(s, /g_settings\.[A-Za-z_0-9]+(\.[A-Za-z_0-9]+)?/)) {
		n++
		rpos[n] = off + RSTART
		rlen[n] = RLENGTH
		off += RSTART + RLENGTH - 1
		s = substr(s, RSTART + RLENGTH)
	}
	ak = 0
	if (basename(func) == "exec" && match(l, /actionKey[ \t]*==/)) ak = RSTART

	next_read = 1
	len = length(l)
	for (i = 1; i <= len + 1; i++) {
		while (next_read <= n && rpos[next_read] == i) {
			tok = substr(l, rpos[next_read] + 11, rlen[next_read] - 11)
			rest = substr(l, rpos[next_read] + rlen[next_read])
			before = substr(l, 1, rpos[next_read] - 1)
			split(tok, seg, ".")
			if (seg[2] != "" && (seg[1] == "theme" || seg[1] == "glcd_theme")) {
				kind = classify(rest, before, inact || actpend)
				emit(tok, kind)
			}
			else {
				kind = classify(rest, before, inact || actpend)
				emit(seg[1], kind)
			}
			next_read++
		}
		if (i == ak) actpend = 1
		c = substr(l, i, 1)
		if (c == "{") {
			depth++
			if (actpend) { inact = 1; actdepth = depth; actpend = 0 }
		}
		else if (c == "}") {
			if (inact && depth == actdepth) inact = 0
			depth--
		}
		else if (c == ";" && actpend) actpend = 0
	}
}

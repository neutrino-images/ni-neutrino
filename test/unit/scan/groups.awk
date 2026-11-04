# The heading a screen files each setting it builds from the declaration under, and the
# headings in the order the screens lead to them. Reads the source with its literals
# intact, because the key is one, behind strip-comments.awk and blank-if0.awk.
#
# A screen builds a menu as a widget and puts items into it, so what a site of
# addSetting says about its heading is what the widget it names says about itself:
#
#   CMenuWidget w(LOCALE_TITLE, ...);   or   w = new CMenuWidget(LOCALE_TITLE, ...)
#   w->addIntroItems(LOCALE_INTRO);     the text the submenu is introduced with
#   w->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_SEP));
#   addSetting(w, "key");
#
# One row per site, tab between: S, the key, the intro, the separator above the site,
# the title, and where it was read. An intro the call states as a choice of two
# (webradio ? LOCALE_A : LOCALE_B) is both, joined by |. An intro the call does not
# name falls back to nothing, the title to nothing, and nothing is -: a widget whose
# heading is held in a variable says nothing a scan can read. A separator holds from
# where it is added to the next titled one or the next intro of the same widget.
#
# The intro of a widget a function receives as a parameter is the one that function
# gives it, which is read in order like any other, so a parameter named like a widget of
# another function in the file inherits nothing: the record is cleared at each function.
#
# Then one row per item that leads to a heading or titles one, in the order of the file:
# H, the locale, and where it was read. A forwarder leads to a submenu and is labelled as
# the submenu is introduced; a titled separator is a heading where the menu files rows
# straight under it.
#
# An array of keys walked by a call is read as addsetting.awk reads it: every element, in
# order, one site each.
function locales(s,   out) {
	out = ""
	while (match(s, /LOCALE_[A-Z0-9_]+/)) {
		out = out (out == "" ? "" : "|") substr(s, RSTART, RLENGTH)
		s = substr(s, RSTART + RLENGTH)
	}
	return out
}
# The first argument of a call whose argument list begins at s, up to the comma or the
# parenthesis that closes it at depth zero.
function firstarg(s,   i, c, depth) {
	depth = 0
	for (i = 1; i <= length(s); i++) {
		c = substr(s, i, 1)
		if (c == "(") depth++
		else if (c == ")") { if (depth == 0) return substr(s, 1, i - 1); depth-- }
		else if (c == "," && depth == 0) return substr(s, 1, i - 1)
	}
	return s
}
function or_none(s) { return s == "" ? "-" : s }

mark != "" && index($0, mark) == 1 {
	where = substr($0, length(mark) + 1); fline = FNR
	delete title; delete intro; delete sep; delete sepvar; delete count; inarr = ""
	next
}

{
	line = $0
	at = where ":" (FNR - fline)

	# A function begins at a line that opens with a name and not with white space.
	if (line ~ /^[A-Za-z_][A-Za-z_0-9:<>, *&]*\(/ && line !~ /;[ \t]*$/) {
		delete title; delete intro; delete sep; delete sepvar
	}

	if (inarr == "" && match(line, /const[ \t]+char[ \t]*\*[ \t]*(const[ \t]+)?[A-Za-z_0-9]+[ \t]*\[[ \t]*\][ \t]*=/)) {
		inarr = substr(line, RSTART, RLENGTH)
		sub(/[ \t]*\[.*$/, "", inarr); sub(/^.*[ \t*]/, "", inarr)
		count[inarr] = 0
	}
	if (inarr != "") {
		rest = line
		while (match(rest, /"[a-z0-9_A-Z.]+"/)) {
			count[inarr]++
			elem[inarr, count[inarr]] = substr(rest, RSTART + 1, RLENGTH - 2)
			rest = substr(rest, RSTART + RLENGTH)
		}
		if (line ~ /\}[ \t]*;/) inarr = ""
		next
	}

	if (match(line, /[A-Za-z_0-9]+[ \t]*(=[ \t]*new[ \t]+CMenuWidget[ \t]*)?\([ \t]*LOCALE_[A-Z0-9_]+/) && line ~ /CMenuWidget/) {
		w = substr(line, RSTART, RLENGTH)
		t = w; sub(/^.*\([ \t]*/, "", t)
		sub(/[ \t]*(=.*)?\(.*$/, "", w)
		title[w] = t
	}

	if (match(line, /[A-Za-z_0-9]+[ \t]*(->|\.)[ \t]*addIntroItems[ \t]*\(/)) {
		w = substr(line, RSTART, RLENGTH); sub(/[ \t]*(->|\.).*$/, "", w)
		intro[w] = locales(firstarg(substr(line, RSTART + RLENGTH)))
		sep[w] = ""
	}

	if (match(line, /[A-Za-z_0-9]+[ \t]*=[ \t]*new[ \t]+CMenuSeparator[ \t]*\([^,]*STRING[^,]*,[ \t]*LOCALE_[A-Z0-9_]+/)) {
		v = substr(line, RSTART, RLENGTH); s = v
		sub(/[ \t]*=.*$/, "", v); sub(/^.*,[ \t]*/, "", s)
		sepvar[v] = s
	}
	if (match(line, /[A-Za-z_0-9]+[ \t]*(->|\.)[ \t]*addItem[ \t]*\([ \t]*new[ \t]+CMenuSeparator[ \t]*\([^,]*STRING[^,]*,[ \t]*LOCALE_[A-Z0-9_]+/)) {
		w = substr(line, RSTART, RLENGTH); s = w
		sub(/[ \t]*(->|\.).*$/, "", w); sub(/^.*,[ \t]*/, "", s)
		sep[w] = s
	} else if (match(line, /[A-Za-z_0-9]+[ \t]*(->|\.)[ \t]*addItem[ \t]*\([ \t]*[A-Za-z_0-9]+[ \t]*[,)]/)) {
		w = substr(line, RSTART, RLENGTH); v = w
		sub(/[ \t]*(->|\.).*$/, "", w); sub(/^.*\([ \t]*/, "", v); sub(/[ \t]*[,)]$/, "", v)
		if (v in sepvar) { sep[w] = sepvar[v]; print "H\t" sepvar[v] "\t" at }
	}

	rest = line
	while (match(rest, /(CMenuD?Forwarder|CFollowForwarder)[ \t]*\([ \t]*LOCALE_[A-Z0-9_]+|new[ \t]+CMenuSeparator[ \t]*\([^,]*STRING[^,]*,[ \t]*LOCALE_[A-Z0-9_]+/)) {
		h = substr(rest, RSTART, RLENGTH); sub(/^.*[(,][ \t]*/, "", h)
		# A separator held in a variable is a heading where it is added, not where it is made.
		if (substr(rest, RSTART, 3) == "new" && substr(rest, 1, RSTART - 1) ~ /=[ \t]*$/) { rest = substr(rest, RSTART + RLENGTH); continue }
		print "H\t" h "\t" at
		rest = substr(rest, RSTART + RLENGTH)
	}

	rest = line
	while (match(rest, /add(Choice|Number|Color)?Setting[ \t]*\([ \t]*&?[A-Za-z_0-9]+[ \t]*,[ \t]*("[a-z0-9_A-Z.]+"|[A-Za-z_0-9]+\[[^]]*\])/)) {
		site = substr(rest, RSTART, RLENGTH)
		rest = substr(rest, RSTART + RLENGTH)
		w = site; sub(/^[^(]*\([ \t]*&?/, "", w); sub(/[ \t]*,.*$/, "", w)
		k = site; sub(/^[^,]*,[ \t]*/, "", k)
		heading = or_none(intro[w]) "\t" or_none(sep[w]) "\t" or_none(title[w]) "\t" at
		if (k ~ /^"/) {
			gsub(/"/, "", k)
			print "S\t" k "\t" heading
		} else {
			sub(/\[.*$/, "", k)
			for (e = 1; e <= count[k]; e++)
				print "S\t" elem[k, e] "\t" heading
		}
	}
}

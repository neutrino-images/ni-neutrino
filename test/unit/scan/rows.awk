# The rows of the settings declaration as the source states them, for a scan that
# has to say what a key built from the declaration stands for. Reads the table
# files after strip-comments.awk with literals kept and blank-if0.awk, so a row
# under #if 0 is no row. Two kinds of line, tab between:
#
#   R key type table guard   one per row; table is the EnumValue list the row
#                            offers, - for none (a number offers none: its list
#                            names a value shown in words); guard is the
#                            preprocessor arms around the row joined by &&, - for none
#   E table value            one per entry of an EnumValue list, as written; the
#                            arms of one #if chain inside a list are alternatives
#                            for one position; each distinct value of a position is
#                            written once, so an arm that offers another value shows
#
# Every other arm is read, taken or not: the source is what is scanned, and which
# arm a box takes is for the build to say.
BEGIN { kinds["boolRow"] = "Bool"; kinds["intRow"] = "Int"; kinds["enumRow"] = "Enum"; kinds["textRow"] = "String"; kinds["keyRow"] = "Key"; kinds["colorRow"] = "Color"; kinds["listRow"] = "List"; kinds["recordsRow"] = "Records" }
# What stands between the parenthesis a call opened and the one that closes it,
# the rest of the line when it does not close there. Quotes are not read: no
# value an entry states holds one.
function argument(rest,   i, c, depth) {
	depth = 1
	for (i = 1; i <= length(rest); i++) {
		c = substr(rest, i, 1)
		if (c == "(") depth++
		else if (c == ")" && --depth == 0) return substr(rest, 1, i - 1)
	}
	return rest
}
function trim(s) { sub(/^[ \t]+/, "", s); sub(/[ \t]+$/, "", s); return s }
function guard(   i, g) {
	g = ""
	for (i = 1; i <= depth; i++) g = g (g == "" ? "" : " && ") cur[i]
	return (g == "") ? "-" : g
}
function flush() {
	if (key != "") print "R\t" key "\t" type "\t" (table == "" ? "-" : table) "\t" kguard
	key = ""
}
# Read file by file behind a mark line, or as one file: either way the state of one
# file does not carry into the next.
function reset() { flush(); depth = 0; intab = "" }
FNR == 1 { reset() }
mark != "" && index($0, mark) == 1 { reset(); next }
{
	l = $0
	d = trim(l)
	if (d ~ /^#[ \t]*(if|elif)/ && d ~ /\\$/) d = d " || @@continued@@"
	if (d ~ /^#[ \t]*if/) {
		c = d
		if (c ~ /^#[ \t]*ifdef/) { sub(/^#[ \t]*ifdef[ \t]*/, "", c); c = "defined(" trim(c) ")" }
		else if (c ~ /^#[ \t]*ifndef/) { sub(/^#[ \t]*ifndef[ \t]*/, "", c); c = "!defined(" trim(c) ")" }
		else { sub(/^#[ \t]*if[ \t]*/, "", c); c = "(" trim(c) ")" }
		cur[++depth] = c; prev[depth] = c
		if (intab != "") { base[depth] = slot; top[depth] = slot }
		next
	}
	if (intab != "" && depth > tdepth && d ~ /^#[ \t]*(elif|else)/) {
		if (slot > top[depth]) top[depth] = slot
		slot = base[depth]
	}
	if (d ~ /^#[ \t]*elif/) {
		c = d; sub(/^#[ \t]*elif[ \t]*/, "", c); c = "(" trim(c) ")"
		cur[depth] = "!" prev[depth] " && " c
		prev[depth] = "(" prev[depth] " || " c ")"
		next
	}
	if (d ~ /^#[ \t]*else/) { cur[depth] = "!" prev[depth]; next }
	if (d ~ /^#[ \t]*endif/) {
		if (intab != "" && depth > tdepth) { if (slot > top[depth]) top[depth] = slot; slot = top[depth] }
		if (depth > 0) depth--
		next
	}

	if (match(l, /EnumValue[ \t]+[A-Za-z_0-9]+[ \t]*\[/)) {
		flush()
		intab = substr(l, RSTART, RLENGTH)
		sub(/^EnumValue[ \t]+/, "", intab); sub(/[ \t]*\[$/, "", intab)
		l = substr(l, RSTART + RLENGTH)
		sub(/^[^=]*=/, "", l)
		opened = 0
		slot = 0; tdepth = depth
		delete offered
	}
	if (intab != "") {
		# the brace that opens the list is not an entry
		if (!opened) { if (!sub(/^[^{]*\{/, "", l)) next; opened = 1 }
		while (match(l, /option\(/)) {
			e = argument(substr(l, RSTART + RLENGTH))
			l = substr(l, RSTART + RLENGTH)
			sub(/,.*$/, "", e)
			++slot; e = trim(e)
			if (!((slot, e) in offered)) { offered[slot, e] = 1; print "E\t" intab "\t" e }
		}
		if (l ~ /\}[ \t]*;/) intab = ""
		next
	}

	if (match(l, /(^|[^A-Za-z0-9_])(bool|int|enum|text|key|color|list|records)Row\([ \t]*"[A-Za-z0-9_.]+"[ \t]*\)/)) {
		flush()
		r = substr(l, RSTART, RLENGTH)
		match(r, /(bool|int|enum|text|key|color|list|records)Row/)
		type = kinds[substr(r, RSTART, RLENGTH)]
		match(r, /"[^"]*"/); key = substr(r, RSTART + 1, RLENGTH - 2)
		table = ""
		kguard = guard()
	}
	if (key != "" && type != "Int" && match(l, /\.values\([ \t]*[A-Za-z_0-9]+/)) {
		table = substr(l, RSTART, RLENGTH)
		sub(/^[^(]*\([ \t]*/, "", table)
	}
	if (key != "" && l ~ /^[ \t]*\}[ \t]*;/) flush()
}
END { flush() }

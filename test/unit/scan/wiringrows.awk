# The rows and value lists of the declaration written the way check-setting-wiring.sh
# reads them: one { "key", ValueType::T, "section", label, hint, min, max, values,
# field } entry per row and one { value, "label", "words", test } entry per value of
# an EnumValue list. The tables are built with calls, row by row, and the check pairs a
# removed chooser with its row by the member the row's field names, so the calls are
# put back in the places the check looks. Reads the table files after
# strip-comments.awk with literals kept. Preprocessor lines pass through where they
# stand; one inside a row is dropped, so a row whose bounds an arm decides is read with
# the first it states.
BEGIN { kinds["boolRow"] = "Bool"; kinds["intRow"] = "Int"; kinds["enumRow"] = "Enum"; kinds["textRow"] = "String"; kinds["keyRow"] = "Key"; kinds["colorRow"] = "Color"; kinds["listRow"] = "List"; kinds["recordsRow"] = "Records" }
function trim(s) { sub(/^[ \t]+/, "", s); sub(/[ \t]+$/, "", s); return s }
function parens(s,   i, c, n) {
	n = 0
	for (i = 1; i <= length(s); i++) { c = substr(s, i, 1); if (c == "(") n++; else if (c == ")") n-- }
	return n
}
# The text between the parenthesis a call opens at pos and the one closing it.
function argof(s, pos,   i, c, d, start) {
	d = 0; start = pos
	for (i = pos; i <= length(s); i++) {
		c = substr(s, i, 1)
		if (c == "(") { if (d == 0) start = i + 1; d++ }
		else if (c == ")" && --d == 0) return substr(s, start, i - start)
	}
	return substr(s, start)
}
function call(s, name,   p) {
	if (!match(s, "\\." name "\\(")) return ""
	return trim(argof(s, RSTART + RLENGTH - 1 + 1 - 1))
}
function str(a) { return (a ~ /^"/) ? a : "NULL" }
function flushrow(   key, kind, t, lbl, hint, lo, hi, vals, fld, sect, a, av, m, n, parts) {
	t = rowbuf
	match(t, /(boolRow|intRow|enumRow|textRow|keyRow|colorRow|listRow|recordsRow)\(/)
	kind = substr(t, RSTART, RLENGTH - 1)
	key = trim(argof(t, RSTART + RLENGTH - 1))
	sect = call(t, "section"); lbl = call(t, "label"); hint = call(t, "hint")
	lo = 0; hi = 0
	if (kind == "boolRow") hi = 1
	a = call(t, "range")
	if (a != "") { n = index(a, ","); lo = trim(substr(a, 1, n - 1)); hi = trim(substr(a, n + 1)) }
	vals = ""
	a = call(t, "values"); if (a != "") vals = "COREAPI_VALUES(" a "), "
	fld = call(t, "field")
	av = call(t, "availableIf")
	if (av != "" && fld ~ /^COREAPI_NUMBER_FIELD\(/) { m = fld; sub(/^COREAPI_NUMBER_FIELD\(/, "", m); sub(/\)$/, "", m); fld = "COREAPI_NUMBER_FIELD_ON(" m ", " av ", NULL)" }
	print "{ " key ", ValueType::" kinds[kind] ", " str(sect) ", " str(lbl) ", " str(hint) ", " lo ", " hi ", " vals fld " },"
	rowbuf = ""; inrow = 0
}
function flushentry(   v, k, t, a, av) {
	if (entbuf == "") return
	v = argof(entbuf, index(entbuf, "option("))
	k = call(entbuf, "label"); t = call(entbuf, "text"); av = call(entbuf, "availableIf")
	print "{ " v ", " str(k) ", " str(t) ", " (av == "" ? "NULL" : av) " },"
	entbuf = ""
}
FNR == 1 { rowbuf = ""; inrow = 0; intab = 0; entbuf = "" }
{
	d = trim($0)
	if (d ~ /^#/) { if (!inrow) print $0; next }
	if (inrow) {
		rowbuf = rowbuf " " d
		if (rowbuf ~ /\.field\(/ && parens(rowbuf) == 0 && d ~ /\),?$/) flushrow()
		next
	}
	if (intab) {
		if (d ~ /^};?$/) { flushentry(); print "};"; intab = 0; next }
		if (d ~ /option\(/) {
			# a line holding the start of an entry ends the one before
			if (entbuf != "" && entbuf ~ /\),?[ \t]*$/ && parens(entbuf) == 0) flushentry()
			entbuf = entbuf " " d
			if (parens(entbuf) == 0 && d ~ /,$/) flushentry()
			else if (parens(entbuf) == 0 && d !~ /,$/) flushentry()
		} else if (entbuf != "") {
			entbuf = entbuf " " d
			if (parens(entbuf) == 0 && d ~ /,$/) flushentry()
		}
		next
	}
	if (d ~ /EnumValue[ \t]+[A-Za-z_0-9]+[ \t]*\[[ \t]*\][ \t]*=/) {
		sub(/^constexpr[ \t]+/, "", d); sub(/^static[ \t]+/, "", d)
		print d; print "{"; intab = 1; next
	}
	if (match(d, /(^|[^A-Za-z_0-9])(boolRow|intRow|enumRow|textRow|keyRow|colorRow|listRow|recordsRow)\(/)) {
		rowbuf = d; inrow = 1
		if (rowbuf ~ /\.field\(/ && parens(rowbuf) == 0 && d ~ /\),?$/) flushrow()
		next
	}
	print $0
}

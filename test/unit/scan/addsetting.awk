# A screen that builds an item from the settings declaration states the key and
# nothing else: label, values and bounds come from the declaration, so there is
# nothing on the screen to compare against it. This reads those sites, one row per
# call: the key, then where it was read. Reads the source with its literals intact,
# because the key is one.
#
# addChoiceSetting and addNumberSetting are the same call typed for the screen that
# keeps the item, so they are read as one. The declarations name a parameter where a
# call names a literal, so they are not matched.
#
# A third column says which preprocessor arms the call sits in, joined by &&, or - for
# none: a site a build leaves out is one whose row that build may leave out as well.
# Spelled the way rows.awk spells the arms of a row, so the two can be compared.
#
# A screen that builds a run of items walks an array of keys:
#   const char *const kKeys[] = { "a", "b" };   ... addSetting(menu, kKeys[i]);
# The call names every element of the array defined earlier in the same file, in its
# order, one row each. An element's arms join the call's, so an element a build leaves
# out is a site that build leaves out. A call that names an array this file does not
# define is no site: nothing says which keys it builds.
function trim(s) { sub(/^[ \t]+/, "", s); sub(/[ \t]+$/, "", s); return s }
function guard(   i, g) {
	g = ""
	for (i = 1; i <= depth; i++) g = g (g == "" ? "" : " && ") cur[i]
	return (g == "") ? "-" : g
}
function both(a, b) { return (a == "-") ? b : ((b == "-") ? a : a " && " b) }
mark != "" && index($0, mark) == 1 { where = substr($0, length(mark) + 1); fline = FNR; depth = 0; delete count; inarr = ""; next }

{
	d = trim($0)
	# A guard continued on the next line, or one a block comment spans, is read from
	# its first line only. An || is added so that it stays one conjunct no arm of a row
	# matches, and reads "row".
	if (d ~ /^#[ \t]*(if|elif)/ && d ~ /\\$/) d = d " || @@continued@@"
	if (d ~ /^#[ \t]*if/) {
		c = d
		if (c ~ /^#[ \t]*ifdef/) { sub(/^#[ \t]*ifdef[ \t]*/, "", c); c = "defined(" trim(c) ")" }
		else if (c ~ /^#[ \t]*ifndef/) { sub(/^#[ \t]*ifndef[ \t]*/, "", c); c = "!defined(" trim(c) ")" }
		else { sub(/^#[ \t]*if[ \t]*/, "", c); c = "(" trim(c) ")" }
		cur[++depth] = c; prev[depth] = c
		next
	}
	if (d ~ /^#[ \t]*elif/) {
		c = d; sub(/^#[ \t]*elif[ \t]*/, "", c); c = "(" trim(c) ")"
		cur[depth] = "!" prev[depth] " && " c
		prev[depth] = "(" prev[depth] " || " c ")"
		next
	}
	if (d ~ /^#[ \t]*else/) { cur[depth] = "!" prev[depth]; next }
	if (d ~ /^#[ \t]*endif/) { if (depth > 0) depth--; next }

	line = $0
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
			eguard[inarr, count[inarr]] = guard()
			rest = substr(rest, RSTART + RLENGTH)
		}
		if (line ~ /\}[ \t]*;/) inarr = ""
		next
	}
	rest = line
	while (match(rest, /add(Choice|Number)?Setting\([^,()]+, *[A-Za-z_0-9]+\[[^]]*\]/)) {
		site = substr(rest, RSTART, RLENGTH)
		rest = substr(rest, RSTART + RLENGTH)
		arr = site; sub(/\[[^]]*\]$/, "", arr); sub(/^.*, */, "", arr)
		for (k = 1; k <= count[arr]; k++)
			print elem[arr, k] "\t" where ":" (FNR - fline) "\t" both(guard(), eguard[arr, k])
	}
	while (match(line, /add(Choice|Number)?Setting\([^,]+, *"[a-z0-9_A-Z.]+"/)) {
		site = substr(line, RSTART, RLENGTH)
		sub(/^[^"]*"/, "", site)
		sub(/"$/, "", site)
		print site "\t" where ":" (FNR - fline) "\t" guard()
		line = substr(line, RSTART + RLENGTH)
	}
}

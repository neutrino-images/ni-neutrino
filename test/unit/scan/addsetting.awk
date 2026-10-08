# A screen that builds an item from the settings declaration states the key and
# nothing else: label, values and bounds come from the declaration, so there is
# nothing on the screen to compare against it. This reads those sites, one row per
# call: the key, then where it was read. Reads the source with its literals intact,
# because the key is one.
#
# The declaration of addSetting itself names a parameter where a call names a
# literal, so it is not matched.
#
# A third column says which preprocessor arms the call sits in, joined by &&, or - for
# none: a site a build leaves out is one whose row that build may leave out as well.
# Spelled the way rows.awk spells the arms of a row, so the two can be compared.
function trim(s) { sub(/^[ \t]+/, "", s); sub(/[ \t]+$/, "", s); return s }
function guard(   i, g) {
	g = ""
	for (i = 1; i <= depth; i++) g = g (g == "" ? "" : " && ") cur[i]
	return (g == "") ? "-" : g
}
mark != "" && index($0, mark) == 1 { where = substr($0, length(mark) + 1); fline = FNR; depth = 0; next }

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
	while (match(line, /addSetting\([^,]+, *"[a-z0-9_A-Z.]+"/)) {
		site = substr(line, RSTART, RLENGTH)
		sub(/^[^"]*"/, "", site)
		sub(/"$/, "", site)
		print site "\t" where ":" (FNR - fline) "\t" guard()
		line = substr(line, RSTART + RLENGTH)
	}
}

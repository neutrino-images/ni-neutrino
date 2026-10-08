# The key of each declared row and the settings members its value lives in, as the
# row states both. Reads the table files behind strip-comments.awk with literals
# kept and blank-if0.awk. One line per key and member, tab between; a key whose
# value is no member (a flag file) is printed with a dash.
#
# A nested struct member is written the way a reader writes it, theme.name, and a
# colour as theme.name_ with a trailing underscore: the row holds three or four
# members of one name and a reader names each.
function trim(s) { sub(/^[ \t]+/, "", s); sub(/[ \t]+$/, "", s); return s }
/[A-Za-z]+Row\("[^"]+"\)/ {
	k = $0
	sub(/^.*[A-Za-z]+Row\("/, "", k); sub(/".*$/, "", k)
	key = k
	seen[key] = 0
	order[++nkeys] = key
}
key != "" && /\.field\(COREAPI_[A-Z_]+\(/ {
	m = $0
	sub(/^.*\.field\(COREAPI_/, "", m)
	macro = m; sub(/\(.*$/, "", macro)
	sub(/^[A-Z_]+\(/, "", m)
	nargs = split(m, arg, ",")
	for (i = 1; i <= nargs; i++) { sub(/\).*$/, "", arg[i]); arg[i] = trim(arg[i]) }
	if (macro == "FLAG_FILE_FIELD") tok = "-"
	else if (macro == "THEME_FIELD") tok = "theme." arg[1]
	else if (macro == "GLCD_THEME_FIELD" || macro == "GLCD_THEME_TEXT_FIELD") tok = "glcd_theme." arg[1]
	else if (macro == "COLOR_FIELD") tok = arg[1] "." arg[2] "_"
	else tok = arg[1]
	print key "\t" tok
	if (macro == "MASK_BIT_FIELD") print key "\t" arg[2]
	seen[key] = 1
	key = ""
}
END {
	for (i = 1; i <= nkeys; i++) if (!seen[order[i]]) print order[i] "\t?"
}

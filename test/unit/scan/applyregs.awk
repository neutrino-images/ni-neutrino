# Where a group is registered or written, outside the files that hold groups. Reads
# the sources behind strip-comments.awk (mark, literals replaced) and blank-if0.awk.
# One line per place, tab between: file, function, what. Groups belong in
# src/coreapi/box/apply_*.cpp, and the one call that registers them is in a function
# named registerApplyGroups, so a screen cannot add a group the scan never reads.
# The registry and its header define the call and are not callers.
index($0, mark) == 1 {
	file = substr($0, length(mark) + 1)
	rel = file
	if (root != "" && index(rel, root) == 1) rel = substr(rel, length(root) + 1)
	rel = "/" rel
	skip = (rel ~ /\/apply_[a-z0-9_]+\.cpp$/ || rel ~ /\/coreapi\/settings\/apply\.cpp$/ || rel ~ /\/coreapi\/base\/apply\.h$/)
	fn_name = ""; pending = ""
	next
}
{
	l = $0
	if (fn_name == "") {
		if (l ~ /^\}/ || l ~ /;[ \t]*$/) pending = ""
		else if (l ~ /^[A-Za-z_]/ && l ~ /\(/) {
			h = substr(l, 1, index(l, "(") - 1); sub(/[ \t]+$/, "", h)
			if (match(h, /[A-Za-z_0-9~:]+$/)) pending = substr(h, RSTART, RLENGTH)
		}
		if (l ~ /^\{/ && pending != "") { fn_name = pending; pending = "" }
	}
	else if (l ~ /^\}/) fn_name = ""
	if (skip) next
	base = fn_name; sub(/^.*::/, "", base)
	if (l ~ /registerApplyGroup[ \t]*\(/ && base != "registerApplyGroups")
		print file "\t" (fn_name == "" ? "-" : fn_name) "\tregisterApplyGroup call"
	if (l ~ /ApplyGroup[ ]+[A-Za-z_0-9]+[ ]*(\[[^]]*\])?[ ]*=/)
		print file "\t" (fn_name == "" ? "-" : fn_name) "\tApplyGroup initialiser"
}

# The apply groups a file registers, read as text behind strip-comments.awk with
# literals kept and blank-if0.awk. A group's initialiser hands COREAPI_KEYS an
# array of strings declared in the same file; one line per key, tab between: the
# group's name, the key, the file. A group whose array is not found prints ERR,
# the file and what is missing, which the caller turns into a stop: a list that
# cannot be read is one that is silently empty.
{ text = text " " $0 }
END {
	gsub(/[ \t]+/, " ", text)
	shape = "{ \"name\", ApplyPhase::X, COREAPI_KEYS(kNameKeys), &run } with kNameKeys an array of string literals in the same file"
	t = text
	while (match(t, /ApplyGroup[ ]+[A-Za-z_0-9]+[ ]*(\[[^]]*\])?[ ]*=/)) { declared++; t = substr(t, RSTART + RLENGTH) }
	s = text
	while (match(s, /\{ ?"[A-Za-z0-9_]+"[^{}]*COREAPI_KEYS\(([A-Za-z_0-9]+)\)/)) {
		piece = substr(s, RSTART, RLENGTH)
		s = substr(s, RSTART + RLENGTH)
		name = piece; sub(/^\{ ?"/, "", name); sub(/".*$/, "", name)
		arr = piece; sub(/^.*COREAPI_KEYS\(/, "", arr); sub(/\)$/, "", arr)
		body = ""
		t = text
		if (match(t, "[ *]" arr " ?\\[[^]]*\\] ?= ?\\{[^;]*\\};")) {
			body = substr(t, RSTART, RLENGTH)
			sub(/^[^{]*\{/, "", body)
		}
		if (body == "") { print "ERR\t" file "\tno key list " arr " for the group " name " (a group is " shape ")"; continue }
		parsed++
		while (match(body, /"[^"]*"/)) {
			print name "\t" substr(body, RSTART + 1, RLENGTH - 2) "\t" file
			body = substr(body, RSTART + RLENGTH)
		}
	}
	if (parsed < declared)
		print "ERR\t" file "\t" declared " ApplyGroup initialisers and " parsed " that could be read (a group is " shape ")"
}

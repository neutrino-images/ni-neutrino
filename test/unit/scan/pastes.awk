# Prints the name of every file given that pastes tokens in a way that could
# spell a build condition, so check-hardware.sh compiles what includes it under
# every condition rather than only under those it mentions.
#
# A condition is HAVE_..._HARDWARE, ENABLE_... or BOXMODEL_..., in capitals,
# digits and underscores alone. A chain a ## b ## c makes one name out of its
# pieces, and a piece that is one of the macro's parameters can be anything. So
# a chain cannot spell a condition when a piece that is not a parameter holds a
# small letter, or when its first piece is not a parameter and does not begin
# the way a condition begins. Every other chain could, and names its file.
#
# Lines continued with a backslash are joined first, since a definition with a
# paste is mostly a long one.

function startsLikeCondition(piece,   p, k) {
	for (k in prefixes) {
		p = prefixes[k]
		if (substr(piece, 1, length(p)) == p || substr(p, 1, length(piece)) == piece)
			return 1
	}
	return 0
}

function judge(text, file,   head, params, n, i, rest, chain, pieces, m, j, safe, param, total, taken) {
	if (index(text, "##") == 0)
		return
	delete isparam
	# The parameters of a function-like definition, which is the only place a paste stands.
	if (match(text, /^[ \t]*#[ \t]*define[ \t]+[A-Za-z_][A-Za-z_0-9]*\(/)) {
		head = substr(text, RSTART + RLENGTH)
		head = substr(head, 1, index(head, ")") - 1)
		n = split(head, params, ",")
		for (i = 1; i <= n; i++) {
			param = params[i]
			gsub(/[ \t]|\.\.\./, "", param)
			if (param != "")
				isparam[param] = 1
		}
		isparam["__VA_ARGS__"] = 1
	}
	total = gsub(/##/, "##", text)
	taken = 0
	rest = text
	while (match(rest, /[A-Za-z_0-9]+([ \t]*##[ \t]*[A-Za-z_0-9]+)+/)) {
		chain = substr(rest, RSTART, RLENGTH)
		rest = substr(rest, RSTART + RLENGTH)
		gsub(/[ \t]/, "", chain)
		m = split(chain, pieces, "##")
		taken += m - 1
		safe = !(pieces[1] in isparam) && !startsLikeCondition(pieces[1])
		for (j = 1; !safe && j <= m; j++)
			if (!(pieces[j] in isparam) && pieces[j] ~ /[a-z]/)
				safe = 1
		if (!safe) {
			flagged[file] = 1
			return
		}
	}
	# A paste no chain above took apart, beside a quote or a number sign.
	if (taken != total)
		flagged[file] = 1
}

BEGIN {
	split("HAVE_ ENABLE_ BOXMODEL_", prefixes, " ")
}

FNR == 1 {
	if (pending != "")
		judge(pending, last)
	pending = ""
	last = FILENAME
}

{
	line = $0
	if (sub(/\\$/, "", line)) {
		pending = pending line " "
		next
	}
	judge(pending line, FILENAME)
	pending = ""
}

END {
	if (pending != "")
		judge(pending, last)
	for (f in flagged)
		print f
}

# What a conversion took out of a screen besides the items, set against what the
# declaration says now. The items themselves are compared by the check this runs
# behind; here it is the code beside them that did the screen's own activation and
# the screen's own applying:
#
#   setActive, COnOffNotifier, CGenericMenuActivate and the key checks of a menu
#     each setting they read has to be named in the condition of some row, or the
#     greying went with the code
#   a branch of a change notifier, found by the locale it tests
#     the setting it stands for has to be in an apply group, or what the branch
#     did is read nowhere
#
# Everything here is read as text and can say only that nothing names a setting,
# never that what a row says is the same thing the code did, so each setting is
# printed with the condition the rows state for it and the person reading the
# screen decides. A line is a NOTE when there is something to compare and a WARN
# when there is nothing: no row names the setting in a condition, or a branch's
# setting has no group.
#
# Files: removed (the removed lines, comments dropped), rows (the declaration
# behind strip-comments.awk with literals kept), members (key and member, tab
# between), groups (group, key, file) and locales
# (enumerator and the label it stands for).
function trim(s) { sub(/^[ \t]+/, "", s); sub(/[ \t]+$/, "", s); return s }
function readlines(path, arr,   n, l) {
	n = 0
	while ((getline l < path) > 0) arr[++n] = l
	close(path)
	return n
}
function warn(msg) { print "WARN " msg; nwarn++ }
function note(msg) { print "NOTE " msg; nnote++ }

# The text between the parenthesis at position p of s and its partner on that line.
function call_arg(s, p,   i, c, depth) {
	depth = 0
	for (i = p; i <= length(s); i++) {
		c = substr(s, i, 1)
		if (c == "(") depth++
		else if (c == ")" && --depth == 0) return substr(s, p + 1, i - p - 1)
	}
	return substr(s, p + 1)
}

# The keys the members read in a piece of code stand for.
function keys_of(code, out,   n, m, tok, k, cnt, parts, i, seen) {
	cnt = 0
	while (match(code, /g_settings\.[A-Za-z_0-9]+(\.[A-Za-z_0-9]+)?/)) {
		tok = substr(code, RSTART + 11, RLENGTH - 11)
		code = substr(code, RSTART + RLENGTH)
		split(tok, parts, ".")
		m = (parts[2] != "" && (parts[1] == "theme" || parts[1] == "glcd_theme")) ? tok : parts[1]
		n = split(keysof[m], ks, " ")
		for (i = 1; i <= n; i++) if (!(ks[i] in seen)) { seen[ks[i]] = 1; out[++cnt] = ks[i] }
		if (n == 0 && !(("?" m) in seen)) { seen["?" m] = 1; out[++cnt] = "?" m }
	}
	return cnt
}

BEGIN {
	# the declaration, whole
	n = readlines(rows, R)
	text = ""
	for (i = 1; i <= n; i++) text = text " " R[i]
	gsub(/[ \t]+/, " ", text)

	s = text
	while (match(s, /Condition [A-Za-z0-9_]+\[\] ?= ?\{[^;]*\};/)) {
		piece = substr(s, RSTART, RLENGTH); s = substr(s, RSTART + RLENGTH)
		name = piece; sub(/^Condition /, "", name); sub(/\[.*$/, "", name)
		body = piece; sub(/^[^{]*\{ ?/, "", body); sub(/ ?\};$/, "", body)
		cond[name] = trim(body)
	}

	nrows = 0
	s = text; off = 0
	while (match(s, /[A-Za-z]+Row\("[^"]+"\)/)) {
		start[++nrows] = off + RSTART
		head = substr(s, RSTART, RLENGTH)
		rkey[nrows] = head; sub(/^[^"]*"/, "", rkey[nrows]); sub(/".*$/, "", rkey[nrows])
		off += RSTART + RLENGTH - 1
		s = substr(s, RSTART + RLENGTH)
	}
	for (i = 1; i <= nrows; i++) {
		seg = substr(text, start[i], (i < nrows ? start[i + 1] - start[i] : length(text) - start[i] + 1))
		lab = ""
		if (match(seg, /\.label\("[^"]*"\)/)) { lab = substr(seg, RSTART + 8, RLENGTH - 10); labelkeys[lab] = labelkeys[lab] " " rkey[i] }
		cw = ""
		if (match(seg, /\.changeableWhen\([A-Za-z0-9_]+\)/)) {
			cw = substr(seg, RSTART + 16, RLENGTH - 17)
			rowcond[rkey[i]] = (cw in cond) ? cond[cw] : ("(" cw ")")
		}
	}
	# which settings some row's condition names
	for (k in rowcond) {
		c = rowcond[k]
		while (match(c, /when\("[^"]+"\)/)) {
			named[substr(c, RSTART + 6, RLENGTH - 8)] = named[substr(c, RSTART + 6, RLENGTH - 8)] " " k
			c = substr(c, RSTART + RLENGTH)
		}
	}

	n = readlines(members, M)
	for (i = 1; i <= n; i++) { split(M[i], f, "\t"); keysof[f[2]] = keysof[f[2]] " " f[1] }
	n = readlines(groups, G)
	for (i = 1; i <= n; i++) { split(G[i], f, "\t"); grp[f[2]] = f[1] }
	n = readlines(locales, L)
	for (i = 1; i <= n; i++) { split(L[i], f, "\t"); loc[f[1]] = f[2] }

	n = readlines(removed, D)
	for (i = 1; i <= n; i++) {
		l = D[i]
		code = l
		if (match(l, /(->|\.)setActive\(/)) {
			arg = call_arg(l, RSTART + RLENGTH - 1)
			report("setActive(" trim(arg) ")", arg)
		}
		if (l ~ /new CMenuOption[A-Za-z]*Chooser\(/ && match(l, /&g_settings\.[A-Za-z_0-9]+/)) {
			last = substr(l, RSTART + 1, RLENGTH - 1)
			itemkeys = keys_of(last, lastks)
		}
		if (l ~ /[Nn]otifier(->|\.)addItem\(/) { item_notifier(trim(l)); continue }
		# a key check handed to an item's constructor is that item's own activation
		if (l ~ /new CMenuOption[A-Za-z]*Chooser\(.*CApiKey::check_/) { item_notifier(trim(l)); continue }
		if (l ~ /COnOffNotifier|CGenericMenuActivate|CApiKey::check_/) report(trim(l), l)
		if (match(l, /(ARE_LOCALES_EQUAL\(OptionName,[ \t]*|OptionName[ \t]*==[ \t]*)LOCALE_[A-Z0-9_]+/)) {
			e = substr(l, RSTART, RLENGTH); sub(/^.*LOCALE_/, "LOCALE_", e)
			branch(e)
		}
	}
	printf "check-setting-wiring apply pass: %d note(s), %d WARN\n", nnote + 0, nwarn + 0
	exit (nwarn > 0) ? 3 : 0
}

# An item handed to a notifier that greys it: the row of that item has to carry a
# condition, the one the notifier tested.
function item_notifier(what,   i, k) {
	if (itemkeys == 0) { note("removed " what ": the item it greys is not one this reads a setting of"); return }
	for (i = 1; i <= itemkeys; i++) {
		k = lastks[i]
		if (substr(k, 1, 1) == "?") continue
		if (k in rowcond) note("removed " what ": the row of " k " says " rowcond[k])
		else warn("removed " what ": the row of " k " has no changeableWhen, so the greying went with the notifier")
	}
}

function report(what, code,   cnt, ks, i, k, who, c) {
	cnt = keys_of(code, ks)
	if (cnt == 0) {
		note("removed " what " names no setting, so it is the screen's own condition: it stays in the screen or becomes an availability predicate")
		return
	}
	for (i = 1; i <= cnt; i++) {
		k = ks[i]
		if (substr(k, 1, 1) == "?") { warn("removed " what " reads g_settings." substr(k, 2) ", which no row holds") ; continue }
		if (k in named) note("removed " what " reads " k ", which the condition of" named[k] " names: " cmp(k))
		else warn("removed " what " reads " k " and no row's condition names it, so the greying went with the code")
	}
}

function cmp(k,   out, j, c) {
	out = ""
	for (j in rowcond) if (index(" " named[k], " " j) && out == "") out = j ": " rowcond[j]
	return out
}

function branch(e,   lab, ks, n, i, k, status) {
	if (!(e in loc)) { warn("removed notifier branch tests " e ", which is no locale the program has"); return }
	lab = loc[e]
	n = split(labelkeys[lab], ks, " ")
	if (n == 0) { note("removed notifier branch tests " e " (" lab "), which no row labels: a screen item that is no setting"); return }
	for (i = 1; i <= n; i++) {
		k = ks[i]
		if (k in grp) note("removed notifier branch of " k ": group " grp[k])
		else warn("removed notifier branch of " k " (" e "): no group, so what the branch did is read nowhere")
	}
}

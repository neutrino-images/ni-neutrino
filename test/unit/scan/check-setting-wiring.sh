#!/bin/sh
# One-off check for a setup screen converted to addSetting(). Not part of make
# check: it needs the revision before the conversion, which a build does not have.
#
# Every CMenuOptionChooser and CMenuOptionNumberChooser construction removed from
# <file> since <rev> is compared with the addSetting call that took its place,
# paired by the g_settings field. What is compared: label, hint, active
# expression, observer, direct key, the menu and any notifier the item is handed
# to, and the numbers with the value shown in words or the table of values with
# the locale each entry shows. Table entries that sit in an #if arm are reported as
# warnings, since the generic build does not say which arm a box takes, unless the
# row has the same entry under the same condition. The arms of one #if chain are
# alternatives for one position and count once.
#
# The hint is read from the item's statements up to its addItem, the variable's
# next assignment or the end of the function, across later items' constructions.
# A hint icon is a NOTE: addSetting sets none, so the screen sets it on the item
# addSetting returns.
#
# A condition on the box around the removed item, an if on g_info.hw_caps or a
# file probe, that is gone around the addSetting has to be the test the row
# carries (read out of predicates.cpp and the capability source); under its else
# the item is compared with the row's other shape, and one row in two shapes
# takes both items. A row with a test whose item sat under no such condition is
# a WARN. addSetting calls the old file had already are paired with the new
# ones by key and held to the same arguments and the same rule. Bounds a row
# writes in #if arms are compared with the arm the removed item was in, and the
# statements of another arm of the item's own chain are not its statements.
#
# <rev> is diffed against the working tree file, so the check runs before a
# commit as well as after one (use HEAD~1 or the parent then).
#
# addChoiceSetting and addNumberSetting are read as addSetting with their arguments
# put in addSetting's places, and every argument left out is compared as its
# default, so moving a call to a typed builder or dropping a trailing default is
# no change. An observer the conversion dropped is a NOTE where the key is in an
# apply group, whose run is what the observer did, and a MISMATCH otherwise. A
# chooser built by hand that stays, its observer the only change, is a NOTE.
#
# For the suite's fixtures: WIRING_OLD names a file that stands for the screen at
# <rev>, WIRING_TOP the source tree, and WIRING_GROUPS one more file of apply
# groups to read; git is not asked then.
#
# Exit 0 when every pair matches and nothing needs a second look, 1 on a mismatch
# or when nothing was found, 2 on bad usage, 3 when there is no mismatch but at
# least one WARN (an entry in an #if arm, a row entry with an availability
# predicate, an item whose menu could not be found, a condition on the box the
# row does not carry). A WARN is for the human who
# reads the screen, so 3 is not a pass.
#
# After the items, a second pass (wiringapply.awk) reads what the conversion took
# out beside them: setActive, the notifiers that grey an item, the key checks of a
# menu and the branches of a change notifier. Each setting they read is set against
# the conditions the rows state and the apply groups; a NOTE shows both sides for the
# reader to compare and a WARN says there is nothing to compare, either because no
# row's condition names the setting or because the branch's setting has no group. Its WARNs count
# as this check's WARNs.
#
# Not checked, and so left to the review of each screen: the #if around the item,
# an if that is not on the box, the order of the items in the menu, arguments of the addItem call other
# than the item, and later uses of the old item variable (setActive, setMarked,
# icon hints). A NOTE line is printed when the removed code used the variable
# again; NOTE does not change the exit status.
set -e
LC_ALL=C
export LC_ALL

REV="$1"
FILE="$2"
[ -n "$REV" ] && [ -n "$FILE" ] || {
	echo "usage: check-setting-wiring.sh <rev> <file>" >&2
	exit 2
}

HERE=`cd "\`dirname "$0"\`" && pwd`
if [ -n "$WIRING_TOP" ]; then
	TOP=`cd "$WIRING_TOP" && pwd`
else
	TOP=`git rev-parse --show-toplevel`
fi
[ -r "$FILE" ] || { echo "check-setting-wiring.sh: cannot read $FILE" >&2; exit 2; }
ABS=`cd "\`dirname "$FILE"\`" && pwd`/`basename "$FILE"`
REL=${ABS#$TOP/}
STRIP="$HERE/strip-comments.awk"

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

cd "$TOP"
if [ -n "$WIRING_OLD" ]; then
	cp "$WIRING_OLD" "$tmp/old.raw" || exit 2
	diff -U0 "$tmp/old.raw" "$REL" > "$tmp/diff" || true
else
	git show "$REV:$REL" > "$tmp/old.raw" 2>/dev/null || {
		echo "check-setting-wiring.sh: $REL does not exist in $REV" >&2
		exit 2
	}
	git diff -U0 "$REV" -- "$REL" > "$tmp/diff" || true
fi
awk -v keepstrings=1 -f "$STRIP" "$tmp/old.raw" > "$tmp/old.c"
awk -v keepstrings=1 -f "$STRIP" "$REL" > "$tmp/new.c"
awk -v keepstrings=1 -f "$STRIP" src/coreapi/settings/settingstable*.cpp > "$tmp/rows.c"
# The rows are built with calls; the item scan reads them in the places a positional row had them.
awk -f "$HERE/wiringrows.awk" "$tmp/rows.c" > "$tmp/rows-pos.c"
# What each test of the box reads, where the tree has tests yet.
: > "$tmp/preds.c"; : > "$tmp/caps.c"
[ -r src/coreapi/settings/predicates.cpp ] && awk -v keepstrings=1 -f "$STRIP" src/coreapi/settings/predicates.cpp > "$tmp/preds.c"
[ -r src/coreapi/box/systemsource_real.cpp ] && awk -v keepstrings=1 -f "$STRIP" src/coreapi/box/systemsource_real.cpp > "$tmp/caps.c"
grep '^@@' "$tmp/diff" > "$tmp/hunks" || true

# The apply groups, which say where an observer the conversion dropped went.
: > "$tmp/groups.tsv"
for g in src/coreapi/box/apply_*.cpp $WIRING_GROUPS; do
	[ -r "$g" ] || continue
	awk -v keepstrings=1 -f "$STRIP" "$g" | awk -f "$HERE/blank-if0.awk" \
		| awk -v file="$g" -f "$HERE/applygroups.awk" | grep -v '^ERR	' >> "$tmp/groups.tsv" || true
done
# Enumerators by value, those of the old screen first: a block the screen itself
# declared is gone from the tree once it is converted.
{
	awk -v keepstrings=0 -f "$STRIP" "$tmp/old.raw" | awk -f "$HERE/enums.awk"
	sh "$HERE/extract-constants.sh" src lib
} > "$tmp/names"

# the pass below still has to run when this one finds a mismatch
set +e
awk -v old="$tmp/old.c" -v new="$tmp/new.c" -v rows="$tmp/rows-pos.c" -v hunks="$tmp/hunks" -v names="$tmp/names" \
	-v preds="$tmp/preds.c" -v capsrc="$tmp/caps.c" -v groups="$tmp/groups.tsv" \
	-v localesh="src/system/locals.h" -v localesi="src/system/locals_intern.h" \
	-v rev="$REV" -v strip="$STRIP" -v rel="$REL" '
function trim(s) { sub(/^[ \t\r]+/, "", s); sub(/[ \t\r]+$/, "", s); return s }
function nows(s) { gsub(/[ \t\r]+/, "", s); return s }
function readfile(path, arr,    n, l) {
	n = 0
	while ((getline l < path) > 0) arr[++n] = l
	close(path)
	return n
}
function fail(msg) { print "MISMATCH " msg; nfail++ }

# A call of addSetting or one of its typed forms, its arguments put in the places
# addSetting has for them, and every one left out filled with its default, so two spellings of one
# item compare equal. fname names the builder, A holds the arguments as written.
function norm_call(fname, n, A, OUT,    j) {
	delete OUT
	if (fname == "addChoiceSetting") {
		for (j = 1; j <= n && j <= 5; j++) OUT[j] = A[j]
		if (n >= 6) OUT[7] = A[6]
	} else if (fname == "addNumberSetting") {
		for (j = 1; j <= n && j <= 6; j++) OUT[j] = A[j]
		if (n >= 7) OUT[8] = A[7]
	} else
		for (j = 1; j <= n; j++) OUT[j] = A[j]
	if (!(3 in OUT)) OUT[3] = "true"
	if (!(4 in OUT)) OUT[4] = "NULL"
	if (!(5 in OUT)) OUT[5] = "CRCInput::RC_nokey"
	if (!(6 in OUT)) OUT[6] = "false"
	if (!(7 in OUT)) OUT[7] = "false"
	if (!(8 in OUT)) OUT[8] = "false"
	if (!(9 in OUT)) OUT[9] = "NONEXISTANT_LOCALE"
	if (!(10 in OUT)) OUT[10] = "NONEXISTANT_LOCALE"
	if (nows(OUT[4]) == "0" || nows(OUT[4]) == "nullptr") OUT[4] = "NULL"
	return 10
}

# A chooser construction told apart from another by everything but its observer,
# the sixth argument of both kinds.
function chooser_sig(kind, n, A,    j, t) {
	t = kind
	for (j = 1; j <= n; j++) if (j != 6) t = t "|" nows(A[j])
	return t
}
function warn(msg) { print "WARN " msg; nwarn++ }

# Splits at top-level commas, quotes and brackets honoured.
function split_args(s, out,    n, i, c, depth, cur, q) {
	n = 0; depth = 0; cur = ""; q = ""
	for (i = 1; i <= length(s); i++) {
		c = substr(s, i, 1)
		if (q != "") {
			cur = cur c
			if (c == "\\") { i++; cur = cur substr(s, i, 1) }
			else if (c == q) q = ""
			continue
		}
		if (c == "\"" || c == "\047") { q = c; cur = cur c; continue }
		if (c == "(" || c == "[" || c == "{") depth++
		if (c == ")" || c == "]" || c == "}") depth--
		if (c == "," && depth == 0) { out[++n] = trim(cur); cur = ""; continue }
		cur = cur c
	}
	if (trim(cur) != "" || n > 0) out[++n] = trim(cur)
	return n
}

# The text between the parenthesis at column pos of line i and its partner,
# which may be some lines on. Result in STMT, last line in STMTEND.
function call_text(L, nl, i, pos,    j, s, k, c, depth, q, text, started) {
	depth = 0; q = ""; text = ""; started = 0
	for (j = i; j <= nl; j++) {
		s = (j == i) ? substr(L[j], pos) : L[j]
		for (k = 1; k <= length(s); k++) {
			c = substr(s, k, 1)
			if (q != "") {
				if (started) text = text c
				if (c == "\\") { k++; if (started) text = text substr(s, k, 1) }
				else if (c == q) q = ""
				continue
			}
			if (c == "\"" || c == "\047") { q = c; if (started) text = text c; continue }
			if (c == "(") { depth++; if (depth == 1) { started = 1; continue } }
			if (c == ")") { depth--; if (depth == 0) { STMT = text; STMTEND = j; return 1 } }
			if (started) text = text c
		}
		if (started) text = text " "
	}
	STMT = text; STMTEND = nl
	return 0
}

# A number, whatever the source spelled it as: a literal, a bool, or a name the
# tree gives a value. "?name" when the tree gives none.
function num(s,    v, cmd, line, m) {
	s = nows(s)
	gsub(/^\(int\)|^\(int\*\)|^\(unsigned\)/, "", s)
	if (s ~ /^-?[0-9]+$/) return s + 0
	if (s == "true") return 1
	if (s == "false") return 0
	if (s ~ /^0[xX][0-9a-fA-F]+$/) return hex(s)
	sub(/^.*::/, "", s)
	if (s in symcache) return symcache[s]
	v = "?" s
	if (s ~ /^[A-Za-z_][A-Za-z_0-9]*$/) {
		defvalues(s)
		if (DVN == 1 && !DVBAD) v = DV[1]
		# an enumerator that takes its value from its place in the block
		else if (DVN == 0 && DSN == 0 && !DVBAD && (s in cname)) v = cname[s]
	}
	symcache[s] = v
	return v
}

# Every value the tree gives a name by a plain integer literal: a define, or the
# name'"'"'s own initialiser. DVBAD is set when some definition is anything else, and
# DSZ lists tables a define takes the size of.
function addv(x,    i, v) {
	x = trim(x)
	if (x ~ /^-?[0-9]+$/) v = x + 0
	else if (x ~ /^0[xX][0-9a-fA-F]+$/) v = hex(x)
	else { DVBAD = 1; return }
	for (i = 1; i <= DVN; i++) if (DV[i] == v) return
	DV[++DVN] = v
}
function consider(l, name,    r, t) {
	gsub(/\/\*[^*]*\*\//, "", l); sub(/\/\/.*$/, "", l)
	if (l ~ ("^[ \t]*#[ \t]*define[ \t]+" name "([ \t]|$)")) {
		sub("^[ \t]*#[ \t]*define[ \t]+" name "[ \t]*", "", l)
		if (match(l, /sizeof[ \t]*\([ \t]*[A-Za-z_0-9]+[ \t]*\)/)) {
			t = substr(l, RSTART, RLENGTH); sub(/^sizeof[ \t]*\([ \t]*/, "", t); sub(/[ \t]*\)$/, "", t)
			DSZ[++DSN] = t
			return
		}
		addv(l)
		return
	}
	if (match(l, "(^|[^A-Za-z_0-9.>])" name "[ \t]*=[^=]")) {
		r = substr(l, RSTART + RLENGTH - 1)
		if (match(r, /^[ \t]*\(?[ \t]*sizeof[ \t]*\([ \t]*[A-Za-z_0-9]+[ \t]*\)/)) {
			t = substr(r, RSTART, RLENGTH); sub(/^.*sizeof[ \t]*\([ \t]*/, "", t); sub(/[ \t]*\)$/, "", t)
			DSZ[++DSN] = t
			return
		}
		sub(/[,;}].*$/, "", r)
		addv(r)
	}
}
function defvalues(name,    cmd, l, i) {
	delete DV; delete DSZ; DVN = 0; DSN = 0; DVBAD = 0
	for (i = 1; i <= nold; i++) if (index(OLD[i], name)) consider(OLD[i], name)
	# The old revision of the screen is the authority for what it defines itself.
	if (DVN > 0 || DSN > 0 || DVBAD) return
	# Comments are removed per file first, so a line inside a block comment is not read as a definition.
	cmd = "grep -rlw -- '"'"'" name "'"'"' src --include='"'"'*.h'"'"' --include='"'"'*.cpp'"'"' 2>/dev/null | xargs awk -v keepstrings=1 -f '"'"'" strip "'"'"' 2>/dev/null | grep -w -- '"'"'" name "'"'"'"
	while ((cmd | getline l) > 0) consider(l, name)
	close(cmd)
}
function hex(s,    i, v, c) {
	v = 0; s = tolower(substr(s, 3))
	for (i = 1; i <= length(s); i++) {
		c = index("0123456789abcdef", substr(s, i, 1)) - 1
		v = v * 16 + c
	}
	return v
}

function str_lit(s) {
	s = trim(s)
	if (s == "NULL" || s == "0" || s == "nullptr") return ""
	if (s ~ /^".*"$/) return substr(s, 2, length(s) - 2)
	return "\001" s
}

function localekey(name) {
	name = nows(name)
	if (name == "NONEXISTANT_LOCALE") return ""
	if (!(name in locidx)) return "\001" name
	return locstr[locidx[name]]
}

# A number that did not resolve compares equal to itself, which proves nothing.
function unres(lbl, what, v) {
	if (substr(v, 1, 1) != "?") return 0
	fail(lbl ": " what " " substr(v, 2) " does not resolve to a number, cannot compare it")
	return 1
}

function show(k) { sub(/^\001/, "?", k); return k }
# No key, however spelled: left out, CRCInput::RC_nokey, RC_NOKEY.
function dkey(s) {
	s = nows(s)
	sub(/^CRCInput::/, "", s)
	if (s == "" || toupper(s) == "RC_NOKEY") return "RC_NOKEY"
	return s
}
function truth(s) { return (s == "1" || s == "true") ? "true" : ((s == "0" || s == "false") ? "false" : s) }

# ---- preprocessor arms inside a table ----
# S holds the state of one table: its depth, each level'"'"'s condition, and the
# position the next entry takes. The arms of one #if chain are alternatives, so
# each arm starts at the position the chain started at, and after #endif the
# list goes on behind the longest arm. Answers whether d was a directive.
function pp_reset(S) { delete S; S["depth"] = 0; S["slot"] = 0 }
function pp_step(d, S,    c, k) {
	d = trim(d)
	if (d !~ /^#/) return 0
	k = S["depth"]
	if (d ~ /^#[ \t]*if/) {
		c = d
		if (c ~ /^#[ \t]*ifdef/) { sub(/^#[ \t]*ifdef[ \t]*/, "", c); c = "defined(" nows(c) ")" }
		else if (c ~ /^#[ \t]*ifndef/) { sub(/^#[ \t]*ifndef[ \t]*/, "", c); c = "!defined(" nows(c) ")" }
		else { sub(/^#[ \t]*if[ \t]*/, "", c); c = "(" nows(c) ")" }
		S["depth"] = ++k
		S["cur", k] = c; S["prev", k] = c
		S["base", k] = S["slot"]; S["top", k] = S["slot"]
		S["armlen", k] = -1; S["haselse", k] = 0
		return 1
	}
	if (k == 0) return 1
	if (d ~ /^#[ \t]*(elif|else|endif)/) pp_arm(S, k)
	if (d ~ /^#[ \t]*(elif|else)/) {
		if (S["slot"] > S["top", k]) S["top", k] = S["slot"]
		S["slot"] = S["base", k]
		if (d ~ /^#[ \t]*else/) S["haselse", k] = 1
		if (d ~ /^#[ \t]*elif/) {
			c = d; sub(/^#[ \t]*elif[ \t]*/, "", c); c = "(" nows(c) ")"
			S["cur", k] = "!" S["prev", k] "&&" c
			S["prev", k] = "(" S["prev", k] "||" c ")"
		} else S["cur", k] = "!" S["prev", k]
		return 1
	}
	if (d ~ /^#[ \t]*endif/) {
		if (!S["haselse", k]) S["optional"] = 1
		if (S["slot"] > S["top", k]) S["top", k] = S["slot"]
		S["slot"] = S["top", k]
		S["depth"] = k - 1
	}
	return 1
}
# An arm that ends: a chain whose arms differ in length, or one without an #else,
# gives a table more than one size.
function pp_arm(S, k,    len) {
	len = S["slot"] - S["base", k]
	if (S["armlen", k] < 0) S["armlen", k] = len
	else if (len != S["armlen", k]) S["uneven"] = 1
}
function pp_guard(S,    i, g) {
	g = ""
	for (i = 1; i <= S["depth"]; i++) g = g (g == "" ? "" : "&&") S["cur", i]
	return g
}

# ---- locale names: enumerator i is the string at index i ----
function load_locales(    n, i, l, inenum, instr, ne, ns) {
	n = readfile(localesh, LH)
	ne = 0
	for (i = 1; i <= n; i++) {
		l = LH[i]
		if (l ~ /^typedef enum/) inenum = 1
		else if (l ~ /^}/) inenum = 0
		else if (inenum && l ~ /^[ \t]*[A-Z_0-9a-z]+,?[ \t]*$/) {
			l = trim(l); sub(/,$/, "", l)
			locidx[l] = ne++
		}
	}
	n = readfile(localesi, LI)
	ns = 0
	for (i = 1; i <= n; i++) {
		l = LI[i]
		if (l ~ /^const char \* *locale_real_names/) instr = 1
		else if (l ~ /^};/) instr = 0
		else if (instr && l ~ /^[ \t]*"/) {
			l = trim(l); sub(/,$/, "", l)
			locstr[ns++] = substr(l, 2, length(l) - 2)
		}
	}
	if (ne != ns || ne == 0) {
		print "check-setting-wiring.sh: locals.h has " ne " names and locals_intern.h " ns " strings" > "/dev/stderr"
		exit 2
	}
}

# ---- the declaration: rows and enum tables ----
function load_rows(    n, i, T, p, rest, starts, ns, k, R, nr, a, t, tok, e, body, ent, ne, parts, np, j, name, args, lo, end, l, tab, opened, pend) {
	n = readfile(rows, RL)
	T = ""
	# a directive ends where its line does, which the joined text marks
	for (i = 1; i <= n; i++) T = T " " RL[i] ((RL[i] ~ /^[ \t]*#/) ? " \002" : "")
	# the other shape a row may take, by name
	rest = T
	while (match(rest, /Shape[ \t]+[A-Za-z_0-9]+[ \t]*=[ \t]*shape\([^;]*;/)) {
		a = substr(rest, RSTART, RLENGTH)
		rest = substr(rest, RSTART + RLENGTH)
		name = a; sub(/^Shape[ \t]+/, "", name); sub(/[ \t=].*$/, "", name)
		sub(/^[^(]*\(/, "", a)
		args = a; sub(/\).*$/, "", args)
		split_args(args, parts)
		t = nows(parts[1]); sub(/^ValueType::/, "", t)
		shtype[name] = t
		shlabel[name] = str_lit(parts[2])
		shmin[name] = 0; shmax[name] = 0
		if (match(a, /\.range\([^)]*\)/)) {
			lo = substr(a, RSTART + 7, RLENGTH - 8)
			split_args(lo, parts)
			shmin[name] = num(parts[1]); shmax[name] = num(parts[2])
		}
		shenum[name] = ""
		if (match(a, /\.values\([^)]*\)/)) shenum[name] = nows(substr(a, RSTART + 8, RLENGTH - 9))
	}
	# enum tables, line by line for the arms inside them
	tab = ""
	for (i = 1; i <= n; i++) {
		l = RL[i]
		if (tab == "") {
			if (!match(l, /EnumValue[ \t]+[A-Za-z_0-9]+[ \t]*\[[ \t]*\][ \t]*=/)) continue
			tab = substr(l, RSTART, RLENGTH)
			sub(/^EnumValue[ \t]+/, "", tab); sub(/[ \t]*\[.*$/, "", tab)
			l = substr(l, RSTART + RLENGTH)
			pp_reset(PS); ne = 0; opened = 0; pend = ""
		}
		if (pp_step(l, PS)) continue
		if (!opened) { if (!sub(/^[^{]*\{/, "", l)) continue; opened = 1 }
		body = pend " " l
		while (match(body, /\{[^{}]*\}/)) {
			ent = substr(body, RSTART + 1, RLENGTH - 2)
			body = substr(body, RSTART + RLENGTH)
			np = split_args(ent, parts)
			ne++
			PS["slot"]++
			etab[tab, ne, "v"] = num(parts[1])
			etab[tab, ne, "k"] = str_lit(parts[2])
			etab[tab, ne, "t"] = str_lit(parts[3])
			etab[tab, ne, "a"] = (np >= 4 && trim(parts[4]) != "NULL" && trim(parts[4]) != "0") ? trim(parts[4]) : ""
			etab[tab, ne, "slot"] = PS["slot"]
			etab[tab, ne, "g"] = pp_guard(PS)
		}
		pend = ""
		if (index(body, "{")) { pend = trim(body); continue }
		if (index(body, "}")) {
			etab[tab, "n"] = ne
			etab[tab, "slots"] = PS["slot"]
			tab = ""
		}
	}
	# rows
	ns = 0
	rest = T; p = 0
	while (match(rest, /\{[ \t]*"[A-Za-z0-9_.]+"[ \t]*,[ \t]*ValueType::[A-Za-z]+[ \t]*,/)) {
		starts[++ns] = p + RSTART
		p += RSTART + RLENGTH - 1
		rest = substr(rest, RSTART + RLENGTH)
	}
	for (k = 1; k <= ns; k++) {
		R = substr(T, starts[k], (k < ns ? starts[k + 1] - starts[k] : length(T)))
		match(R, /"[^"]*"/); name = substr(R, RSTART + 1, RLENGTH - 2)
		R = substr(R, RSTART + RLENGTH)
		match(R, /ValueType::[A-Za-z]+/); t = substr(R, RSTART + 11, RLENGTH - 11)
		R = substr(R, RSTART + RLENGTH)
		sub(/^[ \t]*,/, "", R)
		for (j = 1; j <= 3; j++) {
			R = trim(R)
			if (substr(R, 1, 1) == "\"") {
				match(R, /^"[^"]*"/); tok = substr(R, 2, RLENGTH - 2)
				R = substr(R, RLENGTH + 1)
			} else {
				match(R, /^[A-Za-z_0-9]+/); tok = ""
				R = substr(R, RLENGTH + 1)
			}
			sub(/^[ \t]*,/, "", R)
			if (j == 2) rlabel[name] = tok
			if (j == 3) rhint[name] = tok
		}
		R = trim(R)
		rarmn[name] = 0
		if (substr(R, 1, 1) == "#") {
			# bounds the build decides: each arm of the chain gives both
			pp_reset(BS)
			rmin[name] = "?#if"; rmax[name] = "?#if"
			while (substr(R, 1, 1) == "#" && index(R, "\002")) {
				d = substr(R, 1, index(R, "\002") - 1)
				R = trim(substr(R, index(R, "\002") + 1))
				pp_step(d, BS)
				if (BS["depth"] == 0) break
				k2 = ++rarmn[name]
				rarmg[name, k2] = BS["cur", BS["depth"]]
				match(R, /^[^,]*/); rarmmin[name, k2] = num(substr(R, 1, RLENGTH)); R = trim(substr(R, RLENGTH + 2))
				match(R, /^[^,]*/); rarmmax[name, k2] = num(substr(R, 1, RLENGTH)); R = trim(substr(R, RLENGTH + 2))
			}
		} else {
			match(R, /^[^,]*/); rmin[name] = num(substr(R, 1, RLENGTH)); R = substr(R, RLENGTH + 2)
			R = trim(R)
			match(R, /^[^,]*/); rmax[name] = num(substr(R, 1, RLENGTH)); R = substr(R, RLENGTH + 2)
		}
		R = trim(R)
		rtype[name] = t
		renum[name] = ""
		if (match(R, /^COREAPI_(ENUM|VALUES)\([A-Za-z_0-9]+\)/)) {
			a = substr(R, RSTART, RLENGTH)
			sub(/^[^(]*\(/, "", a); sub(/\)$/, "", a)
			renum[name] = a
		}
		if (match(R, /COREAPI_[A-Z_]*FIELD[A-Z_]*\([ \t]*[A-Za-z_0-9]+/)) {
			a = substr(R, RSTART, RLENGTH)
			sub(/^[^(]*\([ \t]*/, "", a)
			rfield[name] = a
			if (!(a in rowbyfield)) rowbyfield[a] = name
		}
		# the test of the box a row carries, and the shape it takes where that says no
		ravail[name] = ""; rshape[name] = ""
		if (match(R, /COREAPI_NUMBER_FIELD_ON\([^)]*\)/)) {
			a = substr(R, RSTART, RLENGTH)
			sub(/^[^(]*\(/, "", a); sub(/\)$/, "", a)
			split_args(a, parts)
			if (nows(parts[2]) != "NULL") ravail[name] = nows(parts[2])
			if (nows(parts[3]) != "NULL") { rshape[name] = nows(parts[3]); sub(/^&/, "", rshape[name]) }
		}
		rowexists[name] = 1
	}
	# The other shape stands as a row of its own beside the declared one, under
	# the same key with a mark, so a removed item can be compared with either.
	for (name in rshape) {
		if (rshape[name] == "") continue
		o = name "#other"
		if (!(rshape[name] in shtype)) { fail("row " name ": shape " rshape[name] " not found"); continue }
		rtype[o] = shtype[rshape[name]]; rlabel[o] = shlabel[rshape[name]]; rhint[o] = rhint[name]
		rmin[o] = shmin[rshape[name]]; rmax[o] = shmax[rshape[name]]; renum[o] = shenum[rshape[name]]
		rfield[o] = rfield[name]; rarmn[o] = 0; ravail[o] = ravail[name]; rshape[o] = ""
		rowexists[o] = 1
	}
}

# ---- the box: what each test of a row reads ----
# A test reads one capability, and the capability is read from the hardware
# library or probed under /proc; both are spelled the way a screen spelled the
# condition it used to test, so the two compare as text.
function load_tests(    n, i, l, cur, e, m, rest, src) {
	n = readfile(capsrc, CL)
	for (i = 1; i <= n; i++) {
		l = CL[i]
		if (!match(l, /out\.[A-Za-z_0-9]+[ \t]*=[^=][^;]*;/)) continue
		e = substr(l, RSTART + 4, RLENGTH - 5)
		m = e; sub(/[ \t]*=.*$/, "", m)
		src = e; sub(/^[^=]*=[ \t]*/, "", src); src = nows(src)
		sub(/^caps->/, "", src)
		# probed differently per build: the arms are read alike, so none of them is meant
		if ((m in capof) && capof[m] != src) capof[m] = "?arms"
		else capof[m] = src
	}
	n = readfile(preds, PL)
	cur = ""
	for (i = 1; i <= n; i++) {
		l = PL[i]
		if (match(l, /^bool[ \t]+[A-Za-z_0-9]+[ \t]*\([ \t]*\)/)) {
			cur = l; sub(/^bool[ \t]+/, "", cur); sub(/[ \t]*\(.*$/, "", cur)
			continue
		}
		if (cur != "" && match(l, /return[ \t]+capabilities\(\)\.[^;]*;/)) {
			e = substr(l, RSTART, RLENGTH)
			sub(/^return[ \t]+capabilities\(\)\./, "", e); sub(/;$/, "", e)
			e = nows(e)
			match(e, /^[A-Za-z_0-9]+/)
			m = substr(e, 1, RLENGTH); rest = substr(e, RLENGTH + 1)
			testreads[cur] = (m in capof) ? ((capof[m] ~ /^\?/) ? capof[m] : capof[m] rest) : "?" m
			cur = ""
		}
	}
}
# A condition of a screen the way a test is read above.
function boxcond(c) {
	c = nows(c)
	gsub(/g_info\.hw_caps->/, "", c)
	return c
}
function is_hw(c) { return (c ~ /hw_caps->|file_exists\(|"\/proc\/|"\/sys\//) }

# ---- preprocessor arms per line ----
function line_guards(L, n, GD, GC,    i, k) {
	pp_reset(LS)
	for (i = 1; i <= n; i++) {
		if (pp_step(L[i], LS)) { GD[i] = -1; continue }
		GD[i] = LS["depth"]
		for (k = 1; k <= LS["depth"]; k++) GC[i, k] = LS["cur", k]
	}
}
# Two lines one build can both compile.
function old_compat(i, j,    k, m) {
	if (OGD[i] < 0 || OGD[j] < 0) return 1
	m = (OGD[i] < OGD[j]) ? OGD[i] : OGD[j]
	for (k = 1; k <= m; k++) if (OGC[i, k] != OGC[j, k]) return 0
	return 1
}
function new_compat(i, j,    k, m) {
	if (NGD[i] < 0 || NGD[j] < 0) return 1
	m = (NGD[i] < NGD[j]) ? NGD[i] : NGD[j]
	for (k = 1; k <= m; k++) if (NGC[i, k] != NGC[j, k]) return 0
	return 1
}
function old_guard(i,    k, g) { g = ""; for (k = 1; k <= OGD[i]; k++) g = g (g == "" ? "" : "&&") OGC[i, k]; return g }
function new_guard(i,    k, g) { g = ""; for (k = 1; k <= NGD[i]; k++) g = g (g == "" ? "" : "&&") NGC[i, k]; return g }
# Whether the line is in an arm whose condition is c, at any depth.
function old_in_arm(i, c,    k) { for (k = 1; k <= OGD[i]; k++) if (OGC[i, k] == c) return 1; return 0 }

# ---- the C conditions around a line ----
function prevstmt(L, j) {
	while (j >= 1 && (trim(L[j]) == "" || L[j] ~ /^[ \t]*#/)) j--
	return j
}
function count_of(s, ch,    t) { t = s; gsub(/"([^"\\]|\\.)*"/, "", t); return gsub(ch, "", t) }
function brace_delta(s) { return count_of(s, "\\{") - count_of(s, "\\}") }
# The condition of an if head on line h, NULL string for none.
function if_cond(L, n, h) {
	if (!match(L[h], /(^|[^A-Za-z_0-9])if[ \t]*\(/)) return ""
	call_text(L, n, h, RSTART + RLENGTH - 1)
	return nows(STMT)
}
# Adds the condition the head on line h puts on its block: an if gives its own,
# an else the negation of the if before it. Answers the new count.
function head_conds(L, n, h, C, nc,    c, q, hb) {
	c = if_cond(L, n, h)
	if (c != "") { C[++nc] = c; return nc }
	if (L[h] !~ /(^|[^A-Za-z_0-9])else([^A-Za-z_0-9]|$)/) return nc
	# the then branch ends just before the else
	if (L[h] ~ /^[ \t]*\}/) q = h
	else q = prevstmt(L, h - 1)
	if (q < 1) return nc
	if (L[q] ~ /\}[ \t]*(else.*)?$/) {
		# the brace that closes the then block is the first one on q
		hb = block_head_from_close(L, q)
	} else hb = prevstmt(L, q - 1)
	if (hb < 1) return nc
	c = if_cond(L, n, hb)
	if (c != "") C[++nc] = "!(" c ")"
	else C[++nc] = "!(?)"
	return nc
}
# For a line whose first brace closes a block: that block'"'"'s head.
function block_head_from_close(L, q,    bal, j, t) {
	bal = -1
	for (j = q - 1; j >= 1; j--) {
		if (L[j] ~ /^[ \t]*#/) continue
		bal += brace_delta(L[j])
		if (bal >= 0) {
			if (trim(L[j]) == "{") return prevstmt(L, j - 1)
			return j
		}
	}
	return 0
}
# Every condition an if or an else puts on line i, innermost first, up to the
# function the line is in. Loops and switches put none.
function encl(L, n, i, C,    nc, p, h, bal, j, closes) {
	delete C
	nc = 0
	p = prevstmt(L, i - 1)
	# a head without a brace governs the one statement after it
	if (p >= 1 && L[p] !~ /[;{}][ \t]*$/ && (L[p] ~ /(^|[^A-Za-z_0-9])if[ \t]*\(/ || L[p] ~ /(^|[^A-Za-z_0-9])else[ \t]*$/))
		nc = head_conds(L, n, p, C, nc)
	j = i - 1
	bal = 0
	while (j >= 1) {
		if (L[j] ~ /^[ \t]*#/) { j--; continue }
		bal += brace_delta(L[j])
		if (bal > 0) {
			h = j
			if (trim(L[j]) == "{") h = prevstmt(L, j - 1)
			if (h < 1 || L[h] ~ /^[^ \t]/ || L[j] ~ /^\{/) break
			nc = head_conds(L, n, h, C, nc)
			# the head line may close a block of its own, "} else {"
			closes = count_of(L[h], "\\}")
			bal = -closes + ((h == j) ? count_of(L[h], "\\{") - 1 : count_of(L[h], "\\{"))
			j = h - 1
			continue
		}
		j--
	}
	return nc
}

# ---- the diff ----
# The counts are made numbers: as strings, j < "12" stops at 2.
function load_hunks(    n, i, l, a, b, c, d, x) {
	n = readfile(hunks, HK)
	for (i = 1; i <= n; i++) {
		l = HK[i]
		sub(/^@@ -/, "", l)
		split(l, x, " ")
		a = x[1]; b = 1
		if (index(a, ",")) { b = substr(a, index(a, ",") + 1) + 0; a = substr(a, 1, index(a, ",") - 1) }
		c = x[2]; d = 1; sub(/^\+/, "", c)
		if (index(c, ",")) { d = substr(c, index(c, ",") + 1) + 0; c = substr(c, 1, index(c, ",") - 1) }
		for (j = 0; j < b; j++) removed[a + j] = 1
		for (j = 0; j < d; j++) added[c + j] = 1
	}
}

# ---- keyval tables of the old revision ----
function table_lines(name, outlines,    n, i, cmd, path, l) {
	n = 0
	for (i = 1; i <= nold; i++) outlines[++n] = OLD[i]
	if (table_start(outlines, n, name)) return n
	cmd = "git grep -l -E \"keyval(_ext)?[ \t]+" name "[ \t]*(\\\\[|=)\" \"" rev "\" -- src | head -1"
	path = ""
	if ((cmd | getline path) <= 0) path = ""
	close(cmd)
	if (path == "") return 0
	sub(/^[^:]*:/, "", path)
	cmd = "git show \"" rev ":" path "\" | awk -v keepstrings=1 -f \"" strip "\""
	n = 0
	while ((cmd | getline l) > 0) outlines[++n] = l
	close(cmd)
	return n
}
function table_start(L, n, name,    i) {
	for (i = 1; i <= n; i++)
		if (L[i] ~ ("keyval(_ext)?[ \t]+" name "[ \t]*(\\[[^]]*\\])?[ \t]*(=|$)")) return i
	return 0
}
# Fills OT[1..n] with v, key, text, arm, the condition g of the arm and the position,
# and OTSLOTS with the positions. Returns n, or -1 when the table is not to be
# found or an entry does not close.
function read_table(name,    L, n, s, i, l, np, parts, cnt, text, started, done, pend, pendarm, pendg, ent, rem) {
	delete OT
	OTSLOTS = 0
	n = table_lines(name, L)
	if (n == 0) return -1
	s = table_start(L, n, name)
	if (!s) return -1
	pp_reset(OS)
	cnt = 0; started = 0; done = 0; pend = ""; pendarm = 0
	for (i = s; i <= n && !done; i++) {
		l = L[i]
		if (pp_step(l, OS)) continue
		if (i == s) { if (!sub(/^[^=]*=/, "", l)) l = "" }
		if (!started) {
			if (index(l, "{")) { started = 1; sub(/^[^{]*\{/, "", l) } else continue
		}
		if (pend == "") { pendarm = (OS["depth"] > 0); pendg = pp_guard(OS) }
		text = pend " " l
		while (match(text, /\{[^{}]*\}/)) {
			ent = substr(text, RSTART + 1, RLENGTH - 2)
			text = substr(text, RSTART + RLENGTH)
			np = split_args(ent, parts)
			cnt++
			OS["slot"]++
			OT[cnt, "v"] = num(parts[1])
			OT[cnt, "k"] = localekey(parts[2])
			OT[cnt, "t"] = (np >= 3) ? str_lit(parts[3]) : ""
			OT[cnt, "arm"] = (pendarm || OS["depth"] > 0)
			OT[cnt, "g"] = (pendg != "") ? pendg : pp_guard(OS)
			OT[cnt, "slot"] = OS["slot"]
			pendarm = (OS["depth"] > 0); pendg = pp_guard(OS)
		}
		rem = trim(text)
		if (index(rem, "{")) { pend = rem; continue }
		pend = ""
		if (index(rem, "}")) done = 1
	}
	if (pend != "") return -1
	OTSLOTS = OS["slot"]
	return cnt
}

# The number of options the removed call passed must be the number of row
# entries, or the last one is not offered.
function tablesize(t, cs,    c, k, u) {
	c = read_table(t)
	if (c < 0) return 0
	# every arm an alternative of the same length: one size whichever arm a box takes
	if (!OS["optional"] && !OS["uneven"]) { cs[OTSLOTS] = 1; return 1 }
	cs[OTSLOTS] = 1
	u = 0
	for (k = 1; k <= c; k++) if (!OT[k, "arm"]) u++
	cs[u] = 1
	if (u != c) cs[c] = 1
	return 1
}
# A count the box decides, test ? a : b: the test has to be the one carried by the
# tail entries of the row, a the whole row and b the entries without it.
function ternary_cond(e) {
	if (e !~ /^[^?]+\?[^:]+:.+$/) return ""
	e = substr(e, 1, index(e, "?") - 1)
	return is_hw(e) ? e : ""
}
function ternary_covers(P,    c) {
	c = ternary_cond(CNTEXPR)
	return c != "" && (P in testreads) && boxcond(c) == testreads[P]
}
function countval(e,    m, v) {
	if (match(e, /-[0-9]+$/)) {
		m = substr(e, RSTART + 1) + 0
		v = num(substr(e, 1, RSTART - 1))
		return (substr(v, 1, 1) == "?") ? v : v - m
	}
	return num(e)
}
function check_ternary(lbl, e, ne,    c, rest, v1, v2, j, u, P, tail) {
	c = ternary_cond(e)
	rest = substr(e, index(e, "?") + 1)
	v1 = countval(substr(rest, 1, index(rest, ":") - 1))
	v2 = countval(substr(rest, index(rest, ":") + 1))
	if (substr(v1, 1, 1) == "?" || substr(v2, 1, 1) == "?") { fail(lbl ": option count " e " cannot be resolved to a number"); return }
	u = 0; P = ""; tail = 1
	for (j = 1; j <= ne; j++) {
		if (RE[j, "a"] == "") { u++; if (P != "") tail = 0; continue }
		if (P == "") P = RE[j, "a"]
		else if (RE[j, "a"] != P) tail = 0
	}
	if (P == "" || !ternary_covers(P)) { fail(lbl ": option count " e " depends on " c ", no entry of the row carries that test"); return }
	if (!tail) fail(lbl ": the row'"'"'s entries tested by " P " are not its last ones, " e " counts from the front")
	if (v1 != ne) fail(lbl ": option count " e " offers " v1 " where " P " holds, the row has " ne)
	if (v2 != u) fail(lbl ": option count " e " offers " v2 " where " P " says no, the row has " u " without the test")
}

function check_count(lbl, e, tab, ne,    cs, tz, i, bad, t, k, n) {
	bad = 0
	if (ternary_cond(e) != "") { check_ternary(lbl, e, ne); return }
	if (e ~ /^[0-9]+$/) cs[e + 0] = 1
	else if (e ~ /^[A-Za-z_][A-Za-z_0-9]*$/) {
		defvalues(e)
		bad = DVBAD
		n = DSN
		for (i = 1; i <= DVN; i++) cs[DV[i]] = 1
		for (i = 1; i <= n; i++) tz[i] = DSZ[i]
		for (i = 1; i <= n; i++) if (!tablesize(tz[i], cs)) bad = 1
		if (DVN == 0 && n == 0) bad = 1
	}
	else if (match(e, /^sizeof\([A-Za-z_0-9]+\)\/sizeof\(/)) {
		t = substr(e, 8); sub(/\).*$/, "", t)
		if (!tablesize(t, cs)) bad = 1
	}
	else bad = 1
	n = 0
	for (k in cs) n++
	if (bad || n == 0) { fail(lbl ": option count " e " cannot be resolved to a number"); return }
	if (!(ne in cs)) { fail(lbl ": option count " e " is not the " ne " entries of the row"); return }
	if (n > 1) warn(lbl ": option count " e " has more than one value (#if arms), the row has " ne)
}

function ident(k, t) { return (k != "") ? "k:" k : "t:" t }

# The row entry for value v, the one with the same words if there are
# alternatives for v; 0 for none.
function row_entry(v, id, ne,    j, first) {
	first = 0
	for (j = 1; j <= ne; j++) {
		if (RE[j, "v"] != v) continue
		if (RE[j, "id"] == id) return j
		if (!first) first = j
	}
	return first
}
function old_entry(v, id, cnt,    i, first) {
	first = 0
	for (i = 1; i <= cnt; i++) {
		if (OT[i, "v"] != v) continue
		if (ident(OT[i, "k"], OT[i, "t"]) == id) return i
		if (!first) first = i
	}
	return first
}

# Entries in the arms of one #if chain are alternatives for one position, so
# positions are compared and counted, not entries. An entry in an arm that the
# removed table has in an arm of the same condition with the same words is a
# match.
function compare_table(label, tabname, rkey,    cnt, i, j, ne, found, last, ov, oid, et) {
	# a flag names off and on unless its row lists its own two words
	if (rtype[rkey] == "Bool" && renum[rkey] == "") {
		ne = 2
		RE[1, "v"] = 0; RE[1, "id"] = "k:options.off"
		RE[2, "v"] = 1; RE[2, "id"] = "k:options.on"
		RE[1, "slot"] = 1; RE[2, "slot"] = 2
		RE[1, "g"] = ""; RE[2, "g"] = ""
		CMPNE = 2
	} else {
		et = renum[rkey]
		ne = etab[et, "n"]
		for (i = 1; i <= ne; i++) {
			RE[i, "v"] = etab[et, i, "v"]
			RE[i, "id"] = ident(etab[et, i, "k"], etab[et, i, "t"])
			RE[i, "a"] = etab[et, i, "a"]
			RE[i, "slot"] = etab[et, i, "slot"]
			RE[i, "g"] = etab[et, i, "g"]
			if (RE[i, "a"] != "" && !ternary_covers(RE[i, "a"])) warn(label ": row entry " RE[i, "v"] " is offered only when " RE[i, "a"] " holds, compare it with the filter the removed code had")
		}
		CMPNE = etab[et, "slots"]
	}
	cnt = read_table(tabname)
	if (cnt < 0) { fail(label ": table " tabname " not found or not closed, cannot check its values"); return }
	# A name neither side resolves is still the same value where both spell it
	# alike: one build gives one name one number.
	delete spelled
	for (i = 1; i <= cnt; i++) spelled[OT[i, "v"]] = 1
	for (j = 1; j <= ne; j++) if (!(RE[j, "v"] in spelled)) unres(label, "row value", RE[j, "v"])
	delete spelledrow
	for (j = 1; j <= ne; j++) spelledrow[RE[j, "v"]] = 1
	# every unconditional old entry must be in the row, same label
	last = 0
	for (i = 1; i <= cnt; i++) {
		ov = OT[i, "v"]; oid = ident(OT[i, "k"], OT[i, "t"])
		if (!(ov in spelledrow) && unres(label, tabname " value", ov)) continue
		found = row_entry(ov, oid, ne)
		if (OT[i, "arm"]) {
			if (!found) warn(label ": " tabname " entry " ov " (" show(oid) ") is in an #if arm and not in the row")
			else if (RE[found, "id"] != oid) warn(label ": " tabname " entry " ov " in an #if arm shows " show(oid) ", row has " show(RE[found, "id"]))
			else if (RE[found, "g"] != "" && RE[found, "g"] != OT[i, "g"]) warn(label ": " tabname " entry " ov " is under " OT[i, "g"] ", the row has it under " RE[found, "g"])
			continue
		}
		if (!found) { fail(label ": " tabname " offers " ov " (" show(oid) "), row has no such value"); continue }
		if (RE[found, "id"] != oid) fail(label ": value " ov " shows " show(oid) " in " tabname ", row has " show(RE[found, "id"]))
		if (RE[found, "g"] != "") warn(label ": " tabname " offers " ov " always, the row only under " RE[found, "g"])
		if (RE[found, "slot"] < last) fail(label ": value " ov " is in another order in the row")
		else last = RE[found, "slot"]
	}
	for (j = 1; j <= ne; j++) {
		found = old_entry(RE[j, "v"], RE[j, "id"], cnt)
		if (!found) fail(label ": row offers " RE[j, "v"] " (" show(RE[j, "id"]) "), " tabname " does not")
		else if (OT[found, "arm"] && RE[j, "id"] != ident(OT[found, "k"], OT[found, "t"]))
			; # reported above
		else if (OT[found, "arm"] && RE[j, "g"] == OT[found, "g"])
			; # the same alternative under the same condition
		else if (OT[found, "arm"] && RE[j, "g"] != "")
			; # reported above
		else if (OT[found, "arm"]) warn(label ": row offers " RE[j, "v"] " which " tabname " has only in an #if arm")
	}
}

# ---- the conditions on the box that left the screen ----
# The item was at line ol of the old file and its addSetting is at nl of the new
# one. Every condition on the box around the old item and not around the new one
# must be the test its row carries: as it is for the declared shape, negated for
# the other one. A row with a test whose item sat under no such condition hides
# it on boxes that showed it.
#
# act is the active argument of the removed item. One that is the test of the row
# made the item inactive where the box lacks it; the built item is left out
# there instead, which is said as a NOTE. ACTMATCH tells the caller.
function check_box(lbl, ak, half, L, n, ol, nl, act,    OC, NC, no2, nn2, i, j, kept, c, neg, inner, P, want, nd) {
	no2 = encl(L, n, ol, OC)
	nn2 = encl(NEW, nnew, nl, NC)
	P = ravail[ak]
	nd = 0
	ACTMATCH = 0
	if (act != "" && is_hw(act) && P != "") {
		if (boxcond(act) == testreads[P]) {
			ACTMATCH = 1; nd++
			print "NOTE " lbl ": the removed item was inactive where " P " says no, the built item is left out there"; nnote++
		}
	}
	for (i = 1; i <= no2; i++) {
		if (!is_hw(OC[i])) continue
		kept = 0
		for (j = 1; j <= nn2; j++) if (NC[j] == OC[i]) kept = 1
		if (kept) continue
		nd++
		c = OC[i]; neg = 0; inner = c
		if (c ~ /^!\(.*\)$/) { neg = 1; inner = substr(c, 3, length(c) - 3) }
		else if (c ~ /^![^=]/) { neg = 1; inner = substr(c, 2) }
		if (P == "") { warn(lbl ": removed item sat under if (" c "), the row carries no test of the box"); continue }
		want = testreads[P]
		if (want ~ /^\?/) { print "NOTE " lbl ": removed item sat under if (" c "), the row tests " P ", which reads " want " the check cannot spell out, compare them by hand"; nnote++; continue }
		if (boxcond(inner) != want) { warn(lbl ": removed item sat under if (" c "), the row tests " P ", which reads " want); continue }
		if (neg && half != "other") warn(lbl ": removed item sat under if (" c "), where the row'"'"'s test " P " says no and the row has no other shape")
		if (!neg && half == "other") warn(lbl ": removed item sat under if (" c "), where the row'"'"'s test " P " says yes, not its other shape")
	}
	if (nd == 0 && P != "") {
		kept = 0
		for (j = 1; j <= nn2; j++) if (is_hw(NC[j])) kept = 1
		if (!kept) warn(lbl ": the row tests " P " and the removed item sat under no condition on the box, so a box that showed it may not any more")
	}
}

# ---- main ----
BEGIN {
	nold = readfile(old, OLD)
	while ((getline l < names) > 0) {
		split(l, nmx, "\t")
		if (!(nmx[1] in cname)) cname[nmx[1]] = nmx[2]
	}
	close(names)
	load_locales()
	load_rows()
	load_tests()
	load_hunks()
	while ((getline l < groups) > 0) {
		split(l, gx, "\t")
		grouped[gx[2]] = gx[1]
	}
	close(groups)
	nnew = readfile(new, NEW)
	# hand-built choosers the new file adds, by everything but their observer
	for (i = 1; i <= nnew; i++) {
		if (!(i in added)) continue
		if (!match(NEW[i], /new[ \t]+CMenuOption(Number)?Chooser[ \t]*\(/)) continue
		k2 = (NEW[i] ~ /CMenuOptionNumberChooser/) ? "n" : "c"
		call_text(NEW, nnew, i, RSTART + RLENGTH - 1)
		nk = split_args(STMT, tmpk)
		keptobs[chooser_sig(k2, nk, tmpk)] = (nk >= 6) ? nows(tmpk[6]) : "NULL"
	}
	line_guards(OLD, nold, OGD, OGC)
	line_guards(NEW, nnew, NGD, NGC)

	# removed constructions
	nr = 0
	for (i = 1; i <= nold; i++) {
		if (!(i in removed)) continue
		l = OLD[i]
		if (!match(l, /new[ \t]+CMenuOption(Number)?Chooser[ \t]*\(/)) continue
		kind = (l ~ /CMenuOptionNumberChooser/) ? "n" : "c"
		cpos = RSTART + RLENGTH - 1
		pre = substr(l, 1, RSTART - 1)
		var = ""
		if (match(pre, /[A-Za-z_][A-Za-z_0-9]*[ \t]*=[ \t]*$/)) {
			var = substr(pre, RSTART, RLENGTH); sub(/[ \t]*=.*$/, "", var)
		}
		call_text(OLD, nold, i, cpos)
		cstmt = STMT; cend = STMTEND
		nk = split_args(cstmt, tmpk)
		sig = chooser_sig(kind, nk, tmpk)
		if (sig in keptobs) {
			print "NOTE " rel ":" i ": chooser kept, its observer " ((nk >= 6) ? nows(tmpk[6]) : "NULL") " is now " keptobs[sig]; nnote++
			delete keptobs[sig]
			continue
		}
		nr++
		# Every addItem the item was handed to: the menu, and any notifier that
		# switches it on and off.
		ntg = 0
		if (match(pre, /[A-Za-z_][A-Za-z_0-9]*(->|\.)addItem[ \t]*\([ \t]*$/)) {
			x = substr(pre, RSTART, RLENGTH); sub(/(->|\.)addItem.*$/, "", x)
			rtg[nr, ++ntg] = nows(x)
		}
		nn = 0; nsl = 0
		if (var != "") {
			for (j = cend + 1; j <= nold && j <= cend + 40; j++) {
				l2 = OLD[j]
				# another arm of the chain the item is in is not its code
				if (!old_compat(i, j)) continue
				if (l2 !~ ("(^|[^A-Za-z_0-9])" var "([^A-Za-z_0-9]|$)")) { if (l2 ~ /^}/) break; continue }
				if (l2 ~ ("(^|[^A-Za-z_0-9])" var "[ \t]*=[^=]")) break
				if (l2 ~ ("^[ \t]*" var "->setHint\\(")) continue
				if (match(l2, ("^[ \t]*" var "->setLocalizedValue[ \t]*\\("))) {
					call_text(OLD, nold, j, RSTART + RLENGTH - 1)
					nsl++
					if (split_args(STMT, sla) == 2) { rslv[nr, nsl] = sla[1]; rsln[nr, nsl] = sla[2] }
					else { rslv[nr, nsl] = "?unreadable"; rsln[nr, nsl] = "unreadable" }
					continue
				}
				if (match(l2, ("[A-Za-z_][A-Za-z_0-9]*(->|\\.)addItem[ \t]*\\([ \t]*" var "[ \t]*[,)]"))) {
					x = substr(l2, RSTART, RLENGTH); sub(/(->|\.)addItem.*$/, "", x)
					rtg[nr, ++ntg] = nows(x)
					continue
				}
				nn++; rnl[nr, nn] = j; rnt[nr, nn] = trim(l2)
			}
		}
		rntg[nr] = ntg; rnn[nr] = nn; rnsl[nr] = nsl
		# the conditions on the box it sat under; an else of one makes it the other shape
		rnhw[nr] = 0; rneg[nr] = 0
		ncx = encl(OLD, nold, i, CX)
		for (j = 1; j <= ncx; j++) if (is_hw(CX[j])) {
			rhw[nr, ++rnhw[nr]] = CX[j]
			if (CX[j] ~ /^!\(/) rneg[nr] = 1
		}
		rk[nr] = kind; rline[nr] = i; rvar[nr] = var; rargs_n[nr] = split_args(cstmt, tmpa)
		for (j = 1; j <= rargs_n[nr]; j++) rarg[nr, j] = tmpa[j]
		# the hint, in the statements that follow up to the addItem or the next item
		rhk[nr] = ""; rhset[nr] = 0; rhicon[nr] = ""
		if (var != "") {
			for (j = cend + 1; j <= nold && j <= cend + 40; j++) {
				l2 = OLD[j]
				if (!old_compat(i, j)) continue
				if (l2 ~ ("^[ \t]*" var "->setHint[ \t]*\\(")) {
					rhset[nr] = 1
					match(l2, /setHint[ \t]*\(/)
					call_text(OLD, nold, j, RSTART + RLENGTH - 1)
					if (split_args(STMT, hargs) == 2 && nows(hargs[2]) ~ /^[A-Za-z_][A-Za-z_0-9]*$/) {
						rhk[nr] = localekey(hargs[2])
						if (nows(hargs[1]) != "\"\"") rhicon[nr] = nows(hargs[1])
					} else rhk[nr] = "\001unreadable"
					break
				}
				# the construction of a later item does not end the search: the hint of this one
				# may come after it (doj1, doj2, doj1->setHint). Only this variable counts.
				if (l2 ~ /^}/) break
				if (l2 ~ ("(^|[^A-Za-z_0-9])" var "[ \t]*=[^=]")) break
				if (l2 ~ ("addItem[ \t]*\\([ \t]*" var "[ \t]*[,)]")) break
			}
		}
	}

	# added calls, and every addItem the built item is handed to
	# Every call of the new file and not only the added ones: a call that stays
	# may have lost the condition on the box around it.
	na = 0
	for (i = 1; i <= nnew; i++) {
		if (!match(NEW[i], /add(Choice|Number)?Setting[ \t]*\(/)) continue
		if (NEW[i] ~ /add(Choice|Number)?Setting[ \t]*\([ \t]*CMenuWidget/) continue
		pre = substr(NEW[i], 1, RSTART - 1)
		fname = substr(NEW[i], RSTART, RLENGTH); sub(/[ \t]*\($/, "", fname)
		call_text(NEW, nnew, i, RSTART + RLENGTH - 1)
		na++
		aline[na] = i; aargs_n[na] = norm_call(fname, split_args(STMT, tmpa), tmpa, tmpn)
		for (j = 1; j <= aargs_n[na]; j++) aarg[na, j] = tmpn[j]
		akey[na] = str_lit(aarg[na, 2])
		aisadd[na] = (i in added)
		if (aisadd[na]) nadded++
		aused[na] = 0
		if (match(pre, /[A-Za-z_][A-Za-z_0-9]*(->|\.)addItem[ \t]*\([ \t]*$/)) {
			x = substr(pre, RSTART, RLENGTH); sub(/(->|\.)addItem.*$/, "", x)
			atg[na, nows(x)] = 1
		}
		# the variable the built item is kept in, through a cast to its widget if any
		var = ""
		if (match(pre, /[A-Za-z_][A-Za-z_0-9]*[ \t]*=[ \t]*(static_cast[ \t]*<[^<>]*>[ \t]*\(|\([^()]*\*[ \t]*\))?[ \t]*$/)) {
			var = substr(pre, RSTART, RLENGTH); sub(/[ \t]*=.*$/, "", var)
		}
		if (var == "") continue
		aend = STMTEND
		for (j = aend + 1; j <= nnew && j <= aend + 40; j++) {
			l2 = NEW[j]
			if (l2 ~ /^}/) break
			if (l2 ~ ("(^|[^A-Za-z_0-9])" var "[ \t]*=[^=]")) break
			# words the screen still sets on the built item itself
			if (match(l2, ("(^|[^A-Za-z_0-9])" var "->setLocalizedValue[ \t]*\\("))) {
				call_text(NEW, nnew, j, RSTART + RLENGTH - 1)
				ansl[na]++
				if (split_args(STMT, sla) == 2) { aslv[na, ansl[na]] = sla[1]; asln[na, ansl[na]] = sla[2] }
				else { aslv[na, ansl[na]] = "?unreadable"; asln[na, ansl[na]] = "unreadable" }
				continue
			}
			if (match(l2, ("[A-Za-z_][A-Za-z_0-9]*(->|\\.)addItem[ \t]*\\([ \t]*" var "[ \t]*[,)]"))) {
				x = substr(l2, RSTART, RLENGTH); sub(/(->|\.)addItem.*$/, "", x)
				atg[na, nows(x)] = 1
			}
		}
	}

	# The calls the old file had already, each paired with one of the new file
	# under the same key: one that stayed, else one that moved. What they hand
	# addSetting has to be the same.
	no = 0; nchanged = 0
	for (i = 1; i <= nold; i++) {
		if (!match(OLD[i], /add(Choice|Number)?Setting[ \t]*\(/)) continue
		if (OLD[i] ~ /add(Choice|Number)?Setting[ \t]*\([ \t]*CMenuWidget/) continue
		fname = substr(OLD[i], RSTART, RLENGTH); sub(/[ \t]*\($/, "", fname)
		call_text(OLD, nold, i, RSTART + RLENGTH - 1)
		no++
		oline[no] = i; oargs_n[no] = norm_call(fname, split_args(STMT, tmpa), tmpa, tmpn)
		for (j = 1; j <= oargs_n[no]; j++) oarg[no, j] = tmpn[j]
		okey[no] = str_lit(oarg[no, 2])
		if (i in removed) nchanged++
	}
	for (o = 1; o <= no; o++) {
		b = 0
		for (pass = 1; pass <= 2 && !b; pass++)
			for (j = 1; j <= na; j++) {
				if (aused[j] || akey[j] != okey[o]) continue
				if (pass == 1 && aisadd[j]) continue
				b = j; break
			}
		lbl = rel ":" oline[o] " " okey[o]
		if (!b) { fail(lbl ": addSetting of " rev " has no addSetting in its place"); continue }
		aused[b] = 1; acarry[b] = o
		x = ""; y = ""; other = 0
		for (j = 1; j <= oargs_n[o]; j++) x = x "," nows(oarg[o, j])
		for (j = 1; j <= aargs_n[b]; j++) y = y "," nows(aarg[b, j])
		for (j = 1; j <= 10; j++) if (j != 4 && nows(oarg[o, j]) != nows(aarg[b, j])) other = 1
		if (x != y && !other && nows(aarg[b, 4]) == "NULL" && (okey[o] in grouped)) {
			print "NOTE " lbl ": observer " nows(oarg[o, 4]) " dropped, the apply group " grouped[okey[o]] " runs for the key"; nnote++
		} else if (x != y) fail(lbl ": addSetting moved and its arguments changed from (" substr(x, 2) ") to (" substr(y, 2) ")")
		check_box(lbl, okey[o], "primary", OLD, nold, oline[o], aline[b], "")
	}

	if (nr == 0 && nchanged == 0 && nadded == 0) {
		print "check-setting-wiring.sh: no removed chooser and no added addSetting in " rel " against " rev
		exit 1
	}

	npairs = 0
	for (r = 1; r <= nr; r++) {
		lbl = rel ":" rline[r]
		f = ""
		if (match(rarg[r, 2], /g_settings\.[A-Za-z_0-9]+/)) f = substr(rarg[r, 2], RSTART + 11, RLENGTH - 11)
		if (f == "") { fail(lbl ": the bound value " rarg[r, 2] " is not a g_settings field, cannot pair it"); continue }
		# A row in two shapes takes the item of each: the one under the else of its
		# test is the other shape. An addSetting in the same arm as the item first.
		a = 0; half = ""
		for (pass = 1; pass <= 2 && !a; pass++)
			for (j = 1; j <= na; j++) {
				if (!aisadd[j] || (j in acarry)) continue
				ak = akey[j]
				if (!((ak in rfield) && rfield[ak] == f)) continue
				if (pass == 1 && new_guard(aline[j]) != old_guard(rline[r])) continue
				h = (rshape[ak] != "" && rneg[r]) ? "other" : "primary"
				if ((j, h) in ahalf) continue
				if (rshape[ak] == "" && aused[j]) continue
				a = j; half = h; break
			}
		if (!a) { fail(lbl ": g_settings." f " has no addSetting in its place"); continue }
		aused[a] = 1; ahalf[a, half] = 1
		npairs++
		ak = akey[a]
		lbl = lbl " " f " -> " ak (half == "other" ? " (other shape)" : "")
		check_box(lbl, ak, half, OLD, nold, rline[r], aline[a], nows(rarg[r, (rk[r] == "n") ? 3 : 5]))
		actmatch = ACTMATCH
		if (half == "other") ak = ak "#other"
		t = rtype[ak]

		# the menu the item went into; a menu that is an object is handed over by address.
		# Whatever else the removed item was handed to, a notifier that switches it,
		# the built item has to be handed to as well.
		am = nows(aarg[a, 1]); sub(/^&/, "", am)
		if (rntg[r] == 0) warn(lbl ": cannot see which menu the removed item was added to")
		else {
			inmenu = 0; tgs = ""
			for (j = 1; j <= rntg[r]; j++) {
				tgs = tgs (j > 1 ? ", " : "") rtg[r, j]
				if (rtg[r, j] == am) inmenu = 1
			}
			if (!inmenu) fail(lbl ": removed item went into " tgs ", addSetting names " aarg[a, 1])
			else for (j = 1; j <= rntg[r]; j++)
				if (rtg[r, j] != am && !((a, rtg[r, j]) in atg))
					fail(lbl ": removed item was handed to " rtg[r, j] "->addItem as well, the built item is not")
			# and the built item must go nowhere the removed one did not
			if (inmenu) for (k in atg) {
				split(k, kp, SUBSEP)
				if (kp[1] != a || kp[2] == am) continue
				seen = 0
				for (j = 1; j <= rntg[r]; j++) if (rtg[r, j] == kp[2]) seen = 1
				if (!seen) fail(lbl ": built item is handed to " kp[2] "->addItem, the removed item was not")
			}
		}
		for (j = 1; j <= rnn[r]; j++) { print "NOTE " lbl ": removed code uses " rvar[r] " again at line " rnl[r, j] ": " rnt[r, j]; nnote++ }

		# label
		if (localekey(rarg[r, 1]) != rlabel[ak])
			fail(lbl ": label " show(localekey(rarg[r, 1])) " removed, row has " show(rlabel[ak]))

		# hint
		if (rhk[r] != rhint[ak])
			fail(lbl ": hint " (rhk[r] == "" ? "(none)" : show(rhk[r])) " removed, row has " (rhint[ak] == "" ? "(none)" : rhint[ak]))
		if (rhicon[r] != "") { print "NOTE " lbl ": hint icon " rhicon[r] " removed, addSetting sets none: the screen sets it on the returned item"; nnote++ }

		if (rk[r] == "n") {
			ia = 3; imin = 4; imax = 5; iobs = 6; idk = 7; first_extra = 8
			if (t != "Int") fail(lbl ": number chooser removed, row is " t)
			else {
				cmin = rmin[ak]; cmax = rmax[ak]; cmp = 1
				# bounds the build decides: the arm the removed item was in
				if (rarmn[ak] > 0) {
					k2 = 0
					for (k = 1; k <= rarmn[ak]; k++) if (old_in_arm(rline[r], rarmg[ak, k])) { k2 = k; break }
					if (!k2) {
						print "NOTE " lbl ": the row'"'"'s bounds depend on the build and the removed item is in none of its arms, compare them by hand"; nnote++
						cmp = 0
					} else { cmin = rarmmin[ak, k2]; cmax = rarmmax[ak, k2] }
				}
				if (cmp && unres(lbl, "minimum", num(rarg[r, imin])) + unres(lbl, "row minimum", cmin) == 0 && num(rarg[r, imin]) != cmin) fail(lbl ": minimum " num(rarg[r, imin]) " removed, row has " cmin)
				if (cmp && unres(lbl, "maximum", num(rarg[r, imax])) + unres(lbl, "row maximum", cmax) == 0 && num(rarg[r, imax]) != cmax) fail(lbl ": maximum " num(rarg[r, imax]) " removed, row has " cmax)
			}
			ndef[1] = "NULL"; ndef[2] = "0"
			# the twelfth, sliderOn, is compared below with the sixth of addSetting
			rslider = (rargs_n[r] >= 12) ? nows(rarg[r, 12]) : "false"
			rpull = "false"
			for (j = first_extra; j <= rargs_n[r] && j < 10; j++) {
				x = nows(rarg[r, j]); d = ndef[j - first_extra + 1]
				if (x != d && !(d == "NULL" && (x == "0" || x == "nullptr")) && !(d == "0" && x == "0"))
					fail(lbl ": removed argument " j " (" x ") has no place in addSetting")
			}
			# The value shown in words, from the tenth and eleventh or from
			# setLocalizedValue after it. A value without a name is shown as the
			# number, which is no value in words.
			nsp = 0
			if (rargs_n[r] >= 11 && localekey(rarg[r, 11]) != "") {
				nsp++; spv[nsp] = rarg[r, 10]; spk[nsp] = localekey(rarg[r, 11])
			} else if (rargs_n[r] >= 10 && nows(rarg[r, 10]) != "0")
				fail(lbl ": removed argument 10 (" nows(rarg[r, 10]) ") has no place in addSetting")
			for (j = 1; j <= rnsl[r]; j++)
				if (localekey(rsln[r, j]) != "") { nsp++; spv[nsp] = rslv[r, j]; spk[nsp] = localekey(rsln[r, j]) }
			# what the built item shows in words: the value the row names and any
			# the screen sets on the returned item
			nbw = 0
			et = (t == "Int") ? renum[ak] : ""
			if (et != "" && etab[et, "n"] != 1) fail(lbl ": row names " etab[et, "n"] + 0 " values in words, a number names one")
			else if (et != "") { nbw++; bwv[nbw] = etab[et, 1, "v"]; bwk[nbw] = etab[et, 1, "k"]; bwf[nbw] = "row" }
			for (j = 1; j <= ansl[a]; j++)
				if (localekey(asln[a, j]) != "") { nbw++; bwv[nbw] = num(aslv[a, j]); bwk[nbw] = localekey(asln[a, j]); bwf[nbw] = "screen" }
			delete bwused
			for (j = 1; j <= nsp; j++) {
				v = num(spv[j])
				if (unres(lbl, "value in words", v)) continue
				m = 0
				for (k = 1; k <= nbw; k++) if (!(k in bwused) && bwv[k] == v) { m = k; break }
				if (!m) { fail(lbl ": removed item shows " v " as " show(spk[j]) ", the built item shows the number"); continue }
				bwused[m] = 1
				if (bwk[m] != spk[j]) fail(lbl ": removed item shows " v " as " show(spk[j]) ", " bwf[m] " as " show(bwk[m]))
			}
			for (k = 1; k <= nbw; k++)
				if (!(k in bwused) && !unres(lbl, bwf[k] " value in words", bwv[k]))
					fail(lbl ": " bwf[k] " shows " bwv[k] " as " show(bwk[k]) ", removed item showed the number")
			defactive = ""
		} else {
			rslider = "false"
			ia = 5; iobs = 6; idk = 7; first_extra = 8
			if (t != "Enum" && t != "Bool") fail(lbl ": chooser removed, row is " t)
			else {
				CNTEXPR = nows(rarg[r, 4])
				compare_table(lbl, nows(rarg[r, 3]), ak)
				check_count(lbl, nows(rarg[r, 4]), nows(rarg[r, 3]), CMPNE)
			}
			cdef[1] = "NULL"; cdef[2] = "false"; cdef[3] = "false"
			# the ninth, Pulldown, is compared below with the seventh of addSetting
			rpull = (rargs_n[r] >= 9) ? nows(rarg[r, 9]) : "false"
			for (j = first_extra; j <= rargs_n[r]; j++) {
				if (j == 9) continue
				x = nows(rarg[r, j]); d = cdef[j - first_extra + 1]
				# an empty icon name is no icon, as NULL is
				if (j == 8 && x == "\"\"") x = "NULL"
				if (x != d && !(d == "NULL" && (x == "0" || x == "nullptr")))
					fail(lbl ": removed argument " j " (" x ") has no place in addSetting")
			}
		}

		# active, observer, direct key
		ra = (rargs_n[r] >= ia) ? nows(rarg[r, ia]) : "false"
		aa = (aargs_n[a] >= 3) ? nows(aarg[a, 3]) : "true"
		if (ra != aa && !(actmatch && aa == "true")) fail(lbl ": active " ra " removed, addSetting has " aa)
		ro = (rargs_n[r] >= iobs) ? nows(rarg[r, iobs]) : "NULL"
		ao = (aargs_n[a] >= 4) ? nows(aarg[a, 4]) : "NULL"
		if (ro == "0" || ro == "nullptr") ro = "NULL"
		if (ao == "0" || ao == "nullptr") ao = "NULL"
		if (ro != ao) {
			if (ao == "NULL" && (akey[a] in grouped)) { print "NOTE " lbl ": observer " ro " dropped, the apply group " grouped[akey[a]] " runs for the key"; nnote++ }
			else fail(lbl ": observer " ro " removed, addSetting has " ao)
		}
		rd = dkey((rargs_n[r] >= idk) ? rarg[r, idk] : "")
		ad = dkey((aargs_n[a] >= 5) ? aarg[a, 5] : "")
		if (rd != ad) fail(lbl ": direct key " rd " removed, addSetting has " ad)
		as = (aargs_n[a] >= 6) ? nows(aarg[a, 6]) : "false"
		if (truth(rslider) != truth(as)) fail(lbl ": slider " rslider " removed, addSetting has " as)
		ap = (aargs_n[a] >= 7) ? nows(aarg[a, 7]) : "false"
		if (truth(rpull) != truth(ap)) fail(lbl ": pulldown " rpull " removed, addSetting has " ap)
	}
	for (j = 1; j <= na; j++) {
		if (aused[j]) continue
		# A key, a colour or a text row is no chooser: its item replaces a forwarder, which has
		# nothing here to compare. The key of a call in a loop over an array is read by
		# addsetting.awk, not here.
		if (akey[j] ~ /\[/ || rtype[akey[j]] ~ /^(Key|String|Color)$/) {
			print "NOTE " rel ":" aline[j] " addSetting " akey[j] " replaces a forwarder, not a chooser"
			nnote++
			continue
		}
		fail(rel ":" aline[j] " addSetting " akey[j] " replaces no removed chooser")
	}

	printf "check-setting-wiring: %d pair(s) checked, %d mismatch(es), %d WARN, %d note(s)\n", npairs, nfail + 0, nwarn + 0, nnote + 0
	exit (nfail > 0) ? 1 : ((nwarn > 0) ? 3 : 0)
}
'
rc=$?
set -e

# What the conversion took out beside the items, against the rows and the apply
# groups. Its WARN lines count as this check's WARN: exit 3 unless the items
# already failed.
sed -n 's/^-\([^-].*\)$/\1/p; s/^-$//p' "$tmp/diff" \
	| awk -v keepstrings=1 -f "$STRIP" > "$tmp/removed.c"
awk -v keepstrings=1 -f "$STRIP" src/coreapi/settings/settingstable*.cpp | awk -f "$HERE/blank-if0.awk" \
	| awk -f "$HERE/applyrows.awk" | sort -u > "$tmp/members.tsv"
sh "$HERE/extract-bounds.sh" -m src > "$tmp/locales.tsv"
awk -v removed="$tmp/removed.c" -v rows="$tmp/rows.c" -v members="$tmp/members.tsv" -v groups="$tmp/groups.tsv" \
	-v locales="$tmp/locales.tsv" -f "$HERE/wiringapply.awk" < /dev/null
arc=$?
[ "$rc" -ne 0 ] && [ "$rc" -ne 3 ] && exit "$rc"
[ "$rc" -eq 3 ] || [ "$arc" -eq 3 ] && exit 3
exit 0

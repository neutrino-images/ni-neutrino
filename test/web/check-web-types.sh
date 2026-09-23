#!/bin/sh
# The pages, type checked, with nothing built.
#
# The other checks of this family read the source of the page as text. This one reads
# it as a program: every module is handed to tsc with checkJs, strict and noEmit, and
# the types are the JSDoc written into the modules themselves. What comes out is
# nothing at all, on purpose. The file the browser loads is the file in this tree,
# before this check and after it, which is what keeps node out of the path of a
# firmware image; data/ni-web/jsconfig.json carries that reasoning and is where the
# rules live.
#
# WHAT THIS ONE NEEDS, WHICH IS MORE THAN ITS TWENTY ODD SIBLINGS NEED.
#
# They are POSIX shell and awk and nothing else. This one needs node and a
# TypeScript, which is why it belongs to the web half of these checks: a box does not
# need it, a cross build does not need it, and a person building the firmware never
# runs it.
#
#   node          any version that runs TypeScript 5.9, which is any version
#                 still supported. The development image carries one.
#   typescript    test/web/node_modules/typescript, out of the published
#                 archive. Pinned rather than taken from the machine, because a
#                 check whose strictness depends on whoever last ran npm is not
#                 a check. Set NI_WEB_TSC to a tsc.js to use another.
#   preact, htm, preact-router
#                 test/web/node_modules/<name>, for their declaration files.
#                 The page loads those modules from /vendor at run time and the
#                 checker has no server, so it is pointed at the declarations
#                 out of the same archives, at the same versions.
#   openapi.json  the document the server writes about itself, out of the
#                 build directory this is called with. Every shape of every
#                 answer is generated from it on each run rather than
#                 transcribed, so the page's idea of what the box returns
#                 cannot drift from the box.
#
# yweb-devenv/scripts/fetch-webui-vendor.sh puts the four archives there.
#
# IF ANY OF THAT IS MISSING THIS FAILS. It does not pass with a notice. A check that
# steps aside when its tools are absent reports a clean tree on the one machine where
# nothing was ever checked.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-web-types.sh <top source directory> <build directory>" >&2
	exit 2
}

BUILD="$2"
[ -n "$BUILD" ] || {
	echo "check-web-types.sh: no build directory, and openapi.json is written there" >&2
	echo "  usage: check-web-types.sh <top source directory> <build directory>" >&2
	exit 2
}

WEB="$SRC/data/ni-web"
HERE="$SRC/test/web"
CONFIG="$WEB/jsconfig.json"
TYPES="$HERE/types"
GENERATED="$TYPES/api.d.ts"
GENERATOR="$HERE/openapi-types.js"
DOCUMENT="$BUILD/openapi.json"
MODULES="$HERE/node_modules"
TSC="${NI_WEB_TSC:-$MODULES/typescript/lib/tsc.js}"

# The floor under the number of modules this must end up having read. A run
# that resolved no sources, or lost the include, would otherwise print no error
# and be taken for a pass.
FLOOR=30

missing() {
	echo "check-web-types.sh: $1" >&2
	echo "  this check needs node and the four archives under test/web/node_modules." >&2
	echo "  From a clone of this repository, with nothing else:" >&2
	echo "    sh test/web/fetch-types.sh" >&2
	echo "  It is not skipped when they are absent, because a check that steps" >&2
	echo "  aside reports a tree nobody checked as a clean one." >&2
	exit 1
}

command -v node >/dev/null 2>&1 || missing "node is not on the path"
[ -r "$TSC" ] || missing "cannot read $TSC"
[ -r "$CONFIG" ] || { echo "check-web-types.sh: cannot read $CONFIG" >&2; exit 1; }
[ -r "$GENERATOR" ] || { echo "check-web-types.sh: cannot read $GENERATOR" >&2; exit 1; }
for one in preact/src/index.d.ts preact/hooks/src/index.d.ts htm/dist/htm.d.ts preact-router/index.d.ts; do
	[ -r "$MODULES/$one" ] || missing "cannot read $MODULES/$one"
done

[ -r "$DOCUMENT" ] || {
	echo "check-web-types.sh: cannot read $DOCUMENT" >&2
	echo "  That is the document the server writes about itself, and every shape" >&2
	echo "  the page expects is generated from it. Under make check it is built" >&2
	echo "  before this runs; by hand it is" >&2
	echo "    make -C src/httpd openapi.json" >&2
	echo "  in the build tree, and this check is called with that build tree." >&2
	exit 1
}

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# The shapes, out of the document, on every run. Written beside the hand
# written declarations rather than into the build tree so that an editor
# reading data/ni-web/jsconfig.json sees the same types this check does; a
# stream writing a screen gets them without building anything.
#
# Moved into place only when it changed, so that a source tree nobody may write
# to still passes on a file that is already right.
mkdir -p "$TYPES"
node "$GENERATOR" "$DOCUMENT" "$tmp/api.d.ts" || {
	echo "check-web-types.sh: the shapes could not be read out of $DOCUMENT" >&2
	exit 1
}
if ! cmp -s "$tmp/api.d.ts" "$GENERATED"; then
	cp "$tmp/api.d.ts" "$GENERATED" || {
		echo "check-web-types.sh: cannot write $GENERATED" >&2
		exit 1
	}
fi

# Read back rather than assumed: a generator that printed a header and stopped
# would leave a file that declares nothing and refuses nothing.
grep -q '^	interface Ops {$' "$GENERATED" || {
	echo "check-web-types.sh: $GENERATED names no routes, so nothing would be held to the document" >&2
	exit 1
}

# What the checker read, counted before what it said is believed.
( cd "$WEB" && node "$TSC" -p jsconfig.json --listFiles --noEmit ) > "$tmp/out" 2>&1 || true
read_files=`grep -c "/data/ni-web/app/" "$tmp/out" || true`
[ "$read_files" -ge "$FLOOR" ] || {
	echo "check-web-types.sh: the checker read $read_files modules of the page, and there are more than $FLOOR" >&2
	echo "  Something is wrong with the project rather than with the page:" >&2
	grep -E "error TS[0-9]+" "$tmp/out" | head -20 >&2
	exit 1
}

faults=`grep -cE "error TS[0-9]+" "$tmp/out" || true`
if [ "$faults" -ne 0 ]; then
	echo "check-web-types.sh: $faults type errors in the pages" >&2
	echo "" >&2
	grep -E "^[^ ].*error TS[0-9]+|^ " "$tmp/out" >&2
	echo "" >&2
	echo "check-web-types.sh: data/ni-web/jsconfig.json has the rules and why each is on." >&2
	exit 1
fi

routes=`grep -c "^		'" "$GENERATED" || true`
echo "check-web-types.sh: $read_files modules of the page type check against $routes routes of the document"

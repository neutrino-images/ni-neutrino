#!/bin/sh
# What check-web-types.sh needs and this tree does not carry: the declaration
# files of the three modules the pages import, and the type checker itself.
#
# WHY IT IS HERE AND NOT ONLY IN A DEVELOPMENT ENVIRONMENT. The check is part
# of `make check`, so a clone of this repository has to be able to run it.
# Until this script existed it could not: the check pointed at a script in a
# separate repository, and whoever had only this one was told to fetch
# something they did not have.
#
# THE VERSIONS ARE NAMED A THIRD TIME HERE, and that is worth saying out loud.
# The first is ni-buildsystem (package/preact/preact.mk and the two beside
# it), which is what a box gets. The second is a development environment's own
# fetch, which fills a docroot. This one fills test/web/node_modules, which no
# browser and no box ever sees.
#
# They have to agree, because a declaration is only worth anything if it
# describes the bytes the page actually loads. Nothing enforces that from
# here: this repository cannot see the buildsystem. What it buys instead is
# that a drift shows up as a type error rather than as a check nobody could
# run.
#
# Nothing is compiled and nothing is installed. The files land under
# test/web/node_modules, which is outside anything a box receives: everything
# under data/ni-web/app is installed by walking it, so a declaration file left
# there would be shipped as part of the page and weighed against its budget.
set -e
LC_ALL=C
export LC_ALL

PREACT_VERSION=10.24.3
HTM_VERSION=3.1.1
PREACT_ROUTER_VERSION=4.1.2

# Plain JavaScript and no binary per platform, so this runs under any node.
# Pinned rather than taken from the machine, because a check whose strictness
# depends on whoever last ran npm is not a check.
TYPESCRIPT_VERSION=5.9.3

REGISTRY="${NI_WEB_REGISTRY:-https://registry.npmjs.org}"
HERE=$(cd "$(dirname "$0")" && pwd)
DEST="$HERE/node_modules"

command -v tar >/dev/null 2>&1 || { echo "fetch-types.sh: tar is not on the path" >&2; exit 1; }
if command -v curl >/dev/null 2>&1; then
	get() { curl -sSLf -o "$2" "$1"; }
elif command -v wget >/dev/null 2>&1; then
	get() { wget -q -O "$2" "$1"; }
else
	echo "fetch-types.sh: neither curl nor wget is on the path" >&2
	exit 1
fi

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# An npm archive always opens into a directory called package, whatever the
# package is called, so each one is unpacked on its own and then moved.
grab() {
	name="$1"
	version="$2"
	get "$REGISTRY/$name/-/$name-$version.tgz" "$WORK/$name.tgz"
	mkdir -p "$WORK/$name"
	tar xzf "$WORK/$name.tgz" -C "$WORK/$name"
	rm -rf "$DEST/$name"
	mkdir -p "$DEST"
	mv "$WORK/$name/package" "$DEST/$name"
}

mkdir -p "$DEST"
grab preact "$PREACT_VERSION"
grab htm "$HTM_VERSION"
grab preact-router "$PREACT_ROUTER_VERSION"
grab typescript "$TYPESCRIPT_VERSION"

# The four files jsconfig.json and the check name. Said here rather than left
# to the unpacking, so an archive that moved one of them fails now with the
# name of the file instead of later with a type error nobody can place.
for f in preact/src/index.d.ts \
	 preact/hooks/src/index.d.ts \
	 htm/dist/htm.d.ts \
	 preact-router/index.d.ts \
	 typescript/lib/tsc.js; do
	[ -r "$DEST/$f" ] || {
		echo "fetch-types.sh: $f is not in the archive it should be in" >&2
		exit 1
	}
done

echo "fetch-types.sh: preact $PREACT_VERSION, htm $HTM_VERSION, preact-router $PREACT_ROUTER_VERSION and typescript $TYPESCRIPT_VERSION are under test/web/node_modules"

# The first of the three nets the page is held by, and the only one that runs
# here: each of these reads the source of the page against the source of the
# server and against the document the suite builds anyway. Named in one list, and
# by one stream, so a stream adding one of them adds a file and edits nothing
# anybody else is editing.
#
# Each is called with the top of the source tree and the directory this build
# writes into, in that order. The second is where openapi.json is.
WEB_CHECKS = \
	check-web-install.sh \
	check-web-prefixes.sh \
	check-web-paths.sh \
	check-web-imports.sh \
	check-web-text.sh \
	check-web-css-scope.sh \
	check-web-drag.sh \
	check-web-store.sh \
	check-web-player.sh \
	check-web-decide.sh \
	check-web-timerbody.sh \
	check-web-fileplay.sh \
	check-web-size.sh \
	check-web-types.sh \
	check-web-handover.sh

# openapi-types.js is read by check-web-types.sh and writes the types the
# pages are held to; types/ holds what the type checker reads beside them.
EXTRA_DIST = $(WEB_CHECKS) fetch-types.sh serve.js openapi-types.js drag-cases.mjs player-cases.mjs \
	     decide-cases.mjs fileplay-cases.mjs store-cases.mjs timerbody-cases.mjs types

# The document the server answers with, written out by the target that owns it
# and read here so that what the page asks for is held to it. Made through that
# target rather than copied from wherever it was last left, because a document
# built before the change under test would be compared against nothing in
# particular. This is the one thing here that is not simply read off the tree,
# and it is a copy rather than a compile wherever the server has been built
# once.
BUILT_SOURCES = openapi.json
CLEANFILES = openapi.json

openapi.json: FORCE
	@$(MAKE) $(AM_MAKEFLAGS) -C $(top_builddir)/src/httpd openapi.json >/dev/null
	@cp -f $(top_builddir)/src/httpd/openapi.json $@.tmp && mv -f $@.tmp $@ || { rm -f $@.tmp $@; exit 1; }

FORCE:
.PHONY: FORCE

# Every script the list above names. A name without a file is a failure like
# any other check, and a script of this family that the list does not name is
# one too, because nothing would run it.
check-local:
	@for c in $(WEB_CHECKS); do \
		if test -f $(srcdir)/$$c; then \
			$(SHELL) $(srcdir)/$$c $(top_srcdir) $(abs_builddir) || exit 1; \
		else \
			echo "$$c is named in WEB_CHECKS and is not in $(srcdir)" >&2; \
			exit 1; \
		fi; \
	done
	@orphan=`cd $(srcdir) && for f in check-web-*.sh; do \
		test -f "$$f" || continue; \
		case " $(WEB_CHECKS) " in *" $$f "*) ;; *) echo $$f;; esac; done`; \
	test -z "$$orphan" || { echo "in web/ and in no WEB_CHECKS: $$orphan" >&2; exit 1; }

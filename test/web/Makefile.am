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
	check-web-events.sh \
	check-web-session.sh \
	check-web-wake.sh \
	check-web-player.sh \
	check-web-decide.sh \
	check-web-timerbody.sh \
	check-web-timeredit.sh \
	check-web-fileplay.sh \
	check-web-archive.sh \
	check-web-types.sh \
	check-web-handover.sh \
	check-web-guideclock.sh \
	check-web-nav.sh \
	check-web-mcp.sh \
	check-web-grid.sh \
	check-web-state.sh \
	$(AI_GUIDE_CHECKS) \
	$(AI_UI_CHECKS) \
	$(CHANNEL_CHECKS) \
	$(KEEP_CHECKS) \
	$(NOW_CHECKS) \
	check-web-timerend.sh

AI_GUIDE_CHECKS = \
	check-web-aiguides.sh

AI_UI_CHECKS = \
	check-web-ai.sh

AI_GUIDE_DIST = \
	aiguides-cases.mjs

AI_UI_DIST = \
	ai-cases.mjs

CHANNEL_CHECKS = \
	check-web-channels.sh

CHANNEL_DIST = \
	channels-cases.mjs

KEEP_CHECKS = \
	check-web-keep.sh

KEEP_DIST = \
	keep-cases.mjs

NOW_CHECKS = \
	check-web-now.sh

NOW_DIST = \
	now-cases.mjs

# openapi-types.js is read by check-web-types.sh and writes the types the
# pages are held to; types/ holds what the type checker reads beside them.
EXTRA_DIST = $(WEB_CHECKS) fetch-types.sh serve.js openapi-types.js drag-cases.mjs player-cases.mjs \
	     decide-cases.mjs fileplay-cases.mjs archive-cases.mjs store-cases.mjs events-cases.mjs session-cases.mjs timerbody-cases.mjs \
	     timeredit-cases.mjs guideclock-cases.mjs wake-cases.mjs nav-cases.mjs types \
	     $(AI_GUIDE_DIST) $(AI_UI_DIST) $(CHANNEL_DIST) timerend-cases.mjs $(KEEP_DIST) $(NOW_DIST) state-cases.mjs

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
.PHONY: FORCE web-orphans

# Whether the document carries the routes the modules under data/ni-web/ai call.
if ENABLE_MCP
WEB_MCP = yes
else
WEB_MCP = no
endif

# Every script the list above names, one target each, so make -j runs them side
# by side; none writes anything another reads. A name without a file is a
# failure like any other check (make finds no rule for it), and a script of this
# family that the list does not name is one too, because nothing would run it.
# A .run is never written, and FORCE runs a script even beside a stray one.
SUFFIXES = .sh .run
WEB_RUNS = $(WEB_CHECKS:.sh=.run)
$(WEB_RUNS): FORCE

check-local: $(WEB_RUNS) web-orphans

.sh.run:
	@NI_WEB_MCP=$(WEB_MCP) $(SHELL) $< $(top_srcdir) $(abs_builddir)

web-orphans:
	@orphan=`cd $(srcdir) && for f in check-web-*.sh; do \
		test -f "$$f" || continue; \
		case " $(WEB_CHECKS) " in *" $$f "*) ;; *) echo $$f;; esac; done`; \
	test -z "$$orphan" || { echo "in web/ and in no WEB_CHECKS: $$orphan" >&2; exit 1; }

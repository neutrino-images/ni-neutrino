installdir = $(PRIVATE_HTTPDDIR)/ni-web/app

include $(srcdir)/../exclude.am

# Everything under this directory, found rather than listed: a list here would be
# the one file every stream has to edit. Stored compressed and not beside a plain
# copy, because this server inflates the stored form for a caller that took none
# (src/httpd/static.cpp).
#
# The walk starts in $(srcdir), so a build outside the source tree collects the
# same files, and it passes over the names listed in ../exclude.am, so neither
# the Makefiles of a build inside it nor what a botched patch leaves behind is
# installed.
install-data-local:
	@cd $(srcdir) && set -f && ex= && for p in $(exclude_files); do ex="$$ex ! -name $$p"; done && \
	find . -type f $$ex -print \
	| sed 's|^\./||' | while read f; do \
		$(MKDIR_P) "$(DESTDIR)$(installdir)/`dirname $$f`" || exit 1; \
		gzip -9 -n -c "$$f" > "$(DESTDIR)$(installdir)/$$f.gz" || exit 1; \
	done

# Walked here too rather than listed in EXTRA_DIST, for the same reason and
# portably: EXTRA_DIST with a $(shell) is read by make and not by automake, so
# what ends up in the archive would depend on which make ran it.
# The archive directory is named absolutely before the walk changes directory,
# because automake hands it here relative to this build directory.
dist-hook:
	@to=`cd $(distdir) && pwd` && cd $(srcdir) && set -f && ex= && for p in $(exclude_files); do ex="$$ex ! -name $$p"; done && \
	find . -type f $$ex -print \
	| sed 's|^\./||' | while read f; do \
		$(MKDIR_P) "$$to/`dirname $$f`" || exit 1; \
		cp "$$f" "$$to/$$f" || exit 1; \
	done

uninstall-local:
	@rm -rf $(DESTDIR)$(installdir)

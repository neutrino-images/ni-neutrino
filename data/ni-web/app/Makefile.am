installdir = $(PRIVATE_HTTPDDIR)/ni-web/app

# Everything under this directory, found rather than listed: a list here would be
# the one file every stream has to edit. Stored compressed and not beside a plain
# copy, because this server inflates the stored form for a caller that took none
# (src/httpd/static.cpp).
#
# The walk starts in $(srcdir), so a build outside the source tree collects the
# same files, and it passes over every name beginning with Makefile, so a build
# inside it does not install the ones make and automake leave lying about.
install-data-local:
	@cd $(srcdir) && find . -type f ! -name 'Makefile*' -print \
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
	@to=`cd $(distdir) && pwd` && cd $(srcdir) && find . -type f ! -name 'Makefile*' -print \
	| sed 's|^\./||' | while read f; do \
		$(MKDIR_P) "$$to/`dirname $$f`" || exit 1; \
		cp "$$f" "$$to/$$f" || exit 1; \
	done

uninstall-local:
	@rm -rf $(DESTDIR)$(installdir)

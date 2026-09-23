installdir = $(PRIVATE_HTTPDDIR)/ni-web/info

# Everything under this directory, found rather than listed, and stored
# compressed: the same rule the application next door installs itself with, for
# the same two reasons. A list here would be the one file a change to this page
# has to edit as well, and a plain copy beside the stored one would be flash
# nothing reads, this server inflating the stored form for a caller that took
# none (src/httpd/static.cpp).
#
# The document lands as index.html.gz and is answered for /info/, which is what
# the directory rule in static.cpp asks the directory for.
install-data-local:
	@cd $(srcdir) && find . -type f ! -name 'Makefile*' -print \
	| sed 's|^\./||' | while read f; do \
		$(MKDIR_P) "$(DESTDIR)$(installdir)/`dirname $$f`" || exit 1; \
		gzip -9 -n -c "$$f" > "$(DESTDIR)$(installdir)/$$f.gz" || exit 1; \
	done

# Walked here too rather than listed in EXTRA_DIST, for the same reason and
# portably: EXTRA_DIST with a $(shell) is read by make and not by automake, so
# what ends up in the archive would depend on which make ran it.
dist-hook:
	@to=`cd $(distdir) && pwd` && cd $(srcdir) && find . -type f ! -name 'Makefile*' -print \
	| sed 's|^\./||' | while read f; do \
		$(MKDIR_P) "$$to/`dirname $$f`" || exit 1; \
		cp "$$f" "$$to/$$f" || exit 1; \
	done

uninstall-local:
	@rm -rf $(DESTDIR)$(installdir)

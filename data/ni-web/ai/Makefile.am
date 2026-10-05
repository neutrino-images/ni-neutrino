installdir = $(PRIVATE_HTTPDDIR)/ni-web/ai

install-data-local:
	@cd $(srcdir) && find . -type f ! -name 'Makefile*' -print \
	| sed 's|^\./||' | while read f; do \
		$(MKDIR_P) "$(DESTDIR)$(installdir)/`dirname $$f`" || exit 1; \
		gzip -9 -n -c "$$f" > "$(DESTDIR)$(installdir)/$$f.gz" || exit 1; \
	done

dist-hook:
	@to=`cd $(distdir) && pwd` && cd $(srcdir) && find . -type f ! -name 'Makefile*' -print \
	| sed 's|^\./||' | while read f; do \
		$(MKDIR_P) "$$to/`dirname $$f`" || exit 1; \
		cp "$$f" "$$to/$$f" || exit 1; \
	done

uninstall-local:
	@rm -rf $(DESTDIR)$(installdir)

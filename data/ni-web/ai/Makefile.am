installdir = $(PRIVATE_HTTPDDIR)/ni-web/ai

include $(srcdir)/../exclude.am

install-data-local:
	@cd $(srcdir) && set -f && ex= && for p in $(exclude_files); do ex="$$ex ! -name $$p"; done && \
	find . -type f $$ex -print \
	| sed 's|^\./||' | while read f; do \
		$(MKDIR_P) "$(DESTDIR)$(installdir)/`dirname $$f`" || exit 1; \
		gzip -9 -n -c "$$f" > "$(DESTDIR)$(installdir)/$$f.gz" || exit 1; \
	done

dist-hook:
	@to=`cd $(distdir) && pwd` && cd $(srcdir) && set -f && ex= && for p in $(exclude_files); do ex="$$ex ! -name $$p"; done && \
	find . -type f $$ex -print \
	| sed 's|^\./||' | while read f; do \
		$(MKDIR_P) "$$to/`dirname $$f`" || exit 1; \
		cp "$$f" "$$to/$$f" || exit 1; \
	done

uninstall-local:
	@rm -rf $(DESTDIR)$(installdir)

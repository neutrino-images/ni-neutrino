installdir = $(PRIVATE_HTTPDDIR)/ni-web/swagger

# The Swagger UI page this server's own default docroot points at (see
# src/httpd/webconfig.cpp), reached as /swagger/ and not at the root: the
# root is where the WebIf this docroot is also meant for lands later, and a
# docs page sitting there would have to move out of its own way the day it
# does. The bundle and stylesheet this page loads from ./vendor are a
# separate package, not part of this tree.
install_DATA = index.html

# Compressed in place and not installed twice: this server can inflate the
# stored form for a caller that did not ask for it (src/httpd/static.cpp), so
# a plain copy beside it would only be flash nothing ever reads.
install-data-hook:
	cd $(DESTDIR)$(installdir); \
		gzip -9 -nf index.html

uninstall-hook:
	rm -f $(DESTDIR)$(installdir)/index.html.gz

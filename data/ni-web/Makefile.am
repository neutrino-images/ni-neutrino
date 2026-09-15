SUBDIRS = \
	app \
	info

# The handset photographs, under a switch of their own: the screen draws the
# keys where there is no picture for the model, so a box short of flash can
# have the remote control without the likeness of it.
if ENABLE_HANDSET_PICTURES
SUBDIRS += \
	rc
endif

# The reader for the API document, under the same switch as the document's own
# prose: a build told to leave the descriptions out has nothing for this page to
# show, and installing it regardless spent four hundred kilobytes on saying so.
if ENABLE_API_DOC
SUBDIRS += \
	swagger
endif

installdir = $(PRIVATE_HTTPDDIR)/ni-web

# The document the page is: the history fallback answers a path under an area
# of the page with it (src/httpd/apppaths.cpp), so it is asked for under names
# that are not its own and every address inside it has to begin at the root.
#
# LICENSE is not installed. It says what the files under app/ are under and why
# none of them carries a notice of its own, which is a thing to read where the
# files are read: in the tree. The box already carries the notice this project
# puts on a box, under neutrino/license, and a second copy of the full text
# would be seven kilobytes of flash nobody opens.
install_DATA = index.html

# Compressed in place and not installed twice, the same way the docs page
# beside it is: this server can inflate the stored form for a caller that did
# not ask for it (src/httpd/static.cpp), so a plain copy beside it would only
# be flash nothing ever reads.
install-data-hook:
	cd $(DESTDIR)$(installdir); \
		gzip -9 -nf index.html

uninstall-hook:
	rm -f $(DESTDIR)$(installdir)/index.html.gz

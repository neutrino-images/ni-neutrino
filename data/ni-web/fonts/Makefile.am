# The face of the application and the info display (ubuntu.css).
#
# Cut from the Ubuntu font in google/fonts (ufl/ubuntu, version 0.83) with
# fontTools: Google's latin and latin-ext ranges, so a Polish or Czech channel
# name is not drawn in two faces; features tnum, pnum and case kept; hinting
# dropped (82 kB for the three instead of 185 kB); woff2; names rewritten to
# "Ubuntu derivative NI-Web" as the licence asks of a Modified Version.
#
# The woff2 files are not stored compressed, like the handset pictures: they
# are compressed streams already. The licence is installed, unlike the one
# beside index.html, because the Ubuntu Font Licence wants it with every copy
# of the font (condition 1).
installdir = $(PRIVATE_HTTPDDIR)/ni-web/fonts

dist_install_DATA = \
	ubuntu.css \
	ubuntu-400.woff2 \
	ubuntu-500.woff2 \
	ubuntu-700.woff2 \
	LICENCE.txt

install-data-hook:
	cd $(DESTDIR)$(installdir); \
		gzip -9 -nf ubuntu.css LICENCE.txt

uninstall-hook:
	rm -f $(DESTDIR)$(installdir)/ubuntu.css.gz \
		$(DESTDIR)$(installdir)/LICENCE.txt.gz

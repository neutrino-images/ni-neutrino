installdir = $(FONTDIR)

# NOTE: pakenham.ttf is not used by neutrino.
#       It's the default- respectively fallback-font for some classic plugins.
#
#       tuxtxt.otb is the non-ttf font for tuxtxt (UseTTF 0 in tuxtxt2.conf).

neutrino_ttf = Roboto-Regular.ttf
tuxtxt_ttf = DejaVuLGCSansMono-Bold.ttf

install_DATA = \
	$(neutrino_ttf) \
	$(tuxtxt_ttf) \
	pakenham.ttf \
	ubuntu-l-webfont.ttf \
	ubuntu-r-webfont.ttf \
	ubuntumono-b-webfont.ttf \
	ubuntumono-r-webfont.ttf \
	tuxtxt.otb

#NI
install-data-hook:
	cd $(DESTDIR)$(FONTDIR); \
		mv $(neutrino_ttf) neutrino.ttf; \
		mv $(tuxtxt_ttf) tuxtxt.ttf; \
		ln -sf tuxtxt.ttf shell.ttf
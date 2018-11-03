installdir = $(FONTDIR)

# NOTE: pakenham.ttf is not used by neutrino.
#       It's the default- respectively fallback-font for some classic plugins.
#
#       tuxtxt.otb is the non-ttf font for tuxtxt (UseTTF 0 in tuxtxt2.conf).

neutrino_ttf = Roboto-Regular.ttf
tuxtxt_ttf = DejaVuLGCSansMono-Bold.ttf

# default fonts
install_DATA = \
	$(neutrino_ttf) \
	$(tuxtxt_ttf) \
	pakenham.ttf \
	tuxtxt.otb

# additional fonts
install_DATA += \
	ubuntu-l-webfont.ttf \
	ubuntu-r-webfont.ttf \
	ubuntumono-b-webfont.ttf \
	ubuntumono-r-webfont.ttf

install-data-hook:
	cd $(DESTDIR)$(FONTDIR); \
		mv $(neutrino_ttf) neutrino.ttf; \
		mv $(tuxtxt_ttf) tuxtxt.ttf; \
		ln -sf tuxtxt.ttf shell.ttf

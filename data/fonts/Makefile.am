installdir = $(FONTDIR)

install_DATA = \
	SFFourcheCondensedItalic.ttf \
	ubuntu-l-webfont.ttf \
	ubuntu-r-webfont.ttf \
	ubuntumono-b-webfont.ttf \
	ubuntumono-r-webfont.ttf \
	Roboto-Regular.ttf \
	micron_bold.ttf \
	micron.ttf \
	pakenham.ttf \
	DejaVuLGCSansMono-Bold.ttf \
	tuxtxt.otb

if BOXMODEL_CS_HD2
#install_DATA += UnDotum.ttf
endif

#NI
install-data-hook:
	cd $(DESTDIR)$(FONTDIR); \
		mv Roboto-Regular.ttf neutrino.ttf; \
		ln -sf neutrino.ttf tuxtxt.ttf

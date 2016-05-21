installdir = $(FONTDIR)

install_DATA = \
	md_khmurabi_10.ttf \
	micron_bold.ttf \
	micron.ttf \
	pakenham.ttf \
	DejaVuLGCSansMono-Bold.ttf \
	tuxtxt.otb

if BOXMODEL_APOLLO
#install_DATA += UnDotum.ttf
endif

#NI
install-data-hook:
	cd $(DESTDIR)$(FONTDIR); \
		mv md_khmurabi_10.ttf neutrino.ttf; \
		ln -sf neutrino.ttf tuxtxt.ttf

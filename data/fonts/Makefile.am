installdir = $(FONTDIR)

install_DATA = \
	micron_bold.ttf \
	micron.ttf \
	neutrino.ttf \
	pakenham.ttf \
	DejaVuLGCSansMono-Bold.ttf \
	tuxtxt.otb

if BOXMODEL_APOLLO
#install_DATA += UnDotum.ttf
endif

#NI
install-data-hook:
	cd $(DESTDIR)$(FONTDIR) && ln -sf neutrino.ttf tuxtxt.ttf

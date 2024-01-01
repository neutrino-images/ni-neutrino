installdir = $(LOGODIR)

install_DATA =

if ENABLE_LCD4LINUX
install_DATA += \
	Moviebrowser.png \
	Movieplayer.png
endif

# ensure LOGODIR
install-data-hook:
	$(INSTALL) -d $(DESTDIR)/$(LOGODIR)

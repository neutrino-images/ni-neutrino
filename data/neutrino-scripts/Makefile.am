installdir = $(CONFIGDIR)

install_DATA = \
	flash.start \
	migration.sh \
	movieplayer.start \
	movieplayer.end \
	recording.start \
	recording.end \
	standby.on \
	standby.off

install-data-hook:
	chmod 0755 $(DESTDIR)$(CONFIGDIR)/flash.start
	chmod 0755 $(DESTDIR)$(CONFIGDIR)/migration.sh
	chmod 0755 $(DESTDIR)$(CONFIGDIR)/movieplayer.start
	chmod 0755 $(DESTDIR)$(CONFIGDIR)/movieplayer.end
	chmod 0755 $(DESTDIR)$(CONFIGDIR)/recording.start
	chmod 0755 $(DESTDIR)$(CONFIGDIR)/recording.end
	chmod 0755 $(DESTDIR)$(CONFIGDIR)/standby.on
	chmod 0755 $(DESTDIR)$(CONFIGDIR)/standby.off

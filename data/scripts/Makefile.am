installdir = $(bindir)

install_DATA = \
	backup.sh \
	restore.sh \
	wakeup.sh \
	install.sh 

if BOXMODEL_CS_HD2
install_DATA += \
	backup_flash.sh \
	restore_flash.sh
endif

install-data-hook:
	chmod 0755 $(DESTDIR)$(bindir)/backup.sh
	chmod 0755 $(DESTDIR)$(bindir)/restore.sh
	chmod 0755 $(DESTDIR)$(bindir)/wakeup.sh
	chmod 0755 $(DESTDIR)$(bindir)/install.sh
if BOXMODEL_CS_HD2
	chmod 0755 $(DESTDIR)$(bindir)/backup_flash.sh
	chmod 0755 $(DESTDIR)$(bindir)/restore_flash.sh
endif

EXTRA_DIST = \
	backup.sh backup.sh.in \
	restore.sh restore.sh.in \
	backup_flash.sh backup_flash.sh.in \
	restore_flash.sh restore_flash.sh.in \
	migration.sh migration.sh.in

installdir = $(bindir)

install_SCRIPTS = \
	backup.sh \
	restore.sh \
	migration.sh \
	wakeup.sh \
	install.sh 

install_SCRIPTS += \
	start_flash.sh \

if BOXMODEL_CST_HD2
install_SCRIPTS += \
	backup_flash.sh \
	restore_flash.sh
endif

if BOXTYPE_ARMBOX
install_SCRIPTS += \
	restore_flash.sh
endif

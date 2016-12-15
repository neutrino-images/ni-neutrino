#!/bin/sh

. %(PRIVATE_HTTPDDIR)/scripts/_Y_Globals.sh
. %(PRIVATE_HTTPDDIR)/scripts/_Y_Library.sh

BNAME=${0##*/}

case "$1" in
	start)
		echo "[$BNAME] softcamd-start"
		/etc/init.d/emu reset >/dev/console
	;;
	stop)
		echo "[$BNAME] softcamd-stop" 
		/etc/init.d/emu stop >/dev/console
	;;
	reset)
		echo "[$BNAME] softcamd-reset" 
		/etc/init.d/emu reset >/dev/console
	;;
	mgcamd_start)
		echo "[$BNAME] mgcamd-start" 
		touch /var/etc/.mgcamd
		/etc/init.d/emu start mgcamd >/dev/console
	;;
	mgcamd_stop)
		echo "[$BNAME] mgcamd-stop" 
		/etc/init.d/emu stop mgcamd >/dev/console
		rm -rf /var/etc/.mgcamd
	;;
	mgcamd_reset)
		echo "[$BNAME] mgcamd-reset"
		/etc/init.d/emu reset mgcamd >/dev/console
	;;
	gbox_start)
		echo "[$BNAME] gbox-start"
		touch /var/etc/.gbox
		/etc/init.d/emu start gbox >/dev/console
	;;
	gbox_stop)
		echo "[$BNAME] gbox-stop"
		rm -rf /var/etc/.gbox
		/etc/init.d/emu stop gbox >/dev/console
	;;
	gbox_reset)
		echo "[$BNAME] gbox-reset"
		/etc/init.d/emu reset gbox >/dev/console
	;;
	cs2gbox_start)
		echo "[$BNAME] cs2gbox-start"
		touch /var/etc/.cs2gbox
		/etc/init.d/emu start cs2gbox >/dev/console
	;;
	cs2gbox_stop)
		echo "[$BNAME] cs2gbox-stop"
		rm -rf /var/etc/.cs2gbox
		/etc/init.d/emu stop cs2gbox >/dev/console
	;;
	cs2gbox_reset)
		echo "[$BNAME] cs2gbox-reset"
		/etc/init.d/emu reset cs2gbox >/dev/console
	;;
	osemu_start)
		echo "[$BNAME] osemu-start"
		touch /var/etc/.osemu
		/etc/init.d/emu start osemu >/dev/console
	;;
	osemu_stop)
		echo "[$BNAME] osemu-stop"
		rm -rf /var/etc/.osemu
		/etc/init.d/emu stop osemu >/dev/console
	;;
	osemu_reset)
		echo "[$BNAME] osemu-reset"
		/etc/init.d/emu reset osemu >/dev/console
	;;
	oscam_start)
		echo "[$BNAME] oscam-start"
		touch /var/etc/.oscam
		/etc/init.d/emu start oscam >/dev/console
	;;
	oscam_stop)
		echo "[$BNAME] oscam-stop"
		rm -rf /var/etc/.oscam
		/etc/init.d/emu stop oscam >/dev/console
	;;
	oscam_reset)
		echo "[$BNAME] oscam-reset"
		/etc/init.d/emu reset oscam >/dev/console
	;;
	doscam_start)
		echo "[$BNAME] doscam-start"
		touch /var/etc/.doscam
		/etc/init.d/emu start doscam >/dev/console
	;;
	doscam_stop)
		echo "[$BNAME] doscam-stop"
		rm -rf /var/etc/.doscam
		/etc/init.d/emu stop doscam >/dev/console
	;;
	doscam_reset)
		echo "[$BNAME] doscam-reset"
		/etc/init.d/emu reset doscam >/dev/console
	;;
	newcs_start)
		echo "[$BNAME] newcs-start"
		touch /var/etc/.newcs
		/etc/init.d/emu start newcs >/dev/console
	;;
	newcs_stop)
		echo "[$BNAME] newcs-stop"
		rm -rf /var/etc/.newcs
		/etc/init.d/emu stop newcs >/dev/console
	;;
	newcs_reset)
		echo "[$BNAME] newcs-reset"
		/etc/init.d/emu reset newcs >/dev/console
	;;
	*)
		echo "[$BNAME] Parameter wrong: $*"
	;;
esac

exit 0

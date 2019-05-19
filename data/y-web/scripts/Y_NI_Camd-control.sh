#!/bin/sh

. %(PRIVATE_HTTPDDIR)/scripts/_Y_Globals.sh
. %(PRIVATE_HTTPDDIR)/scripts/_Y_Library.sh

BNAME=${0##*/}

case "$1" in
	start)
		echo "[$BNAME] softcamd-start"
		/etc/init.d/camd reset >/dev/console
	;;
	stop)
		echo "[$BNAME] softcamd-stop" 
		/etc/init.d/camd stop >/dev/console
	;;
	reset)
		echo "[$BNAME] softcamd-reset" 
		/etc/init.d/camd reset >/dev/console
	;;
	mgcamd_start)
		echo "[$BNAME] mgcamd-start" 
		touch /var/etc/.mgcamd
		/etc/init.d/camd start mgcamd >/dev/console
	;;
	mgcamd_stop)
		echo "[$BNAME] mgcamd-stop" 
		/etc/init.d/camd stop mgcamd >/dev/console
		rm -rf /var/etc/.mgcamd
	;;
	mgcamd_reset)
		echo "[$BNAME] mgcamd-reset"
		/etc/init.d/camd reset mgcamd >/dev/console
	;;
	gbox_start)
		echo "[$BNAME] gbox-start"
		touch /var/etc/.gbox
		/etc/init.d/camd start gbox >/dev/console
	;;
	gbox_stop)
		echo "[$BNAME] gbox-stop"
		rm -rf /var/etc/.gbox
		/etc/init.d/camd stop gbox >/dev/console
	;;
	gbox_reset)
		echo "[$BNAME] gbox-reset"
		/etc/init.d/camd reset gbox >/dev/console
	;;
	cs2gbox_start)
		echo "[$BNAME] cs2gbox-start"
		touch /var/etc/.cs2gbox
		/etc/init.d/camd start cs2gbox >/dev/console
	;;
	cs2gbox_stop)
		echo "[$BNAME] cs2gbox-stop"
		rm -rf /var/etc/.cs2gbox
		/etc/init.d/camd stop cs2gbox >/dev/console
	;;
	cs2gbox_reset)
		echo "[$BNAME] cs2gbox-reset"
		/etc/init.d/camd reset cs2gbox >/dev/console
	;;
	oscam_start)
		echo "[$BNAME] oscam-start"
		touch /var/etc/.oscam
		/etc/init.d/camd start oscam >/dev/console
	;;
	oscam_stop)
		echo "[$BNAME] oscam-stop"
		rm -rf /var/etc/.oscam
		/etc/init.d/camd stop oscam >/dev/console
	;;
	oscam_reset)
		echo "[$BNAME] oscam-reset"
		/etc/init.d/camd reset oscam >/dev/console
	;;
	ncam_start)
		echo "[$BNAME] ncam-start"
		touch /var/etc/.ncam
		/etc/init.d/camd start ncam >/dev/console
	;;
	ncam_stop)
		echo "[$BNAME] ncam-stop"
		rm -rf /var/etc/.ncam
		/etc/init.d/camd stop ncam >/dev/console
	;;
	ncam_reset)
		echo "[$BNAME] ncam-reset"
		/etc/init.d/camd reset ncam >/dev/console
	;;
	doscam_start)
		echo "[$BNAME] doscam-start"
		touch /var/etc/.doscam
		/etc/init.d/camd start doscam >/dev/console
	;;
	doscam_stop)
		echo "[$BNAME] doscam-stop"
		rm -rf /var/etc/.doscam
		/etc/init.d/camd stop doscam >/dev/console
	;;
	doscam_reset)
		echo "[$BNAME] doscam-reset"
		/etc/init.d/camd reset doscam >/dev/console
	;;
	newcs_start)
		echo "[$BNAME] newcs-start"
		touch /var/etc/.newcs
		/etc/init.d/camd start newcs >/dev/console
	;;
	newcs_stop)
		echo "[$BNAME] newcs-stop"
		rm -rf /var/etc/.newcs
		/etc/init.d/camd stop newcs >/dev/console
	;;
	newcs_reset)
		echo "[$BNAME] newcs-reset"
		/etc/init.d/camd reset newcs >/dev/console
	;;
	*)
		echo "[$BNAME] Parameter wrong: $*"
	;;
esac

exit 0

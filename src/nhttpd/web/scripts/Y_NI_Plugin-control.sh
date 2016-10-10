#!/bin/sh

. %(PRIVATE_HTTPDDIR)/scripts/_Y_Globals.sh
. %(PRIVATE_HTTPDDIR)/scripts/_Y_Library.sh

BNAME=${0##*/}

case "$1" in
	start)
	;;
	stop)
	;;
	reset)
	;;
	fcm_start)
		echo "[$BNAME] fritzcallmonitor start"
		touch /var/etc/.fritzcallmonitor
		$y_path_sbin/service fritzcallmonitor start >/dev/console
	;;
	fcm_stop)
		echo "[$BNAME] fritzcallmonitor stop"
		$y_path_sbin/service fritzcallmonitor stop >/dev/console
		rm -f /var/etc/.fritzcallmonitor
	;;
	nfs_start)
		echo "[$BNAME] nfs-server start"
		touch /var/etc/.nfsd
		$y_path_sbin/service nfsd start >/dev/console
	;;
	nfs_stop)
		echo "[$BNAME] nfs-server stop"
		$y_path_sbin/service nfsd stop >/dev/console
		rm -f /var/etc/.nfsd
	;;
	smb_start)
		echo "[$BNAME] samba-server start"
		touch /var/etc/.samba
		$y_path_sbin/service samba start >/dev/console
	;;
	smb_stop)
		echo "[$BNAME] samba-server stop"
		$y_path_sbin/service samba stop >/dev/console
		rm -f /var/etc/.samba
	;;
	txc_start)
		echo "[$BNAME] tuxcal start"
		touch /var/etc/.tuxcald
		$y_path_sbin/service tuxcald start >/dev/console
	;;
	txc_stop)
		echo "[$BNAME] tuxcal stop"
		$y_path_sbin/service tuxcald stop >/dev/console
		rm -f /var/etc/.tuxcald
	;;
	txm_start)
		echo "[$BNAME] tuxmail start"
		touch /var/etc/.tuxmaild
		$y_path_sbin/service tuxmaild start >/dev/console
	;;
	txm_stop)
		echo "[$BNAME] tuxmail stop"
		$y_path_sbin/service tuxmaild stop >/dev/console
		rm -f /var/etc/.tuxmaild
	;;
	ina_start)
		echo "[$BNAME] inadyn start"
		touch /var/etc/.inadyn
		$y_path_sbin/service inadyn start >/dev/console
	;;
	ina_stop)
		echo "[$BNAME] inadyn stop"
		$y_path_sbin/service inadyn stop >/dev/console
		rm -f /var/etc/.inadyn
	;;
	drop_start)
		echo "[$BNAME] dropbear start"
		touch /var/etc/.dropbear
		$y_path_sbin/service dropbear start >/dev/console
	;;
	drop_stop)
		echo "[$BNAME] dropbear stop"
		$y_path_sbin/service dropbear stop >/dev/console
		rm -f /var/etc/.dropbear
	;;
	ush_start)
		echo "[$BNAME] ushare start"
		touch /var/etc/.ushare
		$y_path_sbin/service ushare start >/dev/console
	;;
	ush_stop)
		echo "[$BNAME] ushare stop"
		$y_path_sbin/service ushare stop >/dev/console
		rm -f /var/etc/.ushare
	;;
	djm_start)
		echo "[$BNAME] djmount start"
		touch /var/etc/.djmount
		$y_path_sbin/service djmount start >/dev/console
	;;
	djm_stop)
		echo "[$BNAME] djmount stop"
		$y_path_sbin/service djmount stop >/dev/console
		rm -f /var/etc/.djmount
	;;
	xud_start)
		echo "[$BNAME] xupnpd start"
		touch /var/etc/.xupnpd
		$y_path_sbin/service xupnpd start >/dev/console
	;;
	xud_stop)
		echo "[$BNAME] xupnpd stop"
		$y_path_sbin/service xupnpd stop >/dev/console
		rm -f /var/etc/.xupnpd
	;;
	cro_start)
		echo "[$BNAME] crond start"
		touch /var/etc/.crond
		$y_path_sbin/service crond start >/dev/console
	;;
	cro_stop)
		echo "[$BNAME] crond stop"
		$y_path_sbin/service crond stop >/dev/console
		rm -f /var/etc/.crond
	;;
	*)
		echo "[$BNAME] Parameter wrong: $*"
	;;
esac

exit 0

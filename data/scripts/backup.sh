#!/bin/sh

. /etc/init.d/globals

HOST=$(hostname)
DATE=$(date +%Y-%m-%d_%H-%M-%S)
USRF="/var/tuxbox/config/tobackup.conf"
BAKF="$1/${2:-settings_${HOST}_${DATE}.tar.gz}"

TOBACKUP="\
	/etc/auto.net \
	/etc/exports \
	/etc/fstab \
	/etc/hostname \
	/etc/network/interfaces \
	/etc/passwd \
	/etc/profile.local \
	/etc/resolv.conf \
	/etc/samba/smb.conf \
	/etc/wpa_supplicant.conf \
	/share/tuxbox/neutrino/flex/flex_user.conf \
	/var/bin/ \
	/var/etc/ \
	/var/keys/ \
	/var/log/ \
	/var/root/ \
	/var/tuxbox/config/auto-record-cleaner.* \
	/var/tuxbox/config/doscam.* \
	/var/tuxbox/config/FritzCallMonitor.addr \
	/var/tuxbox/config/FritzCallMonitor.cfg \
	/var/tuxbox/config/moviebrowser.conf \
	/var/tuxbox/config/neutrino.conf \
	/var/tuxbox/config/oscam.* \
	/var/tuxbox/config/oscammon.conf \
	/var/tuxbox/config/pr-auto-timer.* \
	/var/tuxbox/config/radio-favorites.xml \
	/var/tuxbox/config/scan.conf \
	/var/tuxbox/config/settingsupdate.conf \
	/var/tuxbox/config/smarthomeinfo.conf \
	/var/tuxbox/config/timerd.conf \
	/var/tuxbox/config/tuxcal/ \
	/var/tuxbox/config/tuxmail/ \
	/var/tuxbox/config/webradio_usr.xml \
	/var/tuxbox/config/webtv_usr.xml \
	/var/tuxbox/config/Y-Web.conf \
	/var/tuxbox/config/zapit/ \
	/var/tuxbox/fonts/ \
	/var/tuxbox/httpd/ \
	/var/tuxbox/icons/ \
	/var/tuxbox/locale/ \
	/var/tuxbox/plugins/ \
	/var/tuxbox/themes/ \
	/var/tuxbox/webradio/ \
	/var/tuxbox/webtv/ \
	/var/xupnpd/ \
"

SHOWINFO "backup to ${BAKF} ..."

if [ -e "${USRF}" ]; then
	TOBACKUP="$TOBACKUP ${USRF}"
	while read i
		do [ "${i:0:1}" = "#" ] || TOBACKUP="$TOBACKUP ${i%%#*}"
		done < $USRF
fi

# check existence
RES=""
for i in $TOBACKUP
	do [ -e "$i" ] && RES="$RES $i"
	done

TOBACKUP=$(echo $RES)

tar -czf "${BAKF}" $TOBACKUP 2>&1 >/dev/null

SHOWINFO "done."

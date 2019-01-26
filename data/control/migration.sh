#!/bin/sh

cd /var/tuxbox/config
if [ -e neutrino.conf ]; then
	# remove NG leftovers
	sed -i 's|ng_netfs_\(.*\)|netfs_\1|' neutrino.conf

	# webradio_usr.xml was moved
	sed -i "s|/var/tuxbox/config/webradio_usr.xml|/var/tuxbox/webradio/webradio_usr.xml|" neutrino.conf
	mkdir -p /var/tuxbox/webradio/
	if [ -e /var/tuxbox/config/webradio_usr.xml ]; then
		mv /var/tuxbox/config/webradio_usr.xml /var/tuxbox/webradio/
	fi

	# webtv_usr.xml was moved
	sed -i "s|/var/tuxbox/config/webtv_usr.xml|/var/tuxbox/webtv/webtv_usr.xml|" neutrino.conf
	mkdir -p /var/tuxbox/webtv/
	if [ -e /var/tuxbox/config/webtv_usr.xml ]; then
		mv /var/tuxbox/config/webtv_usr.xml /var/tuxbox/webtv/
	fi

	sort neutrino.conf > neutrino.sort
	mv neutrino.sort neutrino.conf
fi

controlscripts="\
	audioplayer.start \
	audioplayer.end \
	deepstandby.on \
	deepstandby.off \
	inactivity.on \
	movieplayer.start \
	movieplayer.end \
	neutrino.start \
	pictureviewer.start \
	pictureviewer.end \
	recording.timer \
	recording.start \
	recording.end \
	scan.start \
	scan.stop \
	standby.on \
	standby.off \
"

mkdir -p /var/tuxbox/control/
for controlscript in $controlscripts; do
	echo $controlscript
	if [ -e $controlscript ]; then
		mv $controlscript /var/tuxbox/control/
	fi
done

# these control scripts hasn't counterparts in /var
rm -f migration.sh
rm -f flash.start

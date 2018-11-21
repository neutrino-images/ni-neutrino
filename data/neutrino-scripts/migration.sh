#!/bin/sh

cd /var/tuxbox/config
if [ -e neutrino.conf ]; then
	# remove NG leftovers
	sed -i 's|ng_netfs_\(.*\)|netfs_\1|' neutrino.conf

	# webradio_usr.xml was moved
	sed -i "s|/var/tuxbox/config/webradio_usr.xml|/var/tuxbox/webradio/webradio_usr.xml|" neutrino.conf
	mkdir -p /var/tuxbox/webradio/
	if [ -e /var/tuxbox/config/webradio_usr.xml -a ! -e /var/tuxbox/webradio/webradio_usr.xml ]; then
		mv /var/tuxbox/config/webradio_usr.xml /var/tuxbox/webradio/
	fi

	# webtv_usr.xml was moved
	sed -i "s|/var/tuxbox/config/webtv_usr.xml|/var/tuxbox/webtv/webtv_usr.xml|" neutrino.conf
	mkdir -p /var/tuxbox/webtv/
	if [ -e /var/tuxbox/config/webtv_usr.xml -a ! -e /var/tuxbox/webtv/webtv_usr.xml ]; then
		mv /var/tuxbox/config/webtv_usr.xml /var/tuxbox/webtv/
	fi

	sort neutrino.conf > neutrino.sort
	mv neutrino.sort neutrino.conf
fi

#!/bin/sh

cd /var/tuxbox/config
if [ -e neutrino.conf ]; then
	sed -i 's|ng_netfs_\(.*\)|netfs_\1|' neutrino.conf

	sort neutrino.conf > neutrino.sort
	mv neutrino.sort neutrino.conf
fi

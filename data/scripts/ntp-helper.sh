#!/bin/sh

chronyc=$(which chronyc)
conf=/etc/chrony.conf
ntpserver=$1

if [ -z $chronyc -o -z $ntpserver ]; then
	exit 1
fi

pool=$(grep ^pool $conf | cut -d" " -f2)

if [ "$pool" == "$ntpserver" ]; then
	echo "${0##*/}: Synchronize time with $pool"
	$chronyc makestep
else
	echo "${0##*/}: Replacing $pool with $ntpserver in $conf"
	sed -i -e "s|^pool .*|pool $ntpserver iburst|" $conf
	echo "${0##*/}: Restarting chronyd"
	service chrony restart
fi

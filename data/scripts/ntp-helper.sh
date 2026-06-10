#!/bin/sh

chronyc=$(which chronyc)
ntpserver=$1

if [ -z $chronyc -o -z $ntpserver ]; then
	exit 1
fi

conf=/etc/chrony.conf
pool=$(grep ^pool $conf | cut -d" " -f2)

if [ "$pool" == "$ntpserver" ]; then
	printf "${0##*/}: Synchronize time with $pool ... "
	$chronyc makestep
else
	echo "${0##*/}: Replace pool $pool with $ntpserver in $conf"
	sed -i -e "s|^pool .*|pool $ntpserver iburst|" $conf
	echo "${0##*/}: Restart chrony"
	service chrony restart
fi

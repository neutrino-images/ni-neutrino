#!/bin/sh

export PATH=${PATH}:/var/bin:/var/plugins

# Neutrino's exit codes
ERROR=-1
NORMAL=0
SHUTDOWN=1
REBOOT=2

echo "Starting Neutrino"

cd /tmp
/bin/neutrino >/dev/null 2>&1; RET=$?
sync

echo "Neutrino exited with exit code $RET"

if [ $RET -eq $NORMAL ]; then
	# do nothing
elif [ $RET -eq $SHUTDOWN ]; then
	dt -t"Shutdown ..."
	poweroff
elif [ $RET -eq $REBOOT ]; then
	dt -t"Reboot ..."
	reboot
else
	dt -t"Panic ..."
	sleep 5
	reboot -f
fi

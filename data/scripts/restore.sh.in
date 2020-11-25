#!/bin/sh

. /etc/init.d/globals

BAKF="$1"
E=0

sync
sleep 2
killall start_neutrino;	E=$(($E+$?))
killall neutrino;	E=$(($E+$?))
sleep 3

SHOWINFO "restore settings from "${BAKF}""
cd / && tar -xzf "${BAKF}"
sync
SHOWINFO "done."

test $E -eq 0 && reboot || reboot -f

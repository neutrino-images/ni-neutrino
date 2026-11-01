#!/bin/sh
# Turns the --enable-python line of the neutrino package off or on, so that a
# build without the bindings can be checked.
set -e

MK=/home/tango/ni-buildsystem/package/neutrino/neutrino.mk

case "$1" in
off)
	sed -i 's|^NEUTRINO_CONF_OPTS += --enable-python$|#NEUTRINO_CONF_OPTS += --enable-python|' "$MK"
	;;
on)
	sed -i 's|^#NEUTRINO_CONF_OPTS += --enable-python$|NEUTRINO_CONF_OPTS += --enable-python|' "$MK"
	;;
*)
	echo "usage: $0 off|on"
	exit 2
	;;
esac

echo "--- the lines that matter ---"
grep -n -e enable-python -e 'NEUTRINO_DEPENDENCIES += python3' "$MK"

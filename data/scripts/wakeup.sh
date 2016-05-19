#!/bin/sh

. /etc/init.d/globals

test $1 || { SHOWINFO "error"; exit 1; }

directory="$(echo ${@%/})"
timestamp="$(date +%Y%m%d%H%M%S)"
flag="${directory}/.wakeUp_${timestamp}"

SHOWINFO "try to wake up \"${directory}\" ..."
touch "${flag}" 2>/dev/null && rm -f "${flag}"

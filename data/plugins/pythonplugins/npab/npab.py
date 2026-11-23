#!/usr/bin/pyclient

"""
	npab - Neutrino Python API bridge

	Copyright (C) 2026 Sven Hoefer <svenhoefer@svenhoefer.com>

	The Python counterpart of nlab.lua (data/plugins/luaplugins/nlab): called
	through pyclient, it hands values of the Neutrino Python API to the
	shell.

	Usage:	npab get <apiversion[.major|.minor]|dir.<name>|screen.<name>>
		npab misc <postmsg.standby_on|getvolume|setvolume <n>|ismuted|getrevision>
		npab video <showpicture <file>|stoppicture>
"""

import sys

from neutrino import APIVERSION, DIR, SCREEN, POSTMSG, misc, video

npab = "Neutrino Python API bridge"
version = "0.01"

# sys.argv[0] is the script, sys.argv[1] the name pyclient was called by,
# the arguments follow - the same layout nlab.lua finds in 'arg'.
arg = sys.argv
a = [v.lower() for v in arg]


def argument(i):
	return a[i] if len(a) > i else None


def get(key):
	# see src/gui/python/pyinstance.cpp

	if key == "apiversion":
		return "%d.%d" % (APIVERSION["MAJOR"], APIVERSION["MINOR"])
	if key == "apiversion.major":
		return APIVERSION["MAJOR"]
	if key == "apiversion.minor":
		return APIVERSION["MINOR"]

	for prefix, table in (("dir.", DIR), ("screen.", SCREEN)):
		if key.startswith(prefix):
			return table.get(key[len(prefix):].upper())

	return None


def do_misc(cmd):
	# see src/gui/python/py_misc.cpp

	m = misc.new()
	if cmd == "postmsg.standby_on":
		return m.postMsg(POSTMSG["STANDBY_ON"])
	if cmd == "getvolume":
		return m.getVolume()
	if cmd == "setvolume" and argument(4) is not None:
		try:
			return m.setVolume(int(argument(4)))
		except ValueError:
			return None
	if cmd == "ismuted":
		return m.isMuted()
	if cmd == "getrevision":
		revision, hardware = m.GetRevision()
		return revision

	return None


def do_video(cmd):
	# see src/gui/python/py_video.cpp

	v = video.new()
	if cmd == "showpicture" and argument(4) is not None:
		v.zapitStopPlayBack()
		return v.ShowPicture(arg[4])
	if cmd == "stoppicture":
		v.zapitStopPlayBack(False)
		return v.StopPicture()

	return None


r = None

if argument(2) is None:
	r = npab + " v" + version
elif argument(2) == "get" and argument(3) is not None:
	r = get(argument(3))
elif argument(2) == "misc" and argument(3) is not None:
	r = do_misc(argument(3))
elif argument(2) == "video" and argument(3) is not None:
	r = do_video(argument(3))

# What pyclient prints: the name 'result', where nlab.lua returns a string.
# Booleans are written the way Lua's tostring() writes them.
if isinstance(r, bool):
	r = "true" if r else "false"
if r is not None and r != "":
	result = str(r) + "\n"

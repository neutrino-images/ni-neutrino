#!/usr/bin/env python3
# A sample Python plugin: it says what the API answers, so that a box with
# the bindings in src/gui/python in it has something to try them with.
#
# A plugin is a .cfg next to a .py of the same name in one of the plugin
# directories, with "type=python" in it, the way a Lua plugin is a .cfg
# next to a .lua. This file is what a plugin of that kind looks like from
# the inside; drop it into a plugin directory with a .cfg to run it from
# the box's own menu.

from neutrino import APIVERSION, messagebox, misc

m = misc.new()
revision, hardware = m.GetRevision()

messagebox.exec(
    name="Python API",
    text="Version %d.%d on %s (revision %d)" % (
        APIVERSION["MAJOR"], APIVERSION["MINOR"], hardware, revision),
    buttons=["ok"],
    default="ok")

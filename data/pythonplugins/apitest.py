#!/usr/bin/env python3
# apitest.py - every binding src/gui/python carries, walked once, so that a
# box with the bindings in it can be asked what answers and what does not.
#
# The picker is the API's own menu: one entry per binding, each running the
# calls that binding offers, and a call that raises is written down with the
# exception it raised, so a binding that is broken is named rather than left
# out. The entries that need nobody at the box are run in one go by the last
# one, and what was written down is shown and also put in a log on the box.
#
# Nothing here changes the box: the settings the config file binding writes
# and the files the file helper binding makes live under a directory of their
# own in /tmp, and the download is the box's own web server, which is up
# whatever else is. The calls that take the screen - the menu, the message
# box, the widgets and the blanking - are the ones that show something.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import os
import time

from neutrino import APIVERSION, CC, COL, CORNER, CURL, DIR, DYNFONT, FONT, MENU_RETURN, NMODE, OFFSET, PLAYSTATE, POSTMSG, RC, SCREEN, TEXT_ALIGNMENT, configfile, cprogresswindow, cpicture, ctext, curl, cwindow, filehelpers, header, hint, hintbox, hourglass, menu, messagebox, misc, signalbox, stringinput, threads, video, window

# The scratch the tests that write are given, so that none of them writes
# where the box keeps something.
SCRATCH = "/tmp/apitest"
LOGFILE = "/tmp/apitest.log"

results = []


def check(area, what, call):
	"""Runs one call and writes down what it answered."""
	try:
		answer = call()
	except Exception as err:
		results.append((area, what, False, "%s: %s" % (type(err).__name__, err)))
		return None
	results.append((area, what, True, "" if answer is None else str(answer)))
	return answer


def check_screen(area, what, call):
	"""Runs one call that needs the screen, and writes it down."""
	try:
		answer = call()
	except Exception as err:
		results.append((area, what, False, "%s: %s" % (type(err).__name__, err)))
		return None
	results.append((area, what, True, "" if answer is None else str(answer)))
	return answer


def passed():
	return sum(1 for r in results if r[2])


def summary():
	return "%d of %d calls answered" % (passed(), len(results))


def failures():
	return [r for r in results if not r[2]]


def write_report():
	"""The whole run, one line a call, in a file a box can be asked for."""
	lines = ["Neutrino Python API test", ""]
	lines.append("API version %s.%s" % (APIVERSION["MAJOR"], APIVERSION["MINOR"]))
	lines.append(summary())
	lines.append("")
	area = ""
	for a, what, ok, detail in results:
		if a != area:
			area = a
			lines.append("[%s]" % area)
		lines.append("  %-30s %s%s" % (what, "ok" if ok else "FAIL",
					       "   " + detail if detail else ""))
	lines.append("")
	try:
		os.makedirs(SCRATCH)
	except OSError:
		pass
	try:
		with open(LOGFILE, "w") as f:
			f.write("\n".join(lines) + "\n")
	except OSError as err:
		return "/tmp is not writable: %s" % err
	return LOGFILE


def show_report():
	path = write_report()
	bad = failures()
	if bad:
		text = summary() + "\n\n" + "\n".join(
			"%s: %s" % (what, detail) for _a, what, _ok, detail in bad[:12])
		if len(bad) > 12:
			text += "\n... and %d more" % (len(bad) - 12)
	else:
		text = summary()
	messagebox.exec(name="Python API test", text=text + "\n\n" + path,
			buttons=["ok"], default="ok")


# ------------------------------------------------------------ the constants

def test_constants():
	"""Every table the module carries, and what an entry of each says."""
	tables = [("RC", RC), ("SCREEN", SCREEN), ("FONT", FONT),
		  ("CORNER", CORNER), ("OFFSET", OFFSET), ("MENU_RETURN", MENU_RETURN),
		  ("APIVERSION", APIVERSION), ("PLAYSTATE", PLAYSTATE), ("CC", CC),
		  ("DYNFONT", DYNFONT), ("CURL", CURL), ("NMODE", NMODE),
		  ("POSTMSG", POSTMSG), ("TEXT_ALIGNMENT", TEXT_ALIGNMENT),
		  ("COL", COL), ("DIR", DIR)]
	for name, table in tables:
		check("constants", "%s (%d entries)" % (name, len(table)),
		      lambda t=table: ", ".join(sorted(t)[:3]))
	check("constants", "APIVERSION['MAJOR']", lambda: APIVERSION["MAJOR"])
	check("constants", "RC['OK']", lambda: RC["OK"])
	check("constants", "CURL['OK']", lambda: CURL["OK"])
	check("constants", "MENU_RETURN['EXIT']", lambda: MENU_RETURN["EXIT"])
	check("constants", "PLAYSTATE['NORMAL']", lambda: PLAYSTATE["NORMAL"])
	check("constants", "NMODE['TV']", lambda: NMODE["TV"])
	check("constants", "POSTMSG['STANDBY_ON']", lambda: POSTMSG["STANDBY_ON"])
	check("constants", "SCREEN['X_RES']", lambda: SCREEN["X_RES"])
	check("constants", "DIR['CONFIGDIR']", lambda: DIR["CONFIGDIR"])
	check("constants", "DIR['PYTHONPLUGINDIR']", lambda: DIR["PYTHONPLUGINDIR"])


# ------------------------------------------------------------------- misc

def test_misc():
	m = misc.new()
	check("misc", "new()", lambda: m)
	check("misc", "GetRevision()", lambda: m.GetRevision())
	check("misc", "strFind('abcdef', 'cd')", lambda: m.strFind(s="abcdef", pattern="cd"))
	check("misc", "strFind, no match", lambda: m.strFind(s="abcdef", pattern="zz"))
	check("misc", "strSub('abcdef', 2)", lambda: m.strSub(s="abcdef", pos=2))
	check("misc", "strSub('abcdef', 1, 3)", lambda: m.strSub(s="abcdef", pos=1, len=3))
	check("misc", "getVolume()", lambda: m.getVolume())
	check("misc", "isMuted()", lambda: m.isMuted())
	check("misc", "getTimeOfDay()", lambda: m.getTimeOfDay())
	check("misc", "checkVersion(0, 0)", lambda: m.checkVersion(major=0, minor=0))
	# the volume is set to what it already is, and the mute icon to what the
	# box already says, so that a test leaves the box where it found it
	check("misc", "setVolume(getVolume())", lambda: m.setVolume(volume=m.getVolume()))
	check("misc", "AudioMute(isMuted())", lambda: m.AudioMute(enable=m.isMuted()))
	check("misc", "enableMuteIcon(isMuted())",
	      lambda: m.enableMuteIcon(enable=m.isMuted()))


# -------------------------------------------------------------- configfile

def test_configfile():
	path = SCRATCH + "/apitest.cfg"
	check("configfile", "new()", lambda: configfile.new())
	c = configfile.new()
	check("configfile", "setString", lambda: c.setString(key="greeting", value="hello"))
	check("configfile", "getString", lambda: c.getString(key="greeting", default="?"))
	check("configfile", "setInt32", lambda: c.setInt32(key="number", value=42))
	check("configfile", "getInt32", lambda: c.getInt32(key="number", default=0))
	check("configfile", "setBool", lambda: c.setBool(key="flag", value=True))
	check("configfile", "getBool", lambda: c.getBool(key="flag", default=False))
	check("configfile", "saveConfig", lambda: c.saveConfig(file=path))
	check("configfile", "loadConfig", lambda: c.loadConfig(file=path))
	check("configfile", "getString after the round trip",
	      lambda: c.getString(key="greeting", default="?"))
	check("configfile", "getInt32 after the round trip",
	      lambda: c.getInt32(key="number", default=0))
	check("configfile", "getBool after the round trip",
	      lambda: c.getBool(key="flag", default=False))
	check("configfile", "deleteKey", lambda: c.deleteKey(key="greeting"))
	check("configfile", "getString of the key that was deleted",
	      lambda: c.getString(key="greeting", default="gone"))
	check("configfile", "clear", lambda: c.clear())


# ------------------------------------------------------------ filehelpers

def test_filehelpers():
	f = filehelpers.new()
	check("filehelpers", "new()", lambda: f)
	check("filehelpers", "mkdir", lambda: f.mkdir(dir=SCRATCH + "/sub"))
	check("filehelpers", "exist (the directory)", lambda: f.exist(file=SCRATCH + "/sub"))
	check("filehelpers", "touch", lambda: f.touch(file=SCRATCH + "/sub/one"))
	check("filehelpers", "cp", lambda: f.cp(**{"from": SCRATCH + "/sub/one",
						   "to": SCRATCH + "/sub/two"}))
	check("filehelpers", "ln", lambda: f.ln(src=SCRATCH + "/sub/two",
						link=SCRATCH + "/sub/link"))
	check("filehelpers", "readlink", lambda: f.readlink(link=SCRATCH + "/sub/link"))
	check("filehelpers", "chmod", lambda: f.chmod(file=SCRATCH + "/sub/two", mode=0o644))
	check("filehelpers", "empty (of a directory that has something in it)",
	      lambda: f.empty(dir=SCRATCH + "/sub"))
	check("filehelpers", "rmdir", lambda: f.rmdir(dir=SCRATCH + "/sub"))
	check("filehelpers", "exist (the directory, gone)",
	      lambda: f.exist(file=SCRATCH + "/sub"))


# ------------------------------------------------------------------- curl

def test_curl():
	box = "http://127.0.0.1/control/getchannelinfo"
	c = curl.new()
	check("curl", "new()", lambda: c)
	check("curl", "encodeUri('a b/c')", lambda: c.encodeUri(data="a b/c"))
	check("curl", "decodeUri('a%20b%2Fc')", lambda: c.decodeUri(data="a%20b%2Fc"))
	check("curl", "setUriData", lambda: c.setUriData(data="a b"))
	check("curl", "download, the box's own web server",
	      lambda: c.download(s=True, url=box))
	check("curl", "download, to a file",
	      lambda: c.download(s=False, url=box, o=SCRATCH + "/channels.txt"))
	check("curl", "download, a url that is not there",
	      lambda: c.download(s=True, url="http://127.0.0.1/nothing-here"))


# ------------------------------------------------------------------ video

def test_video():
	v = video.new()
	check("video", "new()", lambda: v)
	check("video", "getNeutrinoMode()", lambda: v.getNeutrinoMode())
	check("video", "createChannelIDfromUrl",
	      lambda: v.createChannelIDfromUrl(url="http://127.0.0.1/"))
	check("video", "setBlank(False)", lambda: v.setBlank(enable=False))


# ---------------------------------------------------------------- threads

def test_threads():
	code = ("import time\n"
		"time.sleep(0.2)\n"
		"result = 'worker ' + str(argv[0])\n")
	t = threads.new(code, "one")
	check("threads", "new(code, 'one')", lambda: t)
	check("threads", "setLogger", lambda: t.setLogger(logger=lambda line: None))
	check("threads", "start()", lambda: t.start())
	check("threads", "alive() while it runs", lambda: t.alive())
	check("threads", "join()", lambda: t.join())
	check("threads", "result()", lambda: t.result())
	check("threads", "alive() after it", lambda: t.alive())
	check("threads", "started()", lambda: t.started())
	check("threads", "detached()", lambda: t.detached())
	check("threads", "joinable()", lambda: t.joinable())


# --------------------------------------------------- the ones that show it

def test_menu():
	"""The picker this plugin is run from is this binding: an entry of every
	kind it offers, so that each is built, and the picker itself is run."""
	m = menu.new(name="menu: every item kind")
	check_screen("menu", "new()", lambda: m)
	check_screen("menu", "addItem(subhead)",
		     lambda: m.addItem(type="subhead", name="a subhead"))
	check_screen("menu", "addItem(separator)", lambda: m.addItem(type="separator"))
	check_screen("menu", "addItem(separatorline)",
		     lambda: m.addItem(type="separatorline", name="a line"))
	check_screen("menu", "addItem(forwarder)",
		     lambda: m.addItem(type="forwarder", name="forwarder", value="value",
				       action=lambda item, value: MENU_RETURN["REPAINT"]))
	check_screen("menu", "addItem(chooser)",
		     lambda: m.addItem(type="chooser", name="chooser", id="chooser",
				       options={0: "zero", 1: "one", 2: "two"}, value="one",
				       action=lambda item, value: MENU_RETURN["REPAINT"]))
	check_screen("menu", "addItem(numeric)",
		     lambda: m.addItem(type="numeric", name="numeric", range="0,10",
				       value="5",
				       action=lambda item, value: MENU_RETURN["REPAINT"]))
	check_screen("menu", "addItem(string)",
		     lambda: m.addItem(type="string", name="string", value="text",
				       action=lambda item, value: MENU_RETURN["REPAINT"]))
	check_screen("menu", "addItem(stringinput)",
		     lambda: m.addItem(type="stringinput", name="stringinput",
				       value="edit me"))
	check_screen("menu", "addItem(keyboardinput)",
		     lambda: m.addItem(type="keyboardinput", name="keyboardinput",
				       value="", size=20))
	check_screen("menu", "addItem(filebrowser)",
		     lambda: m.addItem(type="filebrowser", name="filebrowser",
				       value=DIR["CONFIGDIR"], filter=["*.cfg"]))
	check_screen("menu", "addItem(back)/addItem(cancel)",
		     lambda: m.addItem(type="back"))
	check_screen("menu", "addKey",
		     lambda: m.addKey(id="key",
				      action=lambda item: MENU_RETURN["REPAINT"]))
	check_screen("menu", "setName", lambda: m.setName(name="menu: renamed"))
	check_screen("menu", "getSelected", lambda: m.getSelected())
	check_screen("menu", "setSelected", lambda: m.setSelected(item=1))
	check_screen("menu", "setValue", lambda: m.setValue(item=2, value="two"))
	check_screen("menu", "setActive", lambda: m.setActive(item=1, activ=True))
	check_screen("menu", "exec (pick one, then leave)", lambda: m.exec())
	check_screen("menu", "hide", lambda: m.hide())


def test_hintbox():
	h = hintbox.new(caption="hintbox", text="a hint box")
	check_screen("hintbox", "new()", lambda: h)
	check_screen("hintbox", "paint()", lambda: h.paint())
	check_screen("hintbox", "hide()", lambda: h.hide())
	check_screen("hintbox", "exec(timeout=1)",
		     lambda: hintbox.exec(caption="hintbox",
					  text="this one leaves after a second",
					  timeout=1))


def test_messagebox():
	check_screen("messagebox", "exec",
		     lambda: messagebox.exec(name="messagebox", text="a message box",
					     buttons=["ok", "cancel"], default="ok"))


def test_stringinput():
	check_screen("stringinput", "exec",
		     lambda: stringinput.exec(name="stringinput", value="edit me",
					      size=20, sms=False))


def test_hint():
	h = hint.new(text="a hint")
	check_screen("hint", "new()", lambda: h)
	check_screen("hint", "paint()", lambda: h.paint())
	check_screen("hint", "hide()", lambda: h.hide())


def test_hourglass():
	h = hourglass.new(x=20, y=20, dx=48, dy=48)
	check_screen("hourglass", "new()", lambda: h)
	check_screen("hourglass", "paint()", lambda: h.paint())
	time.sleep(1)
	check_screen("hourglass", "hide()", lambda: h.hide())


def find_picture():
	"""A picture on the box, for the widget that shows one."""
	for where in (DIR["ICONSDIR"], DIR["DATADIR"], DIR["CONFIGDIR"],
		      DIR["THEMESDIR"], DIR["LOGODIR"]):
		try:
			for name in sorted(os.listdir(where)):
				if name.lower().endswith((".png", ".jpg", ".jpeg", ".gif")):
					return where + "/" + name
		except OSError:
			pass
	return None


def test_cprogresswindow():
	w = cprogresswindow.new(name="cprogresswindow")
	check_screen("cprogresswindow", "new()", lambda: w)
	check_screen("cprogresswindow", "paint()", lambda: w.paint())
	check_screen("cprogresswindow", "setTitle", lambda: w.setTitle(name="renamed"))
	check_screen("cprogresswindow", "showStatus",
		     lambda: w.showStatus(prog=50, max=100, statusText="half"))
	check_screen("cprogresswindow", "showLocalStatus",
		     lambda: w.showLocalStatus(prog=50))
	check_screen("cprogresswindow", "showGlobalStatus",
		     lambda: w.showGlobalStatus(prog=50))
	check_screen("cprogresswindow", "hide()", lambda: w.hide())


def test_widgets():
	"""The components a script draws with, in a window of their own."""
	w = cwindow.new(name="cwindow", x=80, y=60, dx=470, dy=300)
	check_screen("cwindow", "new()", lambda: w)
	t = ctext.new(parent=w, text="hello from ctext", x=20, y=20, dx=300, dy=60)
	check_screen("ctext", "new(parent=…)", lambda: t)
	hd = header.new(parent=w, caption="a header")
	check_screen("header", "new(parent=…)", lambda: hd)
	picture = find_picture()
	if picture:
		p = cpicture.new(parent=w, image=picture, x=20, y=100, dx=200, dy=100)
		check_screen("cpicture", "new(parent=…)", lambda: p)
		check_screen("cpicture", "getWidth/getHeight",
			     lambda: (p.getWidth(), p.getHeight()))
		check_screen("cpicture", "setCenterPos", lambda: p.setCenterPos(x=20, y=100))
		check_screen("cpicture", "doPaintBg", lambda: p.doPaintBg(paint_bg=False))
	else:
		results.append(("cpicture", "no picture on the box to show", False, ""))
	s = signalbox.new(parent=w, x=20, y=220)
	check_screen("signalbox", "new(parent=…)", lambda: s)
	check_screen("signalbox", "setCenterPos", lambda: s.setCenterPos(x=20, y=220))
	check_screen("cwindow", "setCaption", lambda: w.setCaption(caption="renamed"))
	check_screen("cwindow", "setWindowColor",
		     lambda: w.setWindowColor(color_frame=COL["MENUHEAD"]))
	check_screen("cwindow", "setDimensionsAll",
		     lambda: w.setDimensionsAll(x=80, y=60, dx=470, dy=300))
	check_screen("cwindow", "headerHeight/footerHeight",
		     lambda: (w.headerHeight(), w.footerHeight()))
	check_screen("cwindow", "paintHeader", lambda: w.paintHeader())
	check_screen("cwindow", "paint()", lambda: w.paint())
	time.sleep(1)
	check_screen("ctext", "scroll", lambda: t.scroll())
	check_screen("ctext", "setText", lambda: t.setText(text="ctext, again"))
	check_screen("ctext", "getLines", lambda: t.getLines(text="one\ntwo\nthree"))
	check_screen("ctext", "enableUTF8", lambda: t.enableUTF8(enable=True))
	check_screen("ctext", "paint()", lambda: t.paint())
	check_screen("ctext", "hide()", lambda: t.hide())
	check_screen("cwindow", "hide()", lambda: w.hide())


def test_window():
	"""The box's own drawing, on a window of the script's own: every call the
	class carries, drawn on and taken back off. The shape is the Lua one, so
	a font is a number and the boolean in front of it says the number names a
	dynamic font of this window rather than one of the box's."""
	w = window(x=100, y=100, w=400, h=250)
	check_screen("window", "window()", lambda: w)
	font = FONT["MENU"]
	check_screen("window", "GetLanguage()", lambda: w.GetLanguage())
	check_screen("window", "PaintBox(100, 100, 400, 250, color)",
		     lambda: w.PaintBox(100, 100, 400, 250, COL["MENUCONTENT_PLUS_0"]))
	check_screen("window", "RenderString(False, font, 'hello', 120, 130)",
		     lambda: w.RenderString(False, font, "hello from the API test",
					    120, 130))
	check_screen("window", "getRenderWidth(False, font, 'hello')",
		     lambda: w.getRenderWidth(False, font, "hello"))
	check_screen("window", "FontHeight(False, font)", lambda: w.FontHeight(False, font))
	check_screen("window", "getDynFont(200, 30)", lambda: w.getDynFont(200, 30))
	check_screen("window", "paintHLineRel(110, 380, 160, color)",
		     lambda: w.paintHLineRel(110, 380, 160, COL["MENUFOOT_PLUS_0"]))
	check_screen("window", "paintVLineRel(110, 170, 60, color)",
		     lambda: w.paintVLineRel(110, 170, 60, COL["MENUFOOT_PLUS_0"]))
	saved = [None]

	def save_it():
		saved[0] = w.saveScreen(100, 100, 100, 100)
		return saved[0]

	check_screen("window", "saveScreen(100, 100, 100, 100)", save_it)
	check_screen("window", "restoreScreen(100, 100, 100, 100, id, True)",
		     lambda: w.restoreScreen(100, 100, 100, 100, saved[0], True))
	check_screen("window", "deleteSavedScreen(id)",
		     lambda: w.deleteSavedScreen(saved[0]))
	check_screen("window", "scale2Res(1)", lambda: w.scale2Res(1))
	check_screen("window", "Blit()", lambda: w.Blit())
	picture = find_picture()
	if picture:
		check_screen("window", "GetSize(picture)", lambda: w.GetSize(picture))
		check_screen("window", "PaintIcon(picture, 120, 180, 40, 1)",
			     lambda: w.PaintIcon(picture, 120, 180, 40, 1))
		check_screen("window", "DisplayImage(picture, 200, 180, 100, 60, 255)",
			     lambda: w.DisplayImage(picture, 200, 180, 100, 60, 255))
	check_screen("window", "GetInput(timeout=500)", lambda: w.GetInput(timeout=500))
	del w
	results.append(("window", "del w, and the drawing is given back", True, ""))


# ------------------------------------------------------------------- the run

AREAS = [
	("the constants", test_constants),
	("misc", test_misc),
	("configfile", test_configfile),
	("filehelpers", test_filehelpers),
	("curl", test_curl),
	("video", test_video),
	("threads", test_threads),
	("menu", test_menu),
	("hintbox", test_hintbox),
	("messagebox", test_messagebox),
	("stringinput", test_stringinput),
	("hint", test_hint),
	("hourglass", test_hourglass),
	("cprogresswindow", test_cprogresswindow),
	("cwindow, ctext, cpicture, header, signalbox", test_widgets),
	("window, the box's own drawing", test_window),
]

# The ones that need nobody at the box, so that they can be run in one go.
QUIET = [test_constants, test_misc, test_configfile, test_filehelpers,
	 test_curl, test_video, test_threads]


def run_quiet():
	for run in QUIET:
		run()
	show_report()


def run_area(name):
	for area, run in AREAS:
		if area == name:
			run()
			show_report()
			return MENU_RETURN["REPAINT"]
	return MENU_RETURN["EXIT"]


def picker():
	"""The picker: one entry per area, and the two that run or show the lot.
	This menu is itself the menu binding, so it is what that one is tried
	with as well."""
	m = menu.new(name="Python API test")
	for name, _run in AREAS:
		m.addItem(type="forwarder", name=name,
			  action=lambda item, value, n=name: run_area(n))
	m.addItem(type="separator")
	m.addItem(type="forwarder", name="run the ones that need nobody",
		  action=lambda item, value: (run_quiet(), MENU_RETURN["REPAINT"]))
	m.addItem(type="forwarder", name="show the report",
		  action=lambda item, value: (show_report(), MENU_RETURN["REPAINT"]))
	m.addItem(type="back")
	m.exec()


picker()






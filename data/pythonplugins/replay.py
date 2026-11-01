#!/usr/bin/env python3
# replay.py - the Lua plugin scripts-lua/plugins/replay/replay.lua, in Python.
#
# It offers the live stream of a public broadcaster shifted back by up to the
# buffer the sender allows: the manifest the box plays is downloaded, its
# time shift depth is opened up, and its period is moved back by the minutes
# the viewer picks. The key that ends the play decides whether the picker
# comes up again, so that a viewer can step through the buffer.
#
# What it is written against is src/gui/python: curl for the two downloads,
# video for the play, hintbox for the notice and menu for the picker the Lua
# plugin ran through the shell's msgbox. A .cfg with type=5 beside it, as the
# replay.cfg next to this file, is what makes the box list it as a plugin.
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
import re
import time

from neutrino import CURL, MENU_RETURN, curl, hintbox, menu, video

# The manifest each sender's live stream stands in.
sender_mpd = {
	"3sat": "https://zdf-dash-18.akamaized.net/dash/live/2016511/dach/manifest.mpd",
	"ARD alpha": "https://ardalphadash.akamaized.net/dash/live/2016972/ard_alpha/dvbt2/manifest.mpd",
	"arte": "https://arteliveext.akamaized.net/dash/live/2031004/artelive_de/dash.mpd",
	"BR Fernsehen Nord": "https://bfrnorddash.akamaized.net/dash/live/2016971/bfs_nord_de/dvbt2/manifest.mpd",
	"BR Fernsehen Süd": "https://bfrsueddash.akamaized.net/dash/live/2016970/bfs_sued_de/dvbt2/manifest.mpd",
	"BR Süd": "https://bfrsueddash.akamaized.net/dash/live/2016970/bfs_sued_de/dvbt2/manifest.mpd",
	"Das Erste": "https://daserste-live.ard-mcdn.de/daserste/replay/dash/de/manifest.mpd",
	"hr-fernsehen": "https://hrdashde.akamaized.net/dash/live/2024544/hrdashde/manifest.mpd",
	"KiKA": "https://kikageoilsdash.akamaized.net/dash/live/2099498/dashhbbtv-ebu-proxy-full/manifest.mpd",
	"MDR Sachsen": "https://mdrtvsndash.akamaized.net/dash/live/2094117/mdrtvsn/dash.mpd",
	"MDR S-Anhalt": "https://mdrtvsadash.akamaized.net/dash/live/2094116/mdrtvsa/dash.mpd",
	"MDR Thüringen": "https://mdrtvthdash.akamaized.net/dash/live/2094118/mdrtvth/dash.mpd",
	"NDR FS HH": "https://mcdn.ndr.de/ndr/dash/ndr_hbbtv/ndr_hbbtv_hh/ndr_hbbtv_hh.mpd",
	"NDR FS MV": "https://mcdn.ndr.de/ndr/dash/ndr_hbbtv/ndr_hbbtv_mv/ndr_hbbtv_mv.mpd",
	"NDR FS NDS": "https://mcdn.ndr.de/ndr/dash/ndr_hbbtv/ndr_hbbtv_nds/ndr_hbbtv_nds.mpd",
	"NDR FS SH": "https://mcdn.ndr.de/ndr/dash/ndr_hbbtv/ndr_hbbtv_sh/ndr_hbbtv_sh.mpd",
	"ONE": "https://mcdn-one.ard.de/ardone/dash/manifest.mpd",
	"PHOENIX": "https://zdf-dash-19.akamaized.net/dash/live/2016512/de/manifest.mpd",
	"phoenix": "https://zdf-dash-19.akamaized.net/dash/live/2016512/de/manifest.mpd",
	"Radio Bremen": "https://rbdashlive.akamaized.net/dash/live/2020436/rbfs/dash.mpd",
	"rbb Berlin": "https://rbb-dash-berlin.akamaized.net/dash/live/2017826/rbb_berlin/manifest.mpd",
	"rbb Brandenburg": "https://rbb-dash-brandenburg.akamaized.net/dash/live/2017827/rbb_brandenburg/manifest.mpd",
	"SR Fernsehen": "https://swrsrfs-dash.akamaized.net/dash/live/2018687/srfsgeo/dash.mpd",
	"SWR BW": "https://swrbw-dash.akamaized.net/dash/live/2018674/swrbwd/manifest.mpd",
	"SWR RP": "https://swrrp-dash.akamaized.net/dash/live/2018680/swrrpd/manifest.mpd",
	"tagesschau24": "https://tagesschau.akamaized.net/dash/live/2020098/tagesschau/tagesschau_3/tagesschau_3.mpd",
	"WDR Aachen": "https://wdrlokalzeit.akamaized.net/dash/live/2018107/wdrlz_aachen/dash.mpd",
	"WDR Bielefeld": "https://wdrlokalzeit.akamaized.net/dash/live/2018117/wdrlz_bielefeld/dash.mpd",
	"WDR Bonn": "https://wdrlokalzeit.akamaized.net/dash/live/2018112/wdrlz_bonn/dash.mpd",
	"WDR Dortmund": "https://wdrlokalzeit.akamaized.net/dash/live/2018113/wdrlz_dortmund/dash.mpd",
	"WDR Duisburg": "https://wdrlokalzeit.akamaized.net/dash/live/2018115/wdrlz_duisburg/dash.mpd",
	"WDR Düsseldorf": "https://wdrlokalzeit.akamaized.net/dash/live/2018114/wdrlz_duesseldorf/dash.mpd",
	"WDR Essen": "https://wdrlokalzeit.akamaized.net/dash/live/2018118/wdrlz_essen/dash.mpd",
	"WDR Köln": "https://wdrfs247.akamaized.net/dash/live/2016702/wdrfs247_geo/dash.mpd",
	"WDR Münster": "https://wdrlokalzeit.akamaized.net/dash/live/2018116/wdrlz_muensterland/dash.mpd",
	"WDR Siegen": "https://wdrlokalzeit.akamaized.net/dash/live/2018111/wdrlz_siegen/dash.mpd",
	"WDR Wuppertal": "https://wdrlokalzeit.akamaized.net/dash/live/2018126/wdrlz_wuppertal/dash.mpd",
	"ZDF": "https://zdf-dash-15.akamaized.net/dash/live/2016508/de/manifest.mpd",
	"ZDFinfo": "https://zdf-dash-17.akamaized.net/dash/live/2016510/de/manifest.mpd",
	"ZDFneo": "https://zdf-dash-16.akamaized.net/dash/live/2016509/de/manifest.mpd",
	"zdf_neo": "https://zdf-dash-16.akamaized.net/dash/live/2016509/de/manifest.mpd",
}

outputfile = "/tmp/output.mpd"
vPlay = video.new()
Curl = curl.new()


def umlaute(s):
	"""The Lua plugin maps the latin-1 bytes the box answers to the
	characters a reader sees. Python holds those characters already, so
	what is left of that here is the suffix the box adds to a name."""
	return s.replace(" HD", "")


def getdata(url, output_file=None):
	if not url:
		return None
	ret, data = Curl.download(s=True, url=url, A="Mozilla/5.0", o=output_file)
	if ret == CURL["OK"]:
		return 1 if output_file else data
	return None


def putdata(text, output_file):
	with open(output_file, "wb") as f:
		f.write(text)


def get_text(path):
	"""The lines of a file, as bytes: a manifest is a document, and the
	Lua plugin edits it byte for byte."""
	try:
		with open(path, "rb") as f:
			return f.read().splitlines()
	except OSError:
		return None


def replay(name, epg_now, epg_next, full_time):
	"""The key the play ended on, which is what the Lua plugin reads."""
	return vPlay.PlayFile("Replay - " + name, outputfile,
			      epg_now + " (" + str(full_time) + " min)", epg_next)


def message(txt, seconds=3):
	h = hintbox.new(caption="Hinweis", text=txt)
	if h:
		h.paint()
	time.sleep(seconds)
	h.hide()


def parse_time_shift(value):
	"""PTnHnMnS as the minutes it stands for, on bytes."""
	hours = re.search(rb"(\d+)H", value)
	minutes = re.search(rb"(\d+)M", value)
	seconds = re.search(rb"([\d.]+)S", value)
	return (int(hours.group(1)) if hours else 0) * 60 + \
	       (int(minutes.group(1)) if minutes else 0) + \
	       (float(seconds.group(1)) if seconds else 0.0) / 60


def quit_replay():
	try:
		os.remove(outputfile)
	except OSError:
		pass


def get_channelinfo():
	"""The four lines the box answers about what it is showing now."""
	global zeit, fzeit

	ret, data = Curl.download(s=True, url="http://127.0.0.1/control/getchannelinfo")
	# The box answers in the encoding its own names are written in, which is
	# what the Lua plugin's umlaute() turns into characters.
	lines = data.decode("latin-1").splitlines() if data else []
	channelinfo = [line for line in lines if line]
	if len(channelinfo) < 4:
		channelinfo.insert(1, "keine Daten")
	zeit, fzeit = channelinfo[2].split("/", 1)
	zeit = int(zeit, 10)	# vergangene Zeit
	fzeit = int(fzeit, 10)	# volle Laufzeit
	return channelinfo


def adjust_buffer(max_buffer):
	if max_buffer == 40:		# BR handling
		max_buffer = 120
	elif max_buffer == 180:		# ZDF handling
		max_buffer = 160
	return max_buffer


def ask_step(trim, max_buffer):
	"""The picker the Lua plugin ran through the shell's msgbox, as the
	API's own menu: the steps it offers, and the one that was chosen."""
	steps = [-max_buffer, -25, -10, -5, -1, 0, 1, 5, 10, 25]
	chosen = [None]

	def on_pick(item_id, value):
		chosen[0] = value
		return MENU_RETURN["EXIT"]

	m = menu.new(name="Replay    << " + str(trim) + " / " + str(max_buffer) + " min.")
	m.addItem(type="chooser", id="step", name="~G ~c zurück    0 = Livestream    vorwärts",
		  options=dict(enumerate(["%+d" % step for step in steps])),
		  value="0", action=on_pick)
	m.exec()

	# a picker nobody moved answers nothing, and the step it stood on is 0
	return 0 if chosen[0] is None else int(chosen[0])


# ------------------------------------------------------------------ replay

channelinfo = get_channelinfo()
sender = channelinfo[0]
name = umlaute(sender)
mpd_url = sender_mpd.get(name)

if mpd_url:
	file = getdata(mpd_url, outputfile)
	if file:
		base_url = mpd_url.rsplit("/", 1)[0] + "/"
		mpdlines = get_text(outputfile)
		get_buffer = re.search(rb'timeShiftBufferDepth="([^"]+)"', mpdlines[1]).group(1)
		max_buffer = int(parse_time_shift(get_buffer) + 0.999)	# math.ceil
		max_buffer = adjust_buffer(max_buffer)

		mpdlines[1] = re.sub(rb'timeShiftBufferDepth="PT[^"]+"',
				     b'timeShiftBufferDepth="PT3H0M0S"', mpdlines[1])
		if b"<BaseURL" in mpdlines[2]:
			# BaseURL gefunden
			if b"<BaseURL" in mpdlines[3]:
				del mpdlines[3]

			mpdlines[7] = mpdlines[7].replace(b"video_00", b"video_01")	# zdf handling
			mpdlines[8] = mpdlines[8].replace(b"video_01", b"video_00")
		else:
			# BaseURL nicht gefunden
			mpdlines.insert(2, b"  <BaseURL>" + base_url.encode() + b"</BaseURL>")

			if b"<Role" in mpdlines[5]:		# arte handling
				if b"720" in mpdlines[15]:
					mpdlines[6] = mpdlines[15]
					max_buffer = 30

		output = b"\n".join(mpdlines)
		trim = zeit
		div = 0
		key_press = div
		trim_output = output
		while True:
			div = int(div)
			if div < 0:
				trim = trim + abs(div)
			else:
				trim = trim - abs(div)
			trim = max(1, min(trim, max_buffer - 1))	# Livestream Begrenzung
			trim_text = "%g" % trim
			if b"<Period" in mpdlines[3]:
				# Period gefunden
				output = output.replace(mpdlines[3],
							b'  <Period start="PT' + trim_text.encode() + b'M" id="1">')
			putdata(output, outputfile)
			key_press = replay(sender + "      <<  " + trim_text + " / " + str(max_buffer) + " min.",
					   umlaute(channelinfo[1]), umlaute(channelinfo[3]), fzeit)
			if key_press == 2:
				break		# Taste Down
			elif key_press == 3:
				div = ask_step(trim_text, max_buffer)
				if div == 0:
					trim = div
				elif div == max_buffer:
					div = 0
					trim = max_buffer
			else:
				break
			output = trim_output
			if key_press == 1:
				break
	else:
		message("Konnte mpd nicht finden / laden.", 3)
else:
	message("Replay nicht verfügbar für - " + sender, 3)

quit_replay()



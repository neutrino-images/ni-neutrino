AM_CPPFLAGS = -fno-rtti -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -D_FILE_OFFSET_BITS=64

# Regenerated only by "make" / "make all". Other goals ("make install", and
# the all-am pass automake runs after compiling) keep an existing one, so
# nothing that includes it is rebuilt. Not .PHONY: make would always
# rebuild its dependents.
VERSION_H = version.h
$(VERSION_H): FORCE
	@case "$(MAKECMDGOALS)" in ""|all) ;; *) test -f $@ && exit 0;; esac; \
	echo "#define BUILT_DATE \"$$(date)\"" > $@; \
	if test -d $(top_srcdir)/.git; then \
		pushd $(top_srcdir); \
		GITTAG=$$(git tag -l "NI-*" --sort=version:refname | tail -n1); \
		GITDESCRIBE=$$(git describe --always --long --tags --match $$GITTAG | sed 's/-/./2' || echo "VCS failed"); \
		popd; \
		echo "#define VCS \"$${GITDESCRIBE}\"" >> $@; \
	fi

BUILT_SOURCES = $(VERSION_H)
noinst_HEADERS = \
	$(VERSION_H) \
	scriptapi/script_color.h \
	scriptapi/script_curl.h \
	scriptapi/script_neutrino.h \
	scriptapi/script_hintbox.h \
	scriptapi/script_menu.h \
	scriptapi/script_progresswindow.h \
	scriptapi/script_window.h \
	scriptapi/script_text.h \
	scriptapi/script_picture.h \
	scriptapi/script_header.h \
	scriptapi/script_signalbox.h \
	scriptapi/script_hourglass.h \
	scriptapi/script_stringinput.h \
	scriptapi/script_hint.h \
	scriptapi/script_messagebox.h \
	scriptapi/script_misc.h

FORCE:

# The layer under scriptapi/ stands between the two bindings and the components
# they drive, and the check below keeps the three in step. It hangs on "all"
# here exactly as src/coreapi/check-layering.sh hangs on "all" in its own
# Makefile.am - the autotools way in, and silent when it passes.
#
# A build that does not read these makefiles has to be told to run it, and
# ni-buildsystem is one of those:
#	sh src/gui/scriptapi/check-scriptapi.sh <top of the tree>
# Hence the top of the tree as an argument: the check reads both bindings and
# the layer, wherever the tree stands.
EXTRA_DIST = scriptapi/check-scriptapi.sh

check-scriptapi:
	@$(SHELL) $(srcdir)/scriptapi/check-scriptapi.sh $(top_srcdir)

.PHONY: check-scriptapi

all-local: check-scriptapi

SUBDIRS = bedit components moviebrowser moviedb widget

if ENABLE_LUA
SUBDIRS += lua
endif

if ENABLE_PYTHON
SUBDIRS += python
endif

AM_CPPFLAGS += \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/xmltree \
	-I$(top_srcdir)/lib/libupnpclient \
	-I$(top_srcdir)/lib/jsoncpp \
	@SIGC_CFLAGS@ \
	@CURL_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	@AVFORMAT_CFLAGS@ \
	@AVUTIL_CFLAGS@ \
	@LUA_CFLAGS@ \
	@PYTHON_CFLAGS@ \
	@HWLIB_CFLAGS@

noinst_LIBRARIES = libtimerlist.a libneutrino_gui.a libneutrino_gui2.a libneutrino_gui_scriptapi.a

libneutrino_gui_a_SOURCES = \
	adzap.cpp \
	audio_select.cpp \
	audio_setup.cpp \
	audiomute.cpp \
	audioplayer.cpp \
	audioplayer_setup.cpp\
	bookmarkmanager.cpp \
	bouquetlist.cpp \
	buildinfo.cpp \
	channellist.cpp \
	cec_setup.cpp \
	daemon_control.cpp \
	dboxinfo.cpp \
	epgplus.cpp \
	epgview.cpp \
	eventlist.cpp \
	favorites.cpp \
	filebrowser.cpp \
	followscreenings.cpp \
	frontend_label.cpp \
	imageinfo_ni.cpp \
	info_menue.cpp \
	infoviewer.cpp \
	infoviewer_base.cpp \
	infoviewer_evo.cpp \
	infoviewer_bb.cpp \
	zap_failure_message.cpp \
	keybind_setup.cpp \
	mediaplayer.cpp \
	mediaplayer_setup.cpp \
	miscsettings_menu.cpp \
	movieplayer.cpp \
	network_service.cpp \
	network_setup.cpp \
	nfs.cpp \
	osd_helpers.cpp \
	osd_progressbar_setup.cpp \
	osd_setup.cpp \
	osdlang_setup.cpp \
	parentallock_setup.cpp \
	personalize.cpp \
	pictureviewer.cpp \
	pictureviewer_setup.cpp \
	pluginlist.cpp \
	plugins.cpp \
	plugins_hide.cpp \
	poweroff_menu.cpp \
	proxyserver_setup.cpp \
	radiotext_window.cpp \
	rate_banner.cpp \
	rc_lock.cpp \
	record_setup.cpp \
	scan.cpp \
	scan_setup.cpp \
	screensaver.cpp \
	screensetup.cpp \
	settings_manager.cpp \
	sleeptimer.cpp \
	start_wizard.cpp \
	subchannel_select.cpp \
	test_menu.cpp \
	themes.cpp \
	timeosd.cpp \
	update.cpp \
	update_check.cpp \
	update_menue.cpp \
	update_settings.cpp \
	user_menue.cpp \
	user_menue_setup.cpp \
	vfd_setup.cpp \
	videosettings.cpp \
	volumebar.cpp \
	weather.cpp \
	weather_setup.cpp \
	webchannels_setup.cpp \
	xmltv_setup.cpp \
	zapit_setup.cpp

# The screen that sets the web server up, which a build without that server has
# nothing for. Named under the same switch the link and the two calls that
# start and stop it are named under, so the three cannot come apart.
if ENABLE_NI_WEB
libneutrino_gui_a_SOURCES += \
	webserver_setup.cpp
endif

if ENABLE_PIP
libneutrino_gui_a_SOURCES += \
	pipsetup.cpp
if ENABLE_QUADPIP
libneutrino_gui_a_SOURCES += \
	quadpip_setup.cpp
endif
endif

if ENABLE_PKG_MANAGEMENT
libneutrino_gui_a_SOURCES += \
	opkg_manager.cpp \
	update_check_packages.cpp
endif

if ENABLE_LCD4LINUX
libneutrino_gui_a_SOURCES += \
	lcd4l_setup.cpp
endif

if ENABLE_GRAPHLCD
libneutrino_gui_a_SOURCES += \
	glcdsetup.cpp \
	glcdthemes.cpp
endif

if BOXTYPE_CST
libneutrino_gui_a_SOURCES += \
	streaminfo1.cpp
else
libneutrino_gui_a_SOURCES += \
	streaminfo2.cpp
endif

if ENABLE_EXTUPDATE
libneutrino_gui_a_SOURCES += \
	update_ext.cpp
endif

if BOXTYPE_ARMBOX
libneutrino_gui_a_SOURCES += \
	psisetup.cpp
endif

if BOXTYPE_MIPSBOX
libneutrino_gui_a_SOURCES += \
	psisetup.cpp
endif

libneutrino_gui2_a_SOURCES = \
	cam_menu.cpp \
	color.cpp \
	hdd_info.cpp \
	hdd_menu.cpp \
	infoclock.cpp \
	infoicons.cpp \
	infoicons_setup.cpp \
	motorcontrol.cpp \
	netfs_setup.cpp

if ENABLE_UPNP
libneutrino_gui2_a_SOURCES += \
	upnpbrowser.cpp
endif

# The script API both bindings build their windows with. A library of its own,
# and not a part of libneutrino_gui.a, because it has to stand behind the
# bindings on the link line: an archive answers only what something on the line
# has asked for already, and the bindings ask after the gui archive has been
# passed, so a part of it would be drawn in one pass too late.
libneutrino_gui_scriptapi_a_SOURCES = \
	scriptapi/script_hintbox.cpp \
	scriptapi/script_menu.cpp \
	scriptapi/script_progresswindow.cpp \
	scriptapi/script_window.cpp \
	scriptapi/script_text.cpp \
	scriptapi/script_picture.cpp \
	scriptapi/script_header.cpp \
	scriptapi/script_signalbox.cpp \
	scriptapi/script_hourglass.cpp \
	scriptapi/script_stringinput.cpp \
	scriptapi/script_hint.cpp \
	scriptapi/script_messagebox.cpp \
	scriptapi/script_misc.cpp \
	scriptapi/script_curl.cpp \
	scriptapi/script_neutrino.cpp

libtimerlist_a_SOURCES = \
	timerlist.cpp

# Built with exceptions because the layers above can throw and the catch has
# to be reachable through these frames.
#
# A table written as an aggregate that stops short of the last member leaves the
# rest zero and says nothing, so that one warning is an error: in a parallel
# build a warning scrolls past and the build still succeeds.
AM_CXXFLAGS = -std=c++11 -fno-rtti -fexceptions -Wall -Wextra -Werror=missing-field-initializers -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/include \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/xmltree \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/timerdclient \
	-I$(top_srcdir)/lib/sectionsdclient \
	@HWLIB_CFLAGS@

# The table of key names is generated beside the tool that first needed it, out
# of the input layer's own header. A file that has never been written is not one
# the recorded dependencies can bring into being, so it is asked for here rather
# than left to the order the directories happen to be built in.
BUILT_SOURCES = $(top_builddir)/src/tools/rcsim.h

$(top_builddir)/src/tools/rcsim.h:
	$(MAKE) $(AM_MAKEFLAGS) -C $(top_builddir)/src/tools rcsim.h

# The three subdirectories are one archive and not three: a second archive
# would be a second thing for every link line in the tree to name. Their
# objects stay beside their sources so the rules at the bottom can say which
# directory an object came from.
AUTOMAKE_OPTIONS = subdir-objects

noinst_LIBRARIES = libcoreapi.a

libcoreapi_a_SOURCES = \
	base/deps.cpp \
	box/channelsource_real.cpp \
	box/systemsource_real.cpp \
	box/tunersource_real.cpp \
	box/inputdevice_real.cpp \
	box/screenshotsource_real.cpp \
	box/displaypicture.cpp \
	box/logosource_real.cpp \
	box/pluginsource_real.cpp \
	box/recordingsource_real.cpp \
	box/settingssource_real.cpp \
	box/localesource_real.cpp \
	box/keysource_real.cpp \
	box/recordingsafetysource_real.cpp \
	settings/settingstable.cpp \
	settings/settingsundeclared.cpp \
	settings/settingstable_audio.cpp \
	settings/settingstable_osd.cpp \
	settings/settingstable_misc.cpp \
	settings/settingstable_video.cpp \
	settings/settingstable_recording.cpp \
	settings/settingstable_channel.cpp \
	settings/settingstable_network.cpp \
	settings/settingstable_weather.cpp \
	settings/settingstable_keys.cpp \
	settings/settingstable_display.cpp \
	settings/settingstable_player.cpp \
	settings/settingstable_parental.cpp \
	settings/settingstable_cam.cpp \
	settings/settingstable_hdd.cpp \
	settings/settingstable_update.cpp \
	settings/settingstable_theme.cpp \
	settings/settingstable_elements.cpp \
	settings/settingstable_lists.cpp \
	box/commandsink_real.cpp \
	box/eventsink_real.cpp \
	channels.cpp \
	bouquetorder.cpp \
	epg.cpp \
	timers.cpp \
	system.cpp \
	decryption.cpp \
	tuner.cpp \
	plugins.cpp \
	recordings.cpp \
	archive.cpp \
	playback.cpp \
	osd.cpp \
	settings/settings.cpp \
	settings/apply.cpp \
	settings/apply_groups.cpp \
	box/applyseams.cpp \
	box/apply_video.cpp \
	box/videooutput_real.cpp \
	box/apply_audio.cpp \
	box/audiooutput_real.cpp \
	box/apply_cec.cpp \
	box/cecoutput_real.cpp \
	network.cpp \
	box/apply_channels.cpp \
	box/apply_misc.cpp \
	box/miscoutput_real.cpp \
	box/apply_sectionsd.cpp \
	box/sectionsdoutput_real.cpp \
	box/apply_keys.cpp \
	box/rccontrol_real.cpp \
	box/apply_lang.cpp \
	box/localization_real.cpp \
	settings/choicesources.cpp \
	box/apply_ci.cpp \
	box/cicontrol_real.cpp \
	box/applyworker.cpp \
	box/apply_services.cpp \
	box/servicecontrol_real.cpp \
	box/apply_update.cpp \
	box/updatecheck_real.cpp \
	box/apply_plugins.cpp \
	box/pluginloader_real.cpp \
	box/apply_weather.cpp \
	box/weather_real.cpp \
	box/apply_lcd4l.cpp \
	box/lcd4l_real.cpp \
	box/apply_vfd.cpp \
	box/vfdpanel_real.cpp \
	box/apply_glcd.cpp \
	box/glcdpanel_real.cpp \
	box/apply_osd.cpp \
	box/osdoutput_real.cpp \
	box/apply_record.cpp \
	box/recordconfig_real.cpp \
	box/apply_hdd.cpp \
	box/hddcontrol_real.cpp \
	box/apply_webchannels.cpp \
	box/webchannels_real.cpp \
	settings/couple.cpp \
	settings/couple_epg.cpp \
	settings/couple_osd.cpp \
	settings/couple_channel.cpp \
	settings/couple_weather.cpp \
	settings/couple_plugins.cpp \
	settings/couple_ci.cpp \
	settings/couple_lcd4l.cpp \
	settings/menuspec.cpp \
	settings/predicates.cpp \
	streaming.cpp \
	browserplay.cpp \
	shellcontract.cpp \
	shelltable.cpp \
	storage.cpp \
	box/storage_disks.cpp \
	netfs.cpp \
	daemons.cpp \
	base/eventbus.cpp \
	base/messagebridge.cpp

noinst_HEADERS = \
	base/errors.h \
	base/result.h \
	base/types.h \
	base/version.h \
	base/deps.h \
	channels.h \
	box/channelstreams_internal.h \
	box/displaypicture.h \
	bouquetorder.h \
	epg.h \
	timers.h \
	system.h \
	decryption.h \
	tuner.h \
	plugins.h \
	recordings.h \
	archive.h \
	playback.h \
	osd.h \
	base/bootmode.h \
	base/apply.h \
	box/apply_video.h \
	box/apply_audio.h \
	box/apply_cec.h \
	network.h \
	box/apply_channels.h \
	box/apply_misc.h \
	box/apply_sectionsd.h \
	box/apply_keys.h \
	box/apply_lang.h \
	settings/choicesources.h \
	box/apply_ci.h \
	box/applyworker.h \
	box/apply_services.h \
	box/apply_update.h \
	box/apply_plugins.h \
	box/apply_weather.h \
	box/apply_lcd4l.h \
	box/apply_vfd.h \
	box/apply_glcd.h \
	box/apply_osd.h \
	box/apply_record.h \
	box/apply_hdd.h \
	box/apply_webchannels.h \
	box/sentstate.h \
	box/mode43.h \
	base/schema.h \
	base/flagfile.h \
	settings/settings.h \
	settings/couple.h \
	settings/menuspec.h \
	settings/boxdefaults.h \
	settings/predicates.h \
	settings/settingsfield.h \
	settings/settingstable.h \
	settings/videomodes.h \
	streaming.h \
	browserplay.h \
	shellcontract.h \
	box/shellcontract_internal.h \
	shelltable.h \
	storage.h \
	box/storage_internal.h \
	box/storage_disks.h \
	netfs.h \
	daemons.h \
	base/eventbus.h \
	base/messagebridge.h

EXTRA_DIST = check-layering.sh

check-layering:
	@$(SHELL) $(srcdir)/check-layering.sh $(srcdir)

.PHONY: check-layering

# A textual check cannot see a consumer header reached through a permitted one,
# so the recorded dependencies are checked too. They are only complete once the
# objects are built, hence the prerequisite. A dependency file the build has
# never written holds one comment line, which reads the same as a clean answer,
# so an object standing beside one of those is refused first.
#
# Two spellings of one path, because how a dependency is written down is the
# build's choice and not this rule's business: the buildsystem builds out of
# tree and records an absolute path, and a build made inside the tree records
# ../httpd/json.h for the same header. Reading only the first of them would
# leave the rule holding for the way this is built today and for no other.
all-local: check-layering $(LIBRARIES)
	@blind=; for o in *.$(OBJEXT) base/*.$(OBJEXT) box/*.$(OBJEXT) settings/*.$(OBJEXT); do \
		test -f "$$o" || continue; \
		p=`dirname "$$o"`/$(DEPDIR)/`basename "$${o%.$(OBJEXT)}"`.Po; \
		if test ! -f "$$p" || grep -q '^# dummy$$' "$$p"; then blind="$$blind $$o"; fi; \
	done; \
	test -z "$$blind" || { echo "coreapi objects whose dependencies were never recorded:$$blind" >&2; \
		echo "the check below reads nothing for them, build them again" >&2; exit 1; }
	@bad=`cat $(DEPDIR)/*.Po base/$(DEPDIR)/*.Po box/$(DEPDIR)/*.Po settings/$(DEPDIR)/*.Po 2>/dev/null | tr ' ' '\n' | grep -E '(^|/)\.\./(gui|nhttpd|httpd)/|/src/(gui|nhttpd|httpd)/|/lua[^/]*\.h' || true`; \
	test -z "$$bad" || { echo "coreapi pulls a consumer header transitively:" >&2; echo "$$bad" >&2; exit 1; }

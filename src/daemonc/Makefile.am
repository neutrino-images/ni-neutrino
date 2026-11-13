AM_CPPFLAGS = -fno-rtti -fno-exceptions -D__STDC_FORMAT_MACROS

AM_CPPFLAGS += \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/include \
	@AVFORMAT_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	@HWLIB_CFLAGS@ \
	@LUA_CFLAGS@ \
	@SIGC_CFLAGS@

noinst_LIBRARIES = libneutrino_daemonc.a

libneutrino_daemonc_a_SOURCES = remotecontrol.cpp

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/include \
	@FREETYPE_CFLAGS@ \
	@HWLIB_CFLAGS@ \
	@SIGC_CFLAGS@

AM_CPPFLAGS += -fno-rtti -fno-exceptions

noinst_LIBRARIES = libtuxtxt.a

libtuxtxt_a_SOURCES = libtuxtxt.cpp tuxtxt.cpp

configdir = $(CONFIGDIR)/tuxtxt

config_DATA = tuxtxt2.conf

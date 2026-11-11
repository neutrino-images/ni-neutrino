AM_CXXFLAGS = -fno-rtti -fno-exceptions -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS

# The flags src/Makefile.am compiled these files with while it listed them,
# in the same order.
AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src/include \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/xmltree \
	-I$(top_srcdir)/lib/libmd5sum \
	-I$(top_srcdir)/lib/libupnpclient \
	-I$(top_srcdir)/lib/jsoncpp \
	@SIGC_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	@AVFORMAT_CFLAGS@ \
	@LUA_CFLAGS@ \
	@HWLIB_CFLAGS@

AM_CPPFLAGS += -D_FILE_OFFSET_BITS=64

noinst_LIBRARIES = libneutrino_scriptapi.a

libneutrino_scriptapi_a_SOURCES = \
	script_curl.cpp \
	script_filehelpers.cpp \
	script_header.cpp \
	script_hint.cpp \
	script_hintbox.cpp \
	script_hourglass.cpp \
	script_menu.cpp \
	script_messagebox.cpp \
	script_misc.cpp \
	script_neutrino.cpp \
	script_picture.cpp \
	script_progresswindow.cpp \
	script_signalbox.cpp \
	script_stringinput.cpp \
	script_text.cpp \
	script_window.cpp

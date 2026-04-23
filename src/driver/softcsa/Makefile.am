AM_CXXFLAGS = -fno-rtti -fno-exceptions -D__STDC_FORMAT_MACROS

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	@DVBCSA_CFLAGS@ \
	@HWLIB_CFLAGS@ \
	@SIGC_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	@AVFORMAT_CFLAGS@ \
	@LUA_CFLAGS@

noinst_LIBRARIES = libneutrino_driver_softcsa.a

libneutrino_driver_softcsa_a_SOURCES = \
	softcsa_config.cpp \
	softcsa_engine.cpp \
	softcsa_session.cpp \
	softcsa_manager.cpp \
	ts_demuxer.cpp \
	pes_ringbuffer.cpp

AM_CXXFLAGS = -fno-rtti -fno-exceptions -D__STDC_FORMAT_MACROS

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/include \
	@DVBCSA_CFLAGS@ \
	@HWLIB_CFLAGS@

noinst_LIBRARIES = libneutrino_driver_softcsa.a

libneutrino_driver_softcsa_a_SOURCES = \
	softcsa_config.cpp \
	softcsa_engine.cpp \
	softcsa_manager.cpp \
	softcsa_output_fd_set.cpp \
	softcsa_dvr_demux_slot.cpp \
	softcsa_tap_reader.cpp

noinst_LIBRARIES = libhwcaps.a

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/libcoolstream2

libhwcaps_a_SOURCES = \
	hardware_caps.cpp

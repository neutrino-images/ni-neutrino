AM_CPPFLAGS = -fno-rtti -fno-exceptions -D__STDC_FORMAT_MACROS

AM_CPPFLAGS += \
	@HWLIB_CFLAGS@ \
	-I$(top_builddir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/libeventserver \
	@FREETYPE_CFLAGS@

noinst_LIBRARIES = libtimerd.a

libtimerd_a_SOURCES = timerd.cpp timermanager.cpp timerwake.cpp

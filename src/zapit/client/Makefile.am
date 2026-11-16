AM_CPPFLAGS = -fno-rtti -fno-exceptions

AM_CPPFLAGS += \
	-I$(top_builddir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/include

noinst_LIBRARIES = libzapitclient.a
libzapitclient_a_SOURCES = zapitclient.cpp zapittools.cpp

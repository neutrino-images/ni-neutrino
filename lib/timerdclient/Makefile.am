AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/include

AM_CPPFLAGS += -fno-rtti -fno-exceptions

noinst_LIBRARIES  = libtimerdclient.a

libtimerdclient_a_SOURCES = timerdclient.cpp timerdannounce.cpp

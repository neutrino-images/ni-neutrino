AM_CPPFLAGS = \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/luaclient

AM_CPPFLAGS += -fno-rtti -fno-exceptions

bin_PROGRAMS = luaclient

luaclient_SOURCES = luaclient.cpp
luaclient_LDADD = \
	$(top_builddir)/lib/connection/libconnection.a \
	-lOpenThreads \
	-lpthread

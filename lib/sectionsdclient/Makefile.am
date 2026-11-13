AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/include

noinst_LIBRARIES = libsectionsdclient.a

AM_CPPFLAGS += -fno-rtti -fno-exceptions

libsectionsdclient_a_SOURCES = sectionsdclient.cpp

bin_PROGRAMS = sectionsdcontrol

sectionsdcontrol_SOURCES = sectionsdcontrol.cpp
sectionsdcontrol_LDADD = \
	libsectionsdclient.a \
	$(top_builddir)/lib/connection/libconnection.a \
	-lOpenThreads \
	-lpthread

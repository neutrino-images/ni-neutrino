AM_CPPFLAGS = \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/pyclient

AM_CPPFLAGS += -fno-rtti -fno-exceptions

bin_PROGRAMS = pyclient

pyclient_SOURCES = pyclient.cpp
pyclient_LDADD = \
	$(top_builddir)/lib/connection/libconnection.a \
	-lOpenThreads \
	-lpthread

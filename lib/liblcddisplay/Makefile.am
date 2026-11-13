AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir)/src

AM_CPPFLAGS += -fno-rtti -fno-exceptions
	
noinst_LIBRARIES = liblcddisplay.a

liblcddisplay_a_SOURCES = lcddisplay.cpp fontrenderer.cpp

AM_CXXFLAGS = \
	@FREETYPE_CFLAGS@

AM_CPPFLAGS = -fno-rtti -D__STDC_FORMAT_MACROS

AM_CPPFLAGS += \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/xmltree \
	-I$(top_srcdir)/lib/libupnpclient \
	-I$(top_srcdir)/lib/jsoncpp \
	@SIGC_CFLAGS@ \
	@CURL_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	@PYTHON_CFLAGS@ \
	@AVFORMAT_CFLAGS@ \
	@HWLIB_CFLAGS@

noinst_LIBRARIES = libneutrino_gui_python.a

libneutrino_gui_python_a_SOURCES = \
	pyinstance.cpp \
	pyinstance_helpers.cpp \
	py_cc_text.cpp \
	py_cc_window.cpp \
	py_configfile.cpp \
	py_filehelpers.cpp \
	py_hint.cpp \
	py_hintbox.cpp \
	py_hourglass.cpp \
	py_messagebox.cpp \
	py_misc.cpp \
	py_neutrino.cpp \
	py_progresswindow.cpp \
	py_stringinput.cpp

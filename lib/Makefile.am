SUBDIRS = \
	connection \
	libeventserver \
	sectionsdclient \
	timerdclient \
	libconfigfile \
	libmd5sum \
	libnet \
	xmltree \
	jsoncpp \
	libtuxtxt \
	libiw \
	libdvbsub \
	luaclient

# The client for the socket the bindings answer on: a build without them has
# no server to talk to, so it is left out with them.
if ENABLE_PYTHON
SUBDIRS += \
	pyclient
endif

if ENABLE_UPNP
SUBDIRS += \
	libupnpclient
endif

if ENABLE_LCD
SUBDIRS += \
	liblcddisplay
endif

if BOXTYPE_CST
SUBDIRS += \
	hardware/coolstream
endif

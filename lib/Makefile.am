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
	libiw \
	libdvbsub \
	luaclient

if ENABLE_UPNP
SUBDIRS += \
	libupnpclient
endif

if BOXTYPE_COOL
SUBDIRS += \
	libcoolstream \
	libtuxtxt
else
SUBDIRS += \
	libtuxtxt_mp
endif

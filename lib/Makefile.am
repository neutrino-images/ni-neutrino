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

if ENABLE_UPNP
SUBDIRS += \
	libupnpclient
endif

if BOXTYPE_COOL
if BOXMODEL_CS_HD2
SUBDIRS += \
	libcoolstream2
else
SUBDIRS += \
	libcoolstream
endif
endif

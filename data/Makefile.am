SUBDIRS = \
	config \
	control \
	fonts \
	httpd-scripts \
	icons \
	inetradio \
	initial \
	iso-codes \
	license \
	locale \
	logos \
	pictures \
	policy \
	scripts \
	themes \
	weather \
	webradio \
	webtv \
	zapit

# The pages the new server answers with, under the switch that puts that server
# in the program and not under a second one of their own: a box built without
# the server has nobody to serve them, and two switches for one thing is the
# combination somebody lands on that pays the flash for pages it cannot reach.
if ENABLE_NI_WEB
SUBDIRS += \
	ni-web
endif

if ENABLE_LUA
SUBDIRS += \
	luaplugins
endif

if ENABLE_GRAPHLCD
SUBDIRS += \
	oled
endif

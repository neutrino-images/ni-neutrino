# The photographs of the handsets, one per shape of remote control.
#
# Encoded once, from the photographs these replace:
#   cwebp -q 88 -m 6 -sharp_yuv -metadata none <src> -o <name>.webp
# The pixel dimensions are unchanged, because the hot spots in
# app/screens/now/handsets.js are given in the pixels of these pictures.
#
# NOT STORED COMPRESSED, unlike everything under app/. A webp is already a
# compressed stream, so a stored copy is the same size with a step in front of
# it, and this server would inflate it for every caller that took none.
#
# INSTALLED BY MODEL, WHICH IS THE WHOLE POINT OF THE LISTS BELOW. All of them
# together are 370 kB and the smallest image has about a megabyte free, so a
# box that carries the handsets of fifteen other boxes has spent a third of
# that on pictures nobody there can press. What a model may name itself is a
# short list (the hardware library's boxname, joined to its boxvendor), and the
# pictures for those names are what that model gets.
#
# A model with no arm below gets the two at the top and nothing else, and the
# screen then draws the keys rather than a handset. That is the right way round:
# a picture is a promise about a handset somebody is holding.

installdir = $(PRIVATE_HTTPDDIR)/ni-web/rc

# The two that are a setting rather than a model. Any box can be told it is
# being driven by one of the old dbox handsets
# (keybindings.remote_control_hardware), so both go on every box.
install_DATA = \
	rc_dbox_nokia_old.webp \
	rc_dbox_philips.webp

# CST HD1, CST Neo, CST Zee, CST Neo Twin
if BOXMODEL_CST_HD1
install_DATA += \
	rc_cst_v1.webp \
	rc_cst_v2.webp \
	rc_cst_v3.webp \
	rc_cst_v4.webp
endif

# CST Tank, CST Trinity, CST Trinity Duo, CST Zee2, CST Link
if BOXMODEL_CST_HD2
install_DATA += \
	rc_cst_v4.webp \
	rc_cst_v5.webp \
	rc_cst_v6.webp
endif

# AX HD51, Mutant HD51
if BOXMODEL_HD51
install_DATA += \
	rc_ax_hd51.webp \
	rc_mutant_hd51.webp
endif

# AX HD60, Mutant HD60
if BOXMODEL_HD60
install_DATA += \
	rc_ax_hd51.webp \
	rc_mutant_hd60.webp
endif

# AX HD61
if BOXMODEL_HD61
install_DATA += \
	rc_ax_hd61.webp
endif

# Maxytec Multibox 4K
if BOXMODEL_MULTIBOX
install_DATA += \
	rc_mutant_hd60.webp
endif

# Maxytec Multibox SE 4K
if BOXMODEL_MULTIBOXSE
install_DATA += \
	rc_mutant_hd60.webp
endif

# WWIO BRE2ZE 4K
if BOXMODEL_BRE2ZE4K
install_DATA += \
	rc_wwio_bre2ze4k.webp
endif

# AirDigital Zgemma H7
if BOXMODEL_H7
install_DATA += \
	rc_zgemma_h7.webp
endif

# AXAS E4HD 4K ULTRA
if BOXMODEL_E4HDULTRA
install_DATA += \
	rc_axas_e4hd.webp
endif

# Protek 4K UHD
if BOXMODEL_PROTEK4K
install_DATA += \
	rc_protek_4k_uhd.webp
endif

# The whole VU+ range is sold with one handset.
if BOXMODEL_VUSOLO4K
install_DATA += \
	rc_vuplus.webp
endif

if BOXMODEL_VUDUO4K
install_DATA += \
	rc_vuplus.webp
endif

if BOXMODEL_VUDUO4KSE
install_DATA += \
	rc_vuplus.webp
endif

if BOXMODEL_VUULTIMO4K
install_DATA += \
	rc_vuplus.webp
endif

if BOXMODEL_VUUNO4K
install_DATA += \
	rc_vuplus.webp
endif

if BOXMODEL_VUUNO4KSE
install_DATA += \
	rc_vuplus.webp
endif

if BOXMODEL_VUZERO4K
install_DATA += \
	rc_vuplus.webp
endif

if BOXMODEL_VUDUO
install_DATA += \
	rc_vuplus.webp
endif

# Every picture reaches the archive, whichever model this build is for, or a
# release made on one box could not be built for another.
EXTRA_DIST = \
	rc_ax_hd51.webp \
	rc_ax_hd61.webp \
	rc_axas_e4hd.webp \
	rc_cst_v1.webp \
	rc_cst_v2.webp \
	rc_cst_v3.webp \
	rc_cst_v4.webp \
	rc_cst_v5.webp \
	rc_cst_v6.webp \
	rc_dbox_nokia_old.webp \
	rc_dbox_philips.webp \
	rc_mutant_hd51.webp \
	rc_mutant_hd60.webp \
	rc_protek_4k_uhd.webp \
	rc_vuplus.webp \
	rc_wwio_bre2ze4k.webp \
	rc_zgemma_h7.webp

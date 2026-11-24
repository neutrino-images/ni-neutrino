# The client scripts that come with the box, see README.md: one directory per
# language, one directory per script inside it. Every script installs its own
# files, see its Makefile.am.
SUBDIRS =

if ENABLE_LUA
SUBDIRS += \
	luaplugins
endif

if ENABLE_PYTHON
SUBDIRS += \
	pythonplugins
endif

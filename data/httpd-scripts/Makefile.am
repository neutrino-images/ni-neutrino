installdir = $(PRIVATE_HTTPDDIR)/scripts

# The scripts behind the service control of both web servers, in a directory of
# their own rather than inside the old server's pages.
#
# All nineteen services the new API offers run one of these (see the table in
# src/coreapi/shelltable.cpp, which reads them out of PRIVATE_HTTPDDIR/scripts),
# and so does the old server's own page for them. Left where they were, a box
# built without the old pages would have kept the API that names them and lost
# everything it names, and no build would have said a word: the failure would
# have been a service that cannot be started, found by whoever tried.
install_SCRIPTS = \
	_Y_Globals.sh \
	_Y_Library.sh \
	Y_Camds.sh \
	Y_NAS.sh \
	Y_Plugins.sh \
	Y_Tools.sh

# The six names these scripts are written with, resolved here because the
# scripts are here. Six and not the thirty-one the old pages resolve: a
# substitution for a name that does not occur in this directory is a line
# nobody ever holds against the files again, and the day one of these scripts
# starts using a seventh, this list is where it is added.
install-data-hook:
	find $(DESTDIR)$(installdir)/ -type f -print0 | xargs --no-run-if-empty -0 \
		sed -i \
			-e 's|%(PRIVATE_HTTPDDIR)|$(PRIVATE_HTTPDDIR)|g' \
			-e 's|%(CONFIGDIR)|$(CONFIGDIR)|g' \
			-e 's|%(ZAPITDIR)|$(ZAPITDIR)|g' \
			-e 's|%(PLUGINDIR)|$(PLUGINDIR)|g' \
			-e 's|%(PLUGINDIR_VAR)|$(PLUGINDIR_VAR)|g' \
			-e 's|%(PLUGINDIR_MNT)|$(PLUGINDIR_MNT)|g' \
	;

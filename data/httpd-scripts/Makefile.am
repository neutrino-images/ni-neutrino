installdir = $(PRIVATE_HTTPDDIR)/scripts

# The scripts behind the service control of both web servers, in a directory of
# their own rather than inside the old server's pages.
#
# All nineteen services the new API offers run camds.sh or services.sh (see the
# table in src/coreapi/shelltable.cpp, which reads them out of
# PRIVATE_HTTPDDIR/scripts). Left where they were, a box built without the old
# pages would have kept the API that names them and lost everything it names,
# and no build would have said a word: the failure would have been a service
# that cannot be started, found by whoever tried.
#
# Neither script carries a %(NAME) placeholder, so they are installed as they
# are. The day one of them needs a build path, it gets an install-data-hook
# that substitutes that one name and no other.
install_SCRIPTS = \
	camds.sh \
	services.sh

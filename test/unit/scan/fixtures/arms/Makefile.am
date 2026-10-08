# Read by the self test of the DERIVED arms column only, never by automake.
fixture_SOURCES = site-amtwice.cpp
if SCAN_ARMS_OPTION
fixture_SOURCES += site-built-under.cpp site-amtwice.cpp site-amtwo.cpp
endif
if SCAN_ARMS_OTHER
fixture_SOURCES += site-built-other.cpp site-amtwo.cpp
endif

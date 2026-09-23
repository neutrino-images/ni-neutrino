# Test binaries run on the build machine, so only a native build may enter test/.
if ENABLE_UNIT_TESTS
TESTDIR = test
endif

SUBDIRS = lib src data $(TESTDIR)

# A build without the tests says so when somebody asks it to check.
#
# Without this, make check in such a build walks lib, src and data, finds no
# check target in any of them, and reports success. Nothing ran, the exit code
# is zero, and there is no line anywhere saying which of those two happened.
# That has already been read as a passing suite once, by somebody who had
# configured a second build directory for a different purpose and then measured
# in the first one.
#
# Not an error, because a cross build has no business building host binaries and
# this is the ordinary shape of one. It is a sentence, on the error stream, at
# the moment the wrong conclusion would otherwise be drawn.
if !ENABLE_UNIT_TESTS
check-local:
	@echo "This build was configured without --enable-unit-tests." >&2
	@echo "Nothing under test/ was built and nothing was run. A zero exit here" >&2
	@echo "says the tests were skipped, not that they passed." >&2
endif

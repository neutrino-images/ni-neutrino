# A plugin whose import list is written over several lines, with a name the
# module does not carry: the scan reads a bracketed list to its closing line,
# so this has to be found where a one-line import beside it would be.
from neutrino import (
	curl,
	nosuchbinding,
)

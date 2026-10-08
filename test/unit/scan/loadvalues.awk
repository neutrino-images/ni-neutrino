# The defaults the load names by a key code, as entries of a table the test compiles,
# so that the compiler gives each name its number: the scan reads only plain numbers
# and the key codes are an enumeration. Reads the table extract-pairs.sh -b prints.
# Only the key codes, because those are what a model changes; the other names the
# load uses sit in screen headers a unit test cannot include.
BEGIN { FS = "\t" }
$3 == "expr" {
	e = $4
	sub(/^\(int(32_t)?\)[ ]*/, "", e)
	if (e !~ /^CRCInput::RC_[A-Za-z_0-9]+$/)
		next
	printf "\t{ \"%s\", (long) (%s) },\n", $1, e
}

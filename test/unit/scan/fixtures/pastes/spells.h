/* Each definition could spell a build condition, so pastes.awk names this file
   once for any of them; the case below runs it over each on its own. */
#define PASTE_PREFIX(x) HAVE_##x
#define PASTE_SUFFIX(x) x##_HARDWARE
#define PASTE_BOTH(x, y) x##y
#define PASTE_SPLIT(p) p ## _red \
	+ ENA ## BLE_FOO
#define PASTE_QUOTED(x) "a" ## x

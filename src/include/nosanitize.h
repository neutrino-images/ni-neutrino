#ifndef __neutrino_sanitize_h__
#define __neutrino_sanitize_h__

#ifndef ATTRIBUTE_NO_SANITIZE_ADDRESS
# if defined(__clang__) || defined (__GNUC__)
#  define ATTRIBUTE_NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))
# else
#  define ATTRIBUTE_NO_SANITIZE_ADDRESS
# endif
#endif

#endif /* __neutrino_sanitize_h__ */

//=============================================================================
// The old server's helpers, as they are, for the answers that are built on
// them. Copied out of yhttpd_core/helper.h rather than rewritten: every one of
// these decides bytes a consumer already reads.
//
// yExecuteScript is not here: it is the one helper that reaches up into
// controlapi.h for PLUGIN_DIRS, and this file is used by cases that have
// nothing to do with that. See scriptrunner.h.
//=============================================================================

#ifndef __httpd_compat_helper_h__
#define __httpd_compat_helper_h__

#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace httpd
{
namespace compat
{

// The two containers the old server passes around. Named as it names them,
// because the handlers this layer is built for declare locals of these types.
typedef std::map<std::string, std::string> CStringList;
typedef std::vector<std::string> CStringArray;

//-----------------------------------------------------------------------------
int minmax(int value, int min, int max);
void correctTime(struct tm *zt);

//-----------------------------------------------------------------------------
// String Converter
//-----------------------------------------------------------------------------
std::string itoa(unsigned int conv);
std::string itoh(unsigned int conv);
std::string decodeString(std::string encodedString);
std::string encodeString(const std::string &decodedString);
std::string string_tolower(std::string str);

//-----------------------------------------------------------------------------
// String Helpers
//-----------------------------------------------------------------------------
std::string trim(std::string const &source, char const *delims = " \t\r\n");
void replace(std::string &str, const std::string &find_what, const std::string &replace_with);
std::string string_printf(const char *fmt, ...);
bool ySplitString(std::string str, std::string delimiter, std::string &left, std::string &right);
bool ySplitStringExact(std::string str, std::string delimiter, std::string &left, std::string &right);
bool ySplitStringLast(std::string str, std::string delimiter, std::string &left, std::string &right);
CStringArray ySplitStringVector(std::string str, std::string delimiter);
bool nocase_compare(char c1, char c2);
std::string timeString(time_t time);
bool write_to_file(std::string filename, std::string content, bool append = false);

//-----------------------------------------------------------------------------
// JSON Helpers
//-----------------------------------------------------------------------------
std::string json_out_quote_convert(std::string _str);
std::string json_out_pair(std::string _key, std::string _value);
std::string json_out_success(std::string _result);
std::string json_out_error(std::string _error);
std::string json_convert_string(std::string s);
/// Returns true if ch is a control character (in range [0,32]).
static inline bool isControlCharacter(char ch) { return ch > 0 && ch <= 0x1F;}

} // namespace compat
} // namespace httpd

#endif /* __httpd_compat_helper_h__ */

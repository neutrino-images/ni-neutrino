//=============================================================================
// The old server's query parser. See query.h.
//=============================================================================

#include "httpd/compat/query.h"

#include <cctype>

namespace httpd
{
namespace compat
{

//-----------------------------------------------------------------------------
// parse parameter string
//       parameter               = attribute "=" value
//       attribute               = token
//       value                   = token | quoted-string
//
// 	If parameter attribute is multiple times given, the values are stored like this:
// 		<attribute>=<value1>,<value2>,..,<value n>
//
// The original also runs each parsed pair through the old server's own
// logging singleton (log_level_printf). Nothing downstream reads that log,
// and pulling its class in would tie this layer to a global instance it has
// no other reason to touch, so the call is left out; ParamList ends up
// holding the same bytes either way.
//-----------------------------------------------------------------------------
bool parseParams(std::string param_string, CStringList &ParamList) {
	bool ende = false;
	std::string param, name = "", value = "", number = "";

	while (!ende) {
		if (!ySplitStringExact(param_string, "&", param, param_string))
			ende = true;
		if (ySplitStringExact(param, "=", name, value)) {
			name = decodeString(name);
			value = trim(decodeString(value));
			if (ParamList[name].empty())
				ParamList[name] = value;
			else {
				ParamList[name] += ",";
				ParamList[name] += value;
			}
		}
		else
			name = trim(decodeString(name));
		// size() is size_t, four bytes on the target and eight on the host
		// that builds this suite; the cast keeps the value handed to "%d" the
		// width string_printf's va_arg read expects on either one.
		number = string_printf("%d", (int)(ParamList.size() + 1));
		ParamList[number] = name;
	}
	return true;
}

//-----------------------------------------------------------------------------
bool isPositionKey(const std::string &key)
{
	if (key.empty())
		return false;
	for (std::string::const_iterator c = key.begin(); c != key.end(); ++c)
		if (!std::isdigit(static_cast<unsigned char>(*c)))
			return false;
	return true;
}

//-----------------------------------------------------------------------------
// Analyze URL
// Extract Filename, Path, FileExt
//	form RFC2616 / 3.2.2:
//	http_URL = "http:" "//" host [ ":" port ] [ abs_path [ "?" query ]]
// query data is splitted and stored in ParamList
//-----------------------------------------------------------------------------
void analyzeURL(const std::string &url, CStringList &UrlData, CStringList &ParamList) {
	std::string fullurl = "";
	if (!ParamList.empty())
		ParamList.clear();

	// URI decode
	fullurl = decodeString(url);
	fullurl = trim(fullurl, "\r\n"); // non-HTTP-Standard: allow \r or \n in URL. Delete it.
	UrlData["fullurl"] = fullurl;

	// split Params
	if (ySplitString(url, "?", UrlData["url"], UrlData["paramstring"])) { // split pure URL and all Params
		UrlData["url"] = decodeString(UrlData["url"]);
		parseParams(UrlData["paramstring"], ParamList); // split params to ParamList
	} else {
		// No Params
		UrlData["url"] = fullurl;
	}

	if (!ySplitStringLast(UrlData["url"], "/", UrlData["path"],
			UrlData["filename"])) {
		UrlData["path"] = "/"; // Set "/" if not contained
	} else
		UrlData["path"] += "/";
	if ((UrlData["url"].length() == 1)
			|| (UrlData["url"][UrlData["url"].length() - 1] == '/')) { // if "/" at end use index.html
		UrlData["path"] = UrlData["url"];
		UrlData["filename"] = "index.html";
	}
	ySplitStringLast(UrlData["filename"], ".", UrlData["filenamepure"],
			UrlData["fileext"]);
}

} // namespace compat
} // namespace httpd

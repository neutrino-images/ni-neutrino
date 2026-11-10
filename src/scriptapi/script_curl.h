/*
 * the download both bindings make
 *
 * The Lua binding in lua_curl.cpp and the Python one in py_curl.cpp held the
 * same 42 libcurl options and the same report, twice over. Here it stands
 * once: the bindings read their own syntax - a table, keyword arguments - fill
 * the parameters, and hand them over.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SCRIPT_CURL_H
#define _SCRIPT_CURL_H

#include <string>
#include <vector>

#include <curl/curl.h>

/* What a download wants, gathered before it starts. The defaults are the ones
 * both bindings had. */
struct ScriptCurlParams
{
	std::string url;             /* required */
	std::string outputfile;      /* empty: the body comes back as the answer */
	std::string userAgent;
	std::string postfields;      /* data to POST to the server */
	bool verbose;
	bool silent;
	bool pass_header;            /* headers into the data stream */
	std::vector<std::string> httpheader;
	long connectTimeout;
	bool ipv4;
	bool ipv6;
	bool useProxy;
	bool followRedir;
	long maxRedirs;
	bool redirectOnly;

	ScriptCurlParams();
};

struct progressData
{
	CURL *curl;
	curl_off_t last_dlnow;
};

class ScriptCurl
{
	public:
		enum {
			OK              = 0,
			ERR_HANDLE      = 1,
			ERR_NO_URL      = 2,
			ERR_CREATE_FILE = 3,
			ERR_CURL        = 4
		};

		/* Answers one of the codes above. In answer stands, by the code: the
		 * body, or the message that says what failed, or, with redirectOnly,
		 * the url it ended at. */
		static int Download(const ScriptCurlParams &params, std::string &answer);

		/* encodeUri / decodeUri */
		static std::string UriInternal(const std::string &data, bool decode);

		/* setUriData(name, value, name, value, ...): the query string they
		 * make. A name that is empty is left out - and leaves its "&" behind,
		 * which is what both bindings did. */
		static std::string QueryString(const std::vector<std::string> &namesAndValues);

		static size_t WriteToString(void *ptr, size_t size, size_t nmemb, void *data);
		static int ProgressFunc(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
#if !CURL_AT_LEAST_VERSION( 7,32,0 )
		static int ProgressFuncOld(void *p, double dltotal, double dlnow, double ultotal, double ulnow);
#endif
};

#endif //_SCRIPT_CURL_H

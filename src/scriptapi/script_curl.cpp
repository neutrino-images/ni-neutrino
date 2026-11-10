/*
 * the download both bindings make
 *
 * (C) 2015 M. Liebmann (micha-bbg), and the two bindings this stands in for.
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

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <system/debug.h>
#include <system/helpers.h>
#include <system/settings.h>
#include <neutrino.h>

#include <curl/curl.h>
#include <curl/easy.h>

#include "script_curl.h"

#define CURL_MSG_ERROR "[curl:download \33[1;31mERROR!\33[0m]"

ScriptCurlParams::ScriptCurlParams()
	: url(""),
	  outputfile(""),
	  userAgent(""),
	  postfields(""),
	  verbose(false),
	  silent(false),
	  pass_header(false),
	  connectTimeout(20),
	  ipv4(false),
	  ipv6(false),
	  useProxy(true),
	  followRedir(true),
	  maxRedirs(20),
	  redirectOnly(false)
{
}

size_t ScriptCurl::WriteToString(void *ptr, size_t size, size_t nmemb, void *data)
{
	if (size * nmemb > 0) {
		std::string *pStr = (std::string *)data;
		pStr->append((char *)ptr, nmemb);
	}
	return size * nmemb;
}

/* The bar both bindings drew while a download runs. */
int ScriptCurl::ProgressFunc(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
{
	if (dltotal == 0)
		return 0;

	struct progressData *_pgd = static_cast<struct progressData *>(p);
	if (_pgd->last_dlnow == dlnow)
		return 0;
	_pgd->last_dlnow = dlnow;

	double dlSpeed;
	long responseCode = 0;
#if CURL_AT_LEAST_VERSION( 7,55,0 )
	curl_easy_getinfo(_pgd->curl, CURLINFO_SPEED_DOWNLOAD_T, &dlSpeed);
#else
	curl_easy_getinfo(_pgd->curl, CURLINFO_SPEED_DOWNLOAD, &dlSpeed);
#endif
	curl_easy_getinfo(_pgd->curl, CURLINFO_RESPONSE_CODE, &responseCode);

	uint32_t MUL = 0x7FFF;
	uint32_t dlFragment = (uint32_t)((dlnow * MUL) / dltotal);
	if (responseCode != 200) {
		dlFragment = 0;
		dlSpeed    = 0;
	}

	int showDots = 50;
	int dots = (dlFragment * showDots) / MUL;
	printf(" %d%% [", (dlFragment * 100) / MUL);
	int i = 0;
	for (; i < dots-1; i++)
		printf("=");
	if (i < showDots) {
		printf(">");
		i++;
	}
	for (; i < showDots; i++)
		printf(" ");
	printf("] speed: %.03f KB/sec     \r", dlSpeed/1024); fflush(stdout);
	return 0;
}

#if !CURL_AT_LEAST_VERSION( 7,32,0 )
int ScriptCurl::ProgressFuncOld(void *p, double dltotal, double dlnow, double ultotal, double ulnow)
{
	return ProgressFunc(p, (curl_off_t)dltotal, (curl_off_t)dlnow, (curl_off_t)ultotal, (curl_off_t)ulnow);
}
#endif

std::string ScriptCurl::UriInternal(const std::string &data, bool decode)
{
	std::string retString = "";
	CURL *curl_handle = curl_easy_init();
	if (curl_handle) {
		int outlength;
		char *pfTmp;
		if (decode)
			pfTmp = curl_easy_unescape(curl_handle, data.c_str(), data.length(), &outlength);
		else
			pfTmp = curl_easy_escape(curl_handle, data.c_str(), data.length());
		if (pfTmp) {
			retString = static_cast<std::string>(pfTmp);
			curl_free(pfTmp);
		}
		curl_easy_cleanup(curl_handle);
	}
	return retString;
}

std::string ScriptCurl::QueryString(const std::vector<std::string> &namesAndValues)
{
	std::string retString = "";
	for (size_t i = 0; i + 1 < namesAndValues.size(); i += 2) {
		if (namesAndValues[i].empty())
			continue;
		if (i > 0)
			retString += "&";
		retString += namesAndValues[i] + "=";
		retString += UriInternal(namesAndValues[i+1], false);
	}
	return retString;
}

int ScriptCurl::Download(const ScriptCurlParams &params, std::string &answer)
{
	char errMsg[1024] = {0};

	CURL *curl_handle = curl_easy_init();
	if (!curl_handle) {
		snprintf(errMsg, sizeof(errMsg) - 1, "error creating cUrl handle.");
		printf("%s %s\n", CURL_MSG_ERROR, errMsg);
		answer = errMsg;
		return ERR_HANDLE;
	}

	if (params.url.empty()) {
		curl_easy_cleanup(curl_handle);
		snprintf(errMsg, sizeof(errMsg) - 1, "no url given.");
		printf("%s %s\n", CURL_MSG_ERROR, errMsg);
		answer = errMsg;
		return ERR_NO_URL;
	}

	const std::string url = params.url;
	const std::string outputfile = params.outputfile;
	const bool toFile = !outputfile.empty();
	std::string retString = "";
	FILE *fp = NULL;
	if (toFile) {
		fp = fopen(outputfile.c_str(), "wb");
		if (fp == NULL) {
			snprintf(errMsg, sizeof(errMsg) - 1, "Can't create %s", outputfile.c_str());
			printf("%s %s\n", CURL_MSG_ERROR, errMsg);
			curl_easy_cleanup(curl_handle);
			answer = errMsg;
			return ERR_CREATE_FILE;
		}
	}

	const std::string userAgent = params.userAgent;
	const std::string postfields = params.postfields;
	const bool verbose = params.verbose;
	const bool silent = params.silent;
	const bool pass_header = params.pass_header;
	const long connectTimeout = params.connectTimeout;
	const long maxRedirs = params.maxRedirs;
	const bool followRedir = params.followRedir;
	const bool useProxy = params.useProxy;
	const bool redirectOnly = params.redirectOnly;

	/* the custom set of headers */
	curl_slist *hlist = NULL;
	for (size_t i = 0; i < params.httpheader.size(); i++)
		hlist = curl_slist_append(hlist, params.httpheader[i].c_str());

	curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
	if (toFile) {
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, NULL);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, fp);
	}
	else {
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, &ScriptCurl::WriteToString);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&retString);
	}

	curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, (long)(4*connectTimeout));
	curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, (long)connectTimeout);
	curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
	/* enable all supported built-in compressions */
	curl_easy_setopt(curl_handle, CURLOPT_ACCEPT_ENCODING, "");
	curl_easy_setopt(curl_handle, CURLOPT_COOKIE, "");
	if (!userAgent.empty())
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, userAgent.c_str());

	if (!postfields.empty()) {
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(postfields.length()));
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, postfields.c_str());
	}

	if (params.ipv4 && params.ipv6)
		curl_easy_setopt(curl_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);
	else if (params.ipv4)
		curl_easy_setopt(curl_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	else if (params.ipv6)
		curl_easy_setopt(curl_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);

	/* Copied under the lock: a web channel's script runs this on the stream
	   server's thread as well, while the box's loop may assign to these. */
	const std::string proxy_server = settingsText(g_settings.softupdate_proxyserver);
	if (!proxy_server.empty() && useProxy) {
		curl_easy_setopt(curl_handle, CURLOPT_PROXY, proxy_server.c_str());
		const std::string proxy_user = settingsText(g_settings.softupdate_proxyusername);
		if (!proxy_user.empty()) {
			std::string tmp = proxy_user + ":" + settingsText(g_settings.softupdate_proxypassword);
			curl_easy_setopt(curl_handle, CURLOPT_PROXYUSERPWD, tmp.c_str());
		}
	}

	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, (followRedir)?1L:0L);
	curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, (long)maxRedirs);
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, (silent)?1L:0L);
	curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, (verbose)?1L:0L);
	curl_easy_setopt(curl_handle, CURLOPT_HEADER, (pass_header)?1L:0L);
	if (hlist) {
		/* set our custom set of headers */
		curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, hlist);
	}

	if (!silent) {
		progressData pgd;
		pgd.curl = curl_handle;
		/* -1, where the Python binding stood at 0: the first frame is drawn
		   either way, and this is the one the Lua binding drew. */
		pgd.last_dlnow = -1;
#if CURL_AT_LEAST_VERSION( 7,32,0 )
		curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, ScriptCurl::ProgressFunc);
		curl_easy_setopt(curl_handle, CURLOPT_XFERINFODATA, &pgd);
#else
		curl_easy_setopt(curl_handle, CURLOPT_PROGRESSFUNCTION, ScriptCurl::ProgressFuncOld);
		curl_easy_setopt(curl_handle, CURLOPT_PROGRESSDATA, &pgd);
#endif
	}

	char cerror[CURL_ERROR_SIZE] = {0};
	curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, cerror);

	printf("\n[curl:download] download %s => %s\n", url.c_str(), (toFile)?outputfile.c_str():"return string");
	if (!silent)
		printf("\e[?25l"); /* cursor off */
	printf("\n");
	CURLcode ret = curl_easy_perform(curl_handle);
	if (!silent)
		printf("\e[?25h"); /* cursor on */
	printf("\n");

	std::string redirectUrl;
	/* not part of the report below: silent must not switch redirectOnly off */
	if (redirectOnly) {
		char *effective = NULL;
		if ((curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &effective) == CURLE_OK) && effective)
			redirectUrl = effective;
	}
	std::string msg;
	if (!silent) {
		double dsize, dtime;
		char *dredirect = NULL, *deffektive = NULL;
#if CURL_AT_LEAST_VERSION( 7,55,0 )
		curl_easy_getinfo(curl_handle, CURLINFO_SIZE_DOWNLOAD_T, &dsize);
#else
		curl_easy_getinfo(curl_handle, CURLINFO_SIZE_DOWNLOAD, &dsize);
#endif
		curl_easy_getinfo(curl_handle, CURLINFO_TOTAL_TIME, &dtime);
		CURLcode res1 = curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &deffektive);
		CURLcode res2 = curl_easy_getinfo(curl_handle, CURLINFO_REDIRECT_URL, &dredirect);

		char msg1[1024] = {0};
		snprintf(msg1, sizeof(msg1)-1, "\n[curl:download] O.K. size: %.0f bytes, time: %.02f sec.", dsize, dtime);
		msg = msg1;
		if (toFile)
			msg += std::string("\n\t\t     file: ") + outputfile;
		else
			msg += std::string("\n\t\t   output: return string");
		msg += std::string("\n\t\t      url: ") + url;
		if ((res1 == CURLE_OK) && deffektive) {
			std::string tmp1 = std::string(deffektive);
			std::string tmp2 = url;
			if (trim(tmp1, " /") != trim(tmp2, " /"))
				msg += std::string("\n\t    effektive url: ") + deffektive;
		}
		if ((res2 == CURLE_OK) && dredirect)
			msg += std::string("\n\t      redirect to: ") + dredirect;
	}
	if (hlist)
		curl_slist_free_all(hlist);
	curl_easy_cleanup(curl_handle);
	if (toFile)
		fclose(fp);

	if (ret != 0) {
		snprintf(errMsg, sizeof(errMsg) - 1, "%s", cerror);
		printf("%s curl error: %s\n", CURL_MSG_ERROR, errMsg);
		if (toFile)
			unlink(outputfile.c_str());
		answer = errMsg;
		return ERR_CURL;
	}

	if (redirectOnly && !redirectUrl.empty()) {
		answer = redirectUrl;
		return OK;
	}

	if (verbose == true)
		printf("%s\n \n", msg.c_str());

	answer = retString;
	return OK;
}

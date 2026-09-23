//=============================================================================
// See levels.h. The table below is read off controlapi.cpp by hand, not
// derived from it: every row's kind is the answer to "can any parameter this
// name accepts change something", checked against that name's own handler
// body, and the report for the task that added this file names the line in
// each handler the answer came from.
//=============================================================================

#include "httpd/compat/levels.h"

#include "httpd/compat/query.h"
#include "httpd/credentials.h"
#include "httpd/webconfig.h"

#include <cstddef>
#include <string>

namespace httpd
{
namespace compat
{

namespace
{

enum Kind
{
	KindRead,
	KindWrite,
	KindSystem,
	KindMixed
};

// An aggregate with no constructor, for the reason every other table in
// this tree gives: this is read only memory, and nothing here needs to have
// run before main.
struct Row
{
	const char *name;
	Kind        kind;
};

/* The 74 names yCgiCallList carries, each with the kind its own handler
   earns it. exec, system, startplugin and yweb are System without exception
   and without Mixed: they run a command, a script or a plugin, and no
   parameter on any of them only reads. build_live_url and build_playlist
   write a file under /tmp as a side effect but are kept at Read: they are
   the two endpoints spec 6.6 files under "streaming URLs", meant for a
   player outside this box entirely, and 6.6's own bucket for that is Read. */
const Row kTable[] =
{
	{ "getservicesxml",    KindRead },
	{ "getbouquetsxml",    KindRead },
	{ "getubouquetsxml",   KindRead },
	{ "channellist",       KindRead },
	{ "logolist",          KindRead },
	{ "getbouquet",        KindRead },
	{ "getchannel",        KindRead },
	{ "getbouquets",       KindRead },
	{ "getmode",           KindRead },
	{ "setmode",           KindMixed },
	{ "epgsearchxml",      KindRead },
	{ "epgsearch",         KindRead },
	{ "epg",               KindRead },
	{ "zapto",             KindMixed },
	{ "signal",            KindRead },
	{ "getonidsid",        KindRead },
	{ "getchannelid",      KindRead },
	{ "getchannelinfo",    KindRead },
	{ "getepgid",          KindRead },
	{ "currenttpchannels", KindRead },
	{ "standby",           KindMixed },
	{ "shutdown",          KindSystem },
	{ "reboot",            KindSystem },
	{ "restart",           KindSystem },
	{ "getdate",           KindRead },
	{ "gettime",           KindRead },
	{ "info",              KindRead },
	{ "boxinfo",           KindRead },
	{ "version",           KindRead },
	{ "reloadsetup",       KindWrite },
	{ "reloadplugins",     KindWrite },
	{ "reloadchannels",    KindWrite },
	{ "screenshot",        KindWrite },
	{ "glcdscreenshot",    KindWrite },
	{ "volume",            KindMixed },
	{ "lcd",               KindRead },
	{ "system",            KindSystem },
	{ "message",           KindWrite },
	{ "rc",                KindMixed },
	{ "rcem",              KindWrite },
	{ "startplugin",       KindSystem },
	{ "exec",              KindSystem },
	{ "yweb",              KindSystem },
	{ "aspectratio",       KindRead },
	{ "videoformat",       KindMixed },
	{ "videooutput",       KindRead },
	{ "vcroutput",         KindRead },
	{ "avinputmode",       KindRead },
	{ "audio",             KindRead },
	{ "crypt",             KindRead },
	{ "timer",             KindMixed },
	{ "sendalltimers",     KindWrite },
	{ "setbouquet",        KindWrite },
	{ "savebouquet",       KindWrite },
	{ "movebouquet",       KindWrite },
	{ "deletebouquet",     KindWrite },
	{ "addbouquet",        KindWrite },
	{ "renamebouquet",     KindWrite },
	{ "changebouquet",     KindWrite },
	{ "updatebouquet",     KindWrite },
	{ "xmltv.data",        KindRead },
	{ "xmltv.xml",         KindRead },
	{ "xmltv.m3u",         KindRead },
	{ "xmltvlist",         KindWrite },
	{ "build_live_url",    KindRead },
	{ "build_playlist",    KindRead },
	{ "get_logo",          KindRead },
	{ "config",            KindMixed },
	{ "file",              KindMixed },
	{ "statfs",            KindRead },
	{ "getdir",            KindRead },
	{ "getmovies",         KindRead },
	{ "infoicons",         KindMixed },
	{ "updateinfo",        KindRead }
};

const size_t kTableCount = sizeof(kTable) / sizeof(kTable[0]);

const Row *find(const std::string &name)
{
	for (size_t i = 0; i < kTableCount; ++i)
	{
		if (name == kTable[i].name)
			return &kTable[i];
	}
	return NULL;
}

/* The ten Mixed names, each read the same way its own handler reads
   ParamList: out of the very list that handler will be given, rather than
   through a second reading of the request that could disagree with it about
   what a bare token, a repeated name or a POST body means (query.cpp,
   mount.cpp).

   Taken by value because the ten rules below read it the way a handler does,
   with operator[], and that inserts. A copy of a map with a handful of short
   strings in it, once per gate, is the cheaper of the two prices: the other
   is ten rules written differently here from the way the handler they stand
   in for writes them.

   Every rule below is a whitelist of what still only reads; a form that
   matches nothing here answers Write, not because every unmatched form
   mutates something today - several of them are dead branches a handler
   answers with an unconditional ok or error - but because the table this
   function backs is a floor callers are held to, and a floor that widened
   itself every time a handler's own fallthrough happened to be inert would
   stop being a floor the moment that fallthrough gained a body. */
AuthLevel mixedLevel(const std::string &name, CStringList p)
{
	if (p.empty())
	{
		// Every Mixed handler's own empty-ParamList branch only reports:
		// TimerCGI's is the same list a query naming format= also reaches.
		return AuthLevel::Read;
	}

	if (name == "standby")
	{
		return (p["1"] == "on" || p["1"] == "off") ? AuthLevel::Write : AuthLevel::Read;
	}

	if (name == "setmode")
	{
		return (p["1"] == "radio" || p["1"] == "tv" ||
		        p["record"] == "start" || p["record"] == "stop")
		               ? AuthLevel::Write : AuthLevel::Read;
	}

	if (name == "volume")
	{
		return (!p["1"].empty() && p["1"] != "status") ? AuthLevel::Write : AuthLevel::Read;
	}

	if (name == "rc")
	{
		return (p["1"] == "lock" || p["1"] == "unlock") ? AuthLevel::Write : AuthLevel::Read;
	}

	if (name == "videoformat")
	{
		return (p["1"] != "status") ? AuthLevel::Write : AuthLevel::Read;
	}

	if (name == "zapto")
	{
		const std::string &first = p["1"];
		if (first == "getpids" || first == "getallpids" || first == "statusplayback" ||
		    first == "statussectionsd" || first == "getallsubchannels")
			return AuthLevel::Read;
		return AuthLevel::Write;
	}

	if (name == "timer")
	{
		if (!p["format"].empty())
			return AuthLevel::Read;
		const std::string &action = p["action"];
		return (action == "new" || action == "modify" || action == "remove")
		               ? AuthLevel::Write : AuthLevel::Read;
	}

	if (name == "config")
	{
		const std::string &c = p["config"];
		const bool named = (c == "neutrino" || c == "moviebrowser" ||
		                    c == "nhttpd" || c == "yweb");
		return (named && p["action"] == "submit") ? AuthLevel::Write : AuthLevel::Read;
	}

	if (name == "file")
	{
		const std::string &action = p["action"];
		return (action == "new_folder" || action == "delete") ? AuthLevel::Write : AuthLevel::Read;
	}

	if (name == "infoicons")
	{
		const std::string &first = p["1"];
		return (first == "static" || first == "popup" ||
		        first == "infoviewer" || first == "off")
		               ? AuthLevel::Write : AuthLevel::Read;
	}

	// Every Mixed row in kTable is named above; hasLevel and this function
	// agreeing on that set is what "every endpoint in the table has a
	// level" (test_compat_levels.cpp) actually exercises. Reached only if
	// the two ever disagree, and refuses rather than guesses when they do.
	return AuthLevel::System;
}

/* The list a request carrying only a query leaves behind, built through the
   one parser that builds it anywhere else (query.cpp). Empty for an empty
   query rather than parseParams("")'s own one-entry list, because that is
   what analyzeURL leaves for a request with no "?" in it at all, and what
   mount.cpp hands this file for one. */
CStringList paramsOfQuery(const std::string &query)
{
	CStringList params;
	if (!query.empty())
		parseParams(query, params);
	return params;
}

bool basicIsValid(const std::string &basic)
{
	if (basic.empty())
		return false;

	const size_t colon = basic.find(':');
	const std::string user = basic.substr(0, colon);
	const std::string pass = (colon == std::string::npos) ? std::string() : basic.substr(colon + 1);

	const WebConfig &cfg = config();

	/* Both read whatever the box holds and neither is allowed to settle the
	   answer alone, the same reason ep_auth.cpp's login route keeps them
	   apart: a name that answers in no time and a password that answers in
	   the time a derivation takes would be a way of asking this box whether
	   the name it was given exists. */
	const bool name_matches = !cfg.username.empty() && user == cfg.username;
	const bool secret_matches = verifySecret(pass, cfg.password_hash);
	return name_matches && secret_matches;
}

} // namespace

size_t endpointCount()
{
	return kTableCount;
}

const char *endpointName(size_t index)
{
	return kTable[index].name;
}

bool hasLevel(const std::string &name)
{
	return find(name) != NULL;
}

AuthLevel levelFor(const std::string &name, const CStringList &params)
{
	const Row *row = find(name);
	if (row == NULL)
		return AuthLevel::System;

	switch (row->kind)
	{
		case KindRead:   return AuthLevel::Read;
		case KindWrite:  return AuthLevel::Write;
		case KindSystem: return AuthLevel::System;
		case KindMixed:  return mixedLevel(name, params);
	}

	// Unreachable: the switch above names every Kind there is, and
	// -Werror=switch is what keeps that true the moment a fifth one is
	// added without a case here.
	return AuthLevel::System;
}

AuthLevel levelFor(const std::string &name, const std::string &query)
{
	return levelFor(name, paramsOfQuery(query));
}

bool legacyAllows(const std::string &name, const CStringList &params,
                   AuthLevel have, const Credentials &c)
{
	if (isLoopbackPeer(c.peer))
		return true;

	const AuthLevel need = levelFor(name, params);

	AuthLevel effective = have;

	/* The session comes back off (see levels.h). Written as a floor and not
	   as an assignment so that this can only ever take away: whatever else
	   raised this caller, a request that also carries a cookie must not come
	   out of here holding more than it would have held without one.

	   The two conditions are a cost guard and not the policy. A cookie that
	   names no live session granted nothing in the first place, so asking
	   again would arrive at the same number; what asking would cost is a
	   second derivation for a caller who also holds a bearer token. Nothing
	   answers differently with them removed, which is why there is no case
	   here for them. */
	if (!c.cookie_token.empty() && sessionIsLive(c.cookie_token))
	{
		const AuthLevel without = grantedWithoutSession(c);
		if ((int) without < (int) effective)
			effective = without;
	}

	if (basicIsValid(c.basic))
		effective = AuthLevel::System;

	return (int) effective >= (int) need;
}

bool legacyAllows(const std::string &name, const std::string &query,
                   AuthLevel have, const Credentials &c)
{
	return legacyAllows(name, paramsOfQuery(query), have, c);
}

} // namespace compat
} // namespace httpd

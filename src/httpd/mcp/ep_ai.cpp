/*
 * ep_ai.cpp - the routes the KI tab reads and writes
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "httpd/mcp/ep_ai.h"

#include "httpd/auth.h"
#include "httpd/credentials.h"
#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/mcp/aiguides.h"
#include "httpd/mcp/allowlist.h"
#include "httpd/mcp/exposure.h"
#include "httpd/netmatch.h"
#include "httpd/oauth/twofactor.h"
#include "httpd/schema.h"
#include "httpd/status.h"
#include "httpd/webconfig.h"

#include "coreapi/base/errors.h"
#include "coreapi/plugins.h"
#include "coreapi/settings/settings.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

namespace httpd
{

namespace exposure
{

namespace
{

std::string mcpUrlOf(const std::string &public_url)
{
	return public_url.empty() ? std::string() : public_url + "/mcp";
}

// The router drops a member sent as ""; here "" means clear.
bool namedInBody(const Request &r, const char *name)
{
	std::vector<JsonMember> members;
	if (!readFlatObject(r.body(), members))
		return false;
	for (size_t i = 0; i < members.size(); ++i)
	{
		if (members[i].name == name)
			return true;
	}
	return false;
}

void writeAi(Json &j, const AiSettings &s)
{
	j.beginObject();
	j.key("enabled");
	j.value(s.enabled);
	j.key("public_url");
	j.value(s.public_url);
	j.key("trusted_proxies");
	j.value(trustedProxyText(s.trusted_proxies));
	j.key("allow_lan");
	j.value(s.allow_lan);
	j.key("mcp_url");
	j.value(mcpUrlOf(s.public_url));
	j.key("tunnel_paths");
	j.beginArray();
	size_t n = 0;
	const char *const *paths = tunnelPaths(&n);
	for (size_t i = 0; i < n; ++i)
		j.value(paths[i]);
	j.endArray();
	j.key("port");
	j.value((unsigned long) answeringPort());
	j.key("default_password");
	j.value(shippedPasswordInEffect(config()));
	j.key("totp");
	j.value(oauth::totpActive());
	j.endObject();
}

const FieldDesc kAiFields[] = {
	HTTPD_MEMBER("enabled", FieldType::Bool, "whether AI clients are answered at all"),
	HTTPD_MEMBER("public_url", FieldType::String,
		"the https origin a tunnel publishes this box under, empty while none is set"),
	HTTPD_MEMBER("trusted_proxies", FieldType::String,
		"the addresses a tunnel connects from, comma separated with their lengths"),
	HTTPD_MEMBER("allow_lan", FieldType::Bool,
		"whether AI clients in the local network are answered at /mcp"),
	HTTPD_MEMBER("mcp_url", FieldType::String,
		"the public address of /mcp, empty while no public address is set"),
	HTTPD_LIST_OF_VALUES("tunnel_paths", ElementType::String,
		"what a tunnel request may reach: an entry ending in a slash is a prefix, any other is exact"),
	HTTPD_MEMBER("port", FieldType::UInt, "the port this server answers on"),
	HTTPD_MEMBER("default_password", FieldType::Bool,
		"whether the login is still the password the image ships, which keeps the tunnel shut"),
	HTTPD_MEMBER("totp", FieldType::Bool,
		"whether two-factor sign-in is set up; without it the consent page refuses new sign-ins through the tunnel"),
};

const Schema kAiSchema = { "ai-settings", HTTPD_FIELDS(kAiFields) };

const FieldDesc kAiChangedFields[] = {
	HTTPD_OBJECT("ai", &kAiSchema, "the settings as the file now holds them"),
	HTTPD_MEMBER("restarting", FieldType::Bool,
		"whether the server is put on them once this answer has gone, which ends every connection it holds"),
};

const Schema kAiChangedSchema = { "ai-settings-change", HTTPD_FIELDS(kAiChangedFields) };

const FieldDesc kTunnelFields[] = {
	HTTPD_MEMBER("id", FieldType::String, "cloudflare, tailscale, caddy, nginx or dyndns, which the page keys its words on"),
	HTTPD_MEMBER("file", FieldType::String, "where the snippet goes, empty for one that is a list of commands"),
	HTTPD_MEMBER("snippet", FieldType::String, "the configuration, filled from the settings and forwarding the tunnel paths only"),
	HTTPD_LIST_OF_VALUES("warnings", ElementType::String, "device, cloudflare-ratelimit, tailscale-name, tailscale-port, nginx-acme, dyndns-fixed-address or dyndns-no-direct, each a sentence the page holds"),
	HTTPD_MEMBER("ready", FieldType::Bool, "whether the settings carry everything the snippet needs"),
};

const Schema kTunnelSchema = { "ai-tunnel-guide", HTTPD_FIELDS(kTunnelFields) };

const FieldDesc kClientFields[] = {
	HTTPD_MEMBER("id", FieldType::String, "claude, claude-code, chatgpt or home-assistant, which the page keys its words on"),
	HTTPD_MEMBER("needs", FieldType::String, "public for a client that signs in through the public address, token for one in the local network with a static token"),
	HTTPD_MEMBER("url", FieldType::String, "the address the client is given"),
	HTTPD_MEMBER("command", FieldType::String, "a command that adds the box, empty where the client has none"),
	HTTPD_MEMBER("ready", FieldType::Bool, "whether the settings let this client in"),
};

const Schema kClientSchema = { "ai-client-guide", HTTPD_FIELDS(kClientFields) };

const FieldDesc kGuidesFields[] = {
	HTTPD_MEMBER("mcp_url", FieldType::String, "the public address of /mcp, empty while no public address is set"),
	HTTPD_MEMBER("lan_mcp_url", FieldType::String, "the address of /mcp under the name this page reached the box by, empty where the request named none"),
	HTTPD_LIST_OF_VALUES("paths", ElementType::String, "what a tunnel request may reach, the gate's own list"),
	HTTPD_LIST_OF("tunnels", &kTunnelSchema, "one guide per tunnel, in a fixed order"),
	HTTPD_LIST_OF("clients", &kClientSchema, "one guide per AI client, in a fixed order"),
};

const Schema kGuidesSchema = { "ai-guides", HTTPD_FIELDS(kGuidesFields) };

void writeStrings(Json &j, const std::vector<std::string> &all)
{
	j.beginArray();
	for (size_t i = 0; i < all.size(); ++i)
		j.value(all[i]);
	j.endArray();
}

Response aiGuides(const Request &r)
{
	const AiSettings s = currentAiSettings();

	GuidePlace p;
	// A shut tunnel is no address to hand a client.
	p.public_url = shippedPasswordInEffect(config()) ? std::string() : s.public_url;
	p.box = baseUrl(r);
	p.enabled = s.enabled;
	p.allow_lan = s.allow_lan;
	size_t n = 0;
	const char *const *paths = tunnelPaths(&n);
	for (size_t i = 0; i < n; ++i)
		p.paths.push_back(paths[i]);

	const std::vector<TunnelGuide> tunnels = tunnelGuides(p);
	const std::vector<ClientGuide> clients = clientGuides(p);

	Response out = okJson();
	Json j(out.body, 4096);
	j.beginObject();
	j.key("mcp_url");
	j.value(mcpUrlOf(p.public_url));
	j.key("lan_mcp_url");
	j.value(p.box.empty() ? std::string() : p.box + "/mcp");
	j.key("paths");
	writeStrings(j, p.paths);
	j.key("tunnels");
	j.beginArray();
	for (size_t i = 0; i < tunnels.size(); ++i)
	{
		j.beginObject();
		j.key("id");
		j.value(tunnels[i].id);
		j.key("file");
		j.value(tunnels[i].file);
		j.key("snippet");
		j.value(tunnels[i].snippet);
		j.key("warnings");
		writeStrings(j, tunnels[i].warnings);
		j.key("ready");
		j.value(tunnels[i].ready);
		j.endObject();
	}
	j.endArray();
	j.key("clients");
	j.beginArray();
	for (size_t i = 0; i < clients.size(); ++i)
	{
		j.beginObject();
		j.key("id");
		j.value(clients[i].id);
		j.key("needs");
		j.value(clients[i].needs);
		j.key("url");
		j.value(clients[i].url);
		j.key("command");
		j.value(clients[i].command);
		j.key("ready");
		j.value(clients[i].ready);
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return out;
}

const FieldDesc kAllowedPluginFields[] = {
	HTTPD_MEMBER("name", FieldType::String, "the plugin's name, as GET /api/v1/plugins names it"),
	HTTPD_MEMBER("allowed", FieldType::Bool, "whether AI clients may start it"),
};
const Schema kAllowedPluginSchema = { "ai-allowed-plugin", HTTPD_FIELDS(kAllowedPluginFields) };

const FieldDesc kAllowedSectionFields[] = {
	HTTPD_MEMBER("id", FieldType::String, "the section, as GET /api/v1/settings/sections names it"),
	HTTPD_MEMBER("allowed", FieldType::Bool, "whether AI clients may change its settings"),
	HTTPD_MEMBER("denied", FieldType::String,
		"why it can never be allowed, empty when it can: secret, it holds a credential; "
		"network, it is the network; parental, it is the parental lock and its PIN; "
		"update, it is software update and flashing"),
};
const Schema kAllowedSectionSchema = { "ai-allowed-section", HTTPD_FIELDS(kAllowedSectionFields) };

const FieldDesc kAllowlistFields[] = {
	HTTPD_LIST_OF("plugins", &kAllowedPluginSchema, "every plugin the box carries, and any allowed one it no longer does"),
	HTTPD_LIST_OF("sections", &kAllowedSectionSchema, "every settings section"),
};
const Schema kAllowlistSchema = { "ai-allowlists", HTTPD_FIELDS(kAllowlistFields) };

Response allowlistDocument()
{
	const mcp::Allowlists now = mcp::currentAllowlists();
	std::vector<std::string> names;
	coreapi::Result<coreapi::PluginList> got = coreapi::plugins::list();
	if (got.ok())
	{
		for (size_t i = 0; i < got.value().size(); ++i)
			names.push_back(got.value()[i].name);
	}
	for (size_t i = 0; i < now.plugins.size(); ++i)
	{
		if (std::find(names.begin(), names.end(), now.plugins[i]) == names.end())
			names.push_back(now.plugins[i]);
	}
	Response out = okJson();
	Json j(out.body, 1024);
	j.beginObject();
	j.key("plugins");
	j.beginArray();
	for (size_t i = 0; i < names.size(); ++i)
	{
		j.beginObject();
		j.key("name");
		j.value(names[i]);
		j.key("allowed");
		j.value(std::find(now.plugins.begin(), now.plugins.end(), names[i]) != now.plugins.end());
		j.endObject();
	}
	j.endArray();
	j.key("sections");
	j.beginArray();
	coreapi::Result<std::vector<std::string> > sections = coreapi::settings::sections();
	const std::vector<std::string> ids = sections.ok() ? sections.value() : std::vector<std::string>();
	for (size_t i = 0; i < ids.size(); ++i)
	{
		const std::string denied = mcp::sectionDenial(ids[i]);
		j.beginObject();
		j.key("id");
		j.value(ids[i]);
		j.key("allowed");
		j.value(denied.empty() && std::find(now.sections.begin(), now.sections.end(), ids[i]) != now.sections.end());
		j.key("denied");
		j.value(denied);
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return out;
}

Response getAllowlists(const Request &)
{
	return allowlistDocument();
}

std::vector<std::string> commaList(const std::string &text)
{
	std::vector<std::string> out;
	size_t at = 0;
	while (at <= text.size())
	{
		size_t end = text.find(',', at);
		if (end == std::string::npos)
			end = text.size();
		std::string one = text.substr(at, end - at);
		while (!one.empty() && one[0] == ' ')
			one.erase(0, 1);
		while (!one.empty() && one[one.size() - 1] == ' ')
			one.erase(one.size() - 1);
		if (!one.empty() && std::find(out.begin(), out.end(), one) == out.end())
			out.push_back(one);
		at = end + 1;
	}
	return out;
}

Response putAllowlists(const Request &r)
{
	if (!namedInBody(r, "plugins") && !namedInBody(r, "sections"))
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::MissingParameter, "name plugins or sections");
	mcp::Allowlists next = mcp::currentAllowlists();
	if (namedInBody(r, "plugins"))
	{
		next.plugins = commaList(r.asString("plugins"));
		for (size_t i = 0; i < next.plugins.size(); ++i)
		{
			if (next.plugins[i].size() > 64)
				return problemResponse(StatusBadRequest, coreapi::ErrorCode::BadString,
				                       "a plugin name holds a control byte or is longer than 64 bytes");
			for (size_t k = 0; k < next.plugins[i].size(); ++k)
			{
				const unsigned char c = (unsigned char) next.plugins[i][k];
				if (c < 0x20 || c == 0x7f)
					return problemResponse(StatusBadRequest, coreapi::ErrorCode::BadString,
					                       "a plugin name holds a control byte or is longer than 64 bytes");
			}
		}
	}
	if (namedInBody(r, "sections"))
	{
		next.sections = commaList(r.asString("sections"));
		coreapi::Result<std::vector<std::string> > known = coreapi::settings::sections();
		for (size_t i = 0; i < next.sections.size(); ++i)
		{
			if (!known.ok() || std::find(known.value().begin(), known.value().end(), next.sections[i]) ==
			                       known.value().end())
				return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchName,
				                       "no settings section is called " + next.sections[i]);
			if (!mcp::sectionDenial(next.sections[i]).empty())
				return problemResponse(StatusBadRequest, coreapi::ErrorCode::SettingsSectionDenied,
				                       "no AI client may ever change the section " + next.sections[i]);
		}
	}
	if (!saveAiAllowlists(configPath(), next.plugins, next.sections))
		return problemResponse(StatusInternalServerError, coreapi::ErrorCode::WebserverNotConfigured,
		                       whatWentWrong());
	return allowlistDocument();
}

const Param kAllowlistParams[] = {
	HTTPD_BODY_TEXT("plugins", "the plugins AI clients may start, by name, separated by commas; empty for none", 2048),
	HTTPD_BODY_TEXT("sections", "the settings sections AI clients may change, separated by commas; empty for none", 512),
};

const RouteRefusal kAllowlistRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, MissingParameter, "name plugins or sections"),
	HTTPD_REFUSES(InvalidArgument, BadString, "a plugin name holds a control byte or is longer than 64 bytes"),
	HTTPD_REFUSES(NotFound, NoSuchName, "no settings section is called nonsense"),
	HTTPD_REFUSES(InvalidArgument, SettingsSectionDenied, "no AI client may ever change the section parental"),
	HTTPD_REFUSES(Internal, WebserverNotConfigured, "the file could not be written"),
};

Response aiRead(const Request &)
{
	Response out = okJson();
	Json j(out.body, 512);
	writeAi(j, currentAiSettings());
	return out;
}

Response aiWrite(const Request &r)
{
	const std::string &path = configPath();
	if (path.empty())
	{
		return problemResponse(StatusInternalServerError, coreapi::ErrorCode::WebserverNotConfigured,
				       "this server was never told which file its configuration is kept in");
	}

	const AiSettings before = currentAiSettings();
	AiSettings s = before;
	bool named = false;
	std::string why;

	if (r.has("enabled"))
	{
		s.enabled = r.asBool("enabled");
		named = true;
	}
	if (r.has("allow_lan"))
	{
		s.allow_lan = r.asBool("allow_lan");
		named = true;
	}
	if (namedInBody(r, "public_url"))
	{
		if (!normalisePublicUrl(r.asString("public_url"), &s.public_url, &why))
			return problemResponse(StatusBadRequest, coreapi::ErrorCode::AiPublicUrlRefused, why);
		if (!s.public_url.empty() && s.public_url != before.public_url && shippedPasswordInEffect(config()))
		{
			return problemResponse(StatusConflict, coreapi::ErrorCode::AiDefaultPassword,
					       "the login is still the password the image ships; change it before the box is published");
		}
		named = true;
	}
	if (namedInBody(r, "trusted_proxies"))
	{
		if (!readTrustedProxies(r.asString("trusted_proxies"), &s.trusted_proxies, &why))
			return problemResponse(StatusBadRequest, coreapi::ErrorCode::AiTrustedProxiesRefused, why);
		named = true;
	}
	if (!named)
	{
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::MissingParameter,
				       "the body names nothing to change");
	}

	// Saved, this page would be answered 404 from its next request on.
	if (addressInAnyPrefix(r.peer(), s.trusted_proxies))
	{
		return problemResponse(StatusConflict, coreapi::ErrorCode::AiCallerWouldBeTunnel,
				       "the address this request came from is in the list, and this page would stop answering it");
	}

	// A box that named no key starts strict mode only with the reload.
	const bool first = !config().ai_named;

	if (!saveAiSettings(path, s))
		return problemResponse(StatusInternalServerError, coreapi::ErrorCode::WebserverNotConfigured, whatWentWrong());

	const bool changed = first
		|| s.enabled != before.enabled
		|| s.allow_lan != before.allow_lan
		|| s.public_url != before.public_url
		|| trustedProxyText(s.trusted_proxies) != trustedProxyText(before.trusted_proxies);

	Response out = okJson();
	Json j(out.body, 640);
	j.beginObject();
	j.key("ai");
	writeAi(j, s);
	j.key("restarting");
	j.value(changed);
	j.endObject();

	if (changed)
		out.reload_after = path;
	return out;
}

const Param kAiWriteParams[] = {
	HTTPD_BODY("enabled", ParamType::Bool, "whether AI clients are answered at all"),
	HTTPD_BODY_TEXT("public_url", "an https origin without a path, or empty to clear it", 300),
	HTTPD_BODY_TEXT("trusted_proxies", "comma separated addresses or networks of /24 or /64 at most, never the box itself and never the caller's own address, or empty to clear it", 1024),
	HTTPD_BODY("allow_lan", ParamType::Bool, "whether AI clients in the local network are answered at /mcp"),
};

const RouteRefusal kAiWriteRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, MissingParameter,
		"the body names nothing to change"),
	HTTPD_REFUSES(InvalidArgument, AiPublicUrlRefused,
		"the address does not begin with https://"),
	HTTPD_REFUSES(InvalidArgument, AiTrustedProxiesRefused,
		"an entry names the box itself or no address at all"),
	HTTPD_REFUSES(Conflict, AiCallerWouldBeTunnel,
		"the address this request came from is in the list, and this page would stop answering it"),
	HTTPD_REFUSES(Conflict, AiDefaultPassword,
		"the login is still the password the image ships; change it before the box is published"),
	HTTPD_REFUSES(Internal, WebserverNotConfigured,
		"the configuration file is not there, and a file written from nothing here would carry no login"),
};

std::string boxAccount()
{
	char name[256];
	if (::gethostname(name, sizeof(name)) != 0)
		return "box";
	name[sizeof(name) - 1] = '\0';
	return name[0] != '\0' ? std::string(name) : std::string("box");
}

// A token is no stand-in for the password a session was opened with.
bool fromSignedInHome(const Request &r)
{
	return r.origin() == Origin::Lan && sessionIsLive(r.session());
}

Response notFromHome()
{
	return problemResponse(StatusForbidden, coreapi::ErrorCode::NotPermitted,
	                       "two-factor sign-in is changed from a signed-in session on the home network only");
}

const FieldDesc kTotpSetupFields[] = {
	HTTPD_MEMBER("secret", FieldType::String, "the new secret in base32, answered this once"),
	HTTPD_MEMBER("uri", FieldType::String, "the otpauth address an authenticator app reads from a QR code"),
};

const Schema kTotpSetupSchema = { "ai-totp-setup", HTTPD_FIELDS(kTotpSetupFields) };

Response totpSetup(const Request &r)
{
	if (!fromSignedInHome(r))
		return notFromHome();
	oauth::TotpSetup s;
	if (!oauth::startTotpSetup(boxAccount(), &s))
		return problemResponse(StatusInternalServerError, coreapi::ErrorCode::BoxUnreadable,
		                       "the box could not draw a secret");
	Response out = okJson();
	Json j(out.body, 256);
	j.beginObject();
	j.key("secret");
	j.value(s.secret);
	j.key("uri");
	j.value(s.uri);
	j.endObject();
	return out;
}

Response totpConfirm(const Request &r)
{
	if (!fromSignedInHome(r))
		return notFromHome();
	switch (oauth::confirmTotpSetup(r.asString("code")))
	{
		case oauth::SetupConfirm::Activated:
			return noContent();
		case oauth::SetupConfirm::NoPending:
			return problemResponse(StatusConflict, coreapi::ErrorCode::AiTotpNoPending,
			                       "no setup is waiting for a code; one is kept for ten minutes");
		case oauth::SetupConfirm::ClockUnknown:
			return problemResponse(StatusConflict, coreapi::ErrorCode::AiTotpClockUnknown,
			                       "the clock of the box is not set yet");
		case oauth::SetupConfirm::Wrong:
			return problemResponse(StatusBadRequest, coreapi::ErrorCode::AiTotpCodeWrong,
			                       "the code does not belong to the secret being set up");
		case oauth::SetupConfirm::NotSaved:
			break;
	}
	return problemResponse(StatusInternalServerError, coreapi::ErrorCode::WebserverNotConfigured, whatWentWrong());
}

Response totpDisable(const Request &r)
{
	if (!fromSignedInHome(r))
		return notFromHome();
	unsigned retry_after = 0;
	if (beginLoginAttempt(r.peer(), &retry_after) != LoginAttempt::Open)
		return tooManyAttemptsResponse(retry_after);
	AttemptHeld held(r.peer());
	if (!verifySecret(r.asString("password"), config().password_hash))
		return problemResponse(StatusForbidden, coreapi::ErrorCode::NotPermitted, "the password is wrong");
	held.grant();
	if (!oauth::totpActive())
		return problemResponse(StatusConflict, coreapi::ErrorCode::AiTotpNotSetUp,
		                       "two-factor sign-in is not set up");
	if (!oauth::removeTotp())
		return problemResponse(StatusInternalServerError, coreapi::ErrorCode::WebserverNotConfigured, whatWentWrong());
	return noContent();
}

const Param kTotpConfirmParams[] = {
	HTTPD_BODY_REQUIRED_TEXT("code", "the six digits the authenticator app shows for the pending secret", 16),
};

const Param kTotpDisableParams[] = {
	HTTPD_BODY_REQUIRED_TEXT("password", "the password of the box login, asked again", 512),
};

const RouteRefusal kTotpSetupRefusals[] = {
	HTTPD_REFUSES(Denied, NotPermitted, "the request did not come from a signed-in session on the home network"),
	HTTPD_REFUSES(Internal, BoxUnreadable, "the box could not draw a secret"),
};

const RouteRefusal kTotpConfirmRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, AiTotpCodeWrong, "the code does not belong to the secret being set up"),
	HTTPD_REFUSES(Conflict, AiTotpNoPending, "no setup is waiting for a code"),
	HTTPD_REFUSES(Conflict, AiTotpClockUnknown, "the clock of the box is not set yet"),
	HTTPD_REFUSES(Denied, NotPermitted, "the request did not come from a signed-in session on the home network"),
	HTTPD_REFUSES(Internal, WebserverNotConfigured, "the configuration file could not be written"),
};

const RouteRefusal kTotpDisableRefusals[] = {
	HTTPD_REFUSES(Denied, NotPermitted, "the password is wrong"),
	HTTPD_REFUSES(Conflict, AiTotpNotSetUp, "two-factor sign-in is not set up"),
	HTTPD_REFUSES_AS(429, TooManyAttempts, "too many attempts are being made here; come back in a moment"),
	HTTPD_REFUSES(Internal, WebserverNotConfigured, "the configuration file could not be written"),
};

const Endpoint kAiEndpoints[] = {
	{ Method::Get, "/api/v1/ai/settings", AuthLevel::System,
	  "how AI clients reach this box: whether they are answered, under which public address, and which machines are the tunnel",
	  "Reads the 4 settings that decide how AI clients reach the MCP endpoint at `/mcp`, as `ni-web.conf` "
	  "holds them, together with what follows from them: `mcp_url`, the address a client outside the "
	  "local network is given, and `tunnel_paths`, the only paths a request arriving through the tunnel "
	  "may reach. `port` is the port this server answers on. `default_password` is `true` while the "
	  "login is still the password the image ships; the tunnel is shut then, whatever `public_url` says. "
	  "`totp` is `true` once two-factor sign-in is set up; without it the consent page refuses new "
	  "sign-ins through the tunnel.\n\n"
	  "**Related:** `PUT /api/v1/ai/settings`, `GET /api/v1/ai/guides`, `POST /api/v1/ai/totp/setup`.",
	  NULL, 0, &kAiSchema, &aiRead, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Put, "/api/v1/ai/settings", AuthLevel::System,
	  "changes how AI clients reach this box, naming only what is to change, and puts the server on it after the answer",
	  "Changes the AI settings, naming only the members to change; a member left out keeps its stored "
	  "value. `public_url` is written back as an https origin in lower case without a trailing `/`, "
	  "`trusted_proxies` as a canonical list of networks. The file is written first, then this answer "
	  "is sent, and only after it has gone out does the server restart on the new file, when anything "
	  "changed; `restarting` says whether it will.\n\n"
	  "**Side effects:** rewrites the AI lines of `ni-web.conf`; a restart drops every connection the "
	  "server holds, this one included.\n\n"
	  "**Refusals:**\n"
	  "- `400 missing-parameter`: the body names nothing to change.\n"
	  "- `400 ai-public-url-refused`: `public_url` is not an https origin without path, query, "
	  "fragment or user.\n"
	  "- `400 ai-trusted-proxies-refused`: an entry of `trusted_proxies` is no address, a loopback "
	  "address, a network wider than /24 or /64, or one entry too many.\n"
	  "- `409 ai-caller-would-be-tunnel`: the address this request came from is in the new list, so "
	  "this page would no longer be answered.\n"
	  "- `409 ai-default-password`: `public_url` names an address while the login is still the "
	  "password the image ships; change it under `PUT /api/v1/system/webserver` first.\n"
	  "- `500 webserver-not-configured`: the configuration file is missing or could not be written; "
	  "nothing was changed.\n\n"
	  "**Related:** `GET /api/v1/ai/settings`.",
	  HTTPD_PARAMS(kAiWriteParams), &kAiChangedSchema, &aiWrite, false,
	  Answers200, HTTPD_REFUSALS_AND_BODY(kAiWriteRefusals,
		"{\"enabled\":true,\"public_url\":\"https://tv.example.org\",\"trusted_proxies\":\"192.168.1.5/32\",\"allow_lan\":true}") },
	{ Method::Get, "/api/v1/ai/guides", AuthLevel::System,
	  "how to put a tunnel in front of this box and connect AI clients to it, with each snippet filled from the settings",
	  "Answers the guides the KI tab shows: one per tunnel (`cloudflare`, `tailscale`, `caddy`, `nginx`, "
	  "`dyndns`, in that order) with a configuration snippet filled from the AI settings, and one per AI client "
	  "(`claude`, `claude-code`, `chatgpt`, `home-assistant`) with the address it is given. Every snippet "
	  "forwards exactly `paths` and answers 404 to everything else. An address the settings do not give "
	  "appears as `YOUR-DOMAIN` or `BOX-ADDRESS`, and the guides that need it say `ready` `false`; "
	  "`TUNNEL-ID` and `YOUR-TOKEN` are always left for the reader to replace. "
	  "The words around each guide are the page's; this answer carries only ids, addresses and "
	  "configuration text.\n\n"
	  "**Related:** `GET /api/v1/ai/settings`.",
	  NULL, 0, &kGuidesSchema, &aiGuides, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/ai/allowlists", AuthLevel::System,
	  "which plugins AI clients may start and which settings sections they may change",
	  "Lists every plugin the box carries and every settings section, each with whether AI clients may "
	  "start or change it. A section that can never be allowed says why in `denied`: it holds a credential, "
	  "or it is the network, the parental lock, or software update. Both lists start empty.\n\n"
	  "**Related:** `PUT /api/v1/ai/allowlists`, `GET /api/v1/plugins`, `GET /api/v1/settings/sections`.",
	  NULL, 0, &kAllowlistSchema, &getAllowlists, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Put, "/api/v1/ai/allowlists", AuthLevel::System,
	  "sets which plugins AI clients may start and which settings sections they may change",
	  "Replaces either list or both, each as names separated by commas; an empty text empties it. A plugin "
	  "is an arbitrary program, so allowing one lets an AI client run it. The lists are saved and hold from "
	  "the next request on, without a restart. Answers the same document as the read.\n\n"
	  "**Refusals:**\n"
	  "- `400 missing-parameter`: the body names neither list.\n"
	  "- `400 bad-string`: a plugin name holds a control byte or is longer than 64 bytes.\n"
	  "- `404 no-such-name`: a section the box does not have.\n"
	  "- `400 settings-section-denied`: a section that can never be allowed.\n"
	  "- `500 webserver-not-configured`: the file could not be written.\n\n"
	  "**Related:** `GET /api/v1/ai/allowlists`.",
	  HTTPD_PARAMS(kAllowlistParams), &kAllowlistSchema, &putAllowlists, false,
	  Answers200, HTTPD_REFUSALS_AND_BODY(kAllowlistRefusals, "{\"plugins\":\"Tierpark\",\"sections\":\"audio\"}") },
	{ Method::Post, "/api/v1/ai/totp/setup", AuthLevel::System,
	  "draws a new secret for two-factor sign-in and holds it until a code from it confirms it",
	  "Draws a new 160 bit secret for two-factor sign-in (TOTP, RFC 6238: HMAC-SHA1, 6 digits, 30 s) and "
	  "answers it once, in base32 and as the `otpauth://` address an authenticator app reads from a QR code. "
	  "The secret is pending: the consent page keeps the state it had, none or the previous secret, until "
	  "`POST /api/v1/ai/totp/confirm` is sent a code from it. A pending secret is dropped after ten minutes "
	  "and by the next call here. Answered to a session from `POST /api/v1/login` on the home network only, "
	  "never to a bearer token.\n\n"
	  "**Refusals:**\n"
	  "- `403 not-permitted`: the request did not come from a signed-in session on the home network.\n"
	  "- `500 box-unreadable`: the box could not draw a secret; nothing changed.\n\n"
	  "**Related:** `POST /api/v1/ai/totp/confirm`, `POST /api/v1/ai/totp/disable`, `GET /api/v1/ai/settings`.",
	  NULL, 0, &kTotpSetupSchema, &totpSetup, false,
	  Answers200, HTTPD_REFUSALS(kTotpSetupRefusals) },
	{ Method::Post, "/api/v1/ai/totp/confirm", AuthLevel::System,
	  "turns the pending secret on once a code from it is right",
	  "Checks `code` against the pending secret, the current 30 s step and one step either side, and turns "
	  "the secret on when it matches; from then on the consent page asks for a code for every new sign-in "
	  "through the tunnel. A wrong code leaves everything as it was, the pending secret included. "
	  "Answered to a session from `POST /api/v1/login` on the home network only, never to a bearer token.\n\n"
	  "**Side effects:** writes `ai_totp_secret` and `ai_totp_last_step` into `ni-web.conf`.\n\n"
	  "**Refusals:**\n"
	  "- `400 ai-totp-code-wrong`: the code does not belong to the pending secret.\n"
	  "- `409 ai-totp-no-pending`: no setup is waiting, or it is older than ten minutes.\n"
	  "- `409 ai-totp-clock-unknown`: the clock of the box is not set yet.\n"
	  "- `403 not-permitted`: the request did not come from a signed-in session on the home network.\n"
	  "- `500 webserver-not-configured`: the configuration file could not be written; nothing was turned on.\n\n"
	  "**Related:** `POST /api/v1/ai/totp/setup`.",
	  HTTPD_PARAMS(kTotpConfirmParams), NULL, &totpConfirm, false,
	  Answers204, HTTPD_REFUSALS_AND_BODY(kTotpConfirmRefusals, "{\"code\":\"123456\"}") },
	{ Method::Post, "/api/v1/ai/totp/disable", AuthLevel::System,
	  "turns two-factor sign-in off, asking for the password again",
	  "Turns two-factor sign-in off after checking the box password once more; wrong passwords count in the "
	  "same delay the login has. Connections already granted keep working; new sign-ins through the tunnel "
	  "are refused until it is set up again. Answered to a session from `POST /api/v1/login` on the home "
	  "network only, never to a bearer token.\n\n"
	  "**Side effects:** empties `ai_totp_secret` in `ni-web.conf` and drops a pending setup.\n\n"
	  "**Refusals:**\n"
	  "- `403 not-permitted`: the password is wrong, or the request did not come from a signed-in session on the home network.\n"
	  "- `409 ai-totp-not-set-up`: two-factor sign-in is not set up.\n"
	  "- `429 too-many-attempts`: too many wrong passwords from this address lately; `Retry-After` says how long to wait.\n"
	  "- `500 webserver-not-configured`: the configuration file could not be written; nothing changed.\n\n"
	  "**Related:** `POST /api/v1/ai/totp/setup`.",
	  HTTPD_PARAMS(kTotpDisableParams), NULL, &totpDisable, false,
	  Answers204, HTTPD_REFUSALS_AND_BODY(kTotpDisableRefusals, "{\"password\":\"your-password\"}") },
};

} // namespace

extern const RouteTable aiTable = {
	HTTPD_TABLE("ai", kAiEndpoints)
};

} // namespace exposure

} // namespace httpd

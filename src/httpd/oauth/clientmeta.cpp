/*
 * clientmeta.cpp - OAuth client metadata as registration and metadata documents carry it
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

#include "httpd/oauth/clientmeta.h"

#include "httpd/json.h"
#include "httpd/oauth/uri.h"

#include <jsoncpp/json/json.h>

#include <memory>

namespace httpd
{
namespace oauth
{

namespace
{

const size_t kMaxListValueBytes = 64;

bool printable(const std::string &s)
{
	for (size_t i = 0; i < s.size(); ++i)
	{
		const unsigned char c = (unsigned char) s[i];
		if (c < 0x20 || c == 0x7f)
			return false;
	}
	return isUtf8(s.data(), s.size());
}

// Absent is fine; present must be a printable string within max bytes.
bool stringMember(const ::Json::Value &root, const char *name, size_t max,
                  std::string *out, bool *present)
{
	*present = root.isMember(name);
	if (!*present)
		return true;
	const ::Json::Value &v = root[name];
	if (!v.isString())
		return false;
	*out = v.asString();
	return out->size() <= max && printable(*out);
}

bool listMember(const ::Json::Value &root, const char *name, std::vector<std::string> *out,
                bool *present)
{
	*present = root.isMember(name);
	if (!*present)
		return true;
	const ::Json::Value &v = root[name];
	if (!v.isArray() || v.size() > kMaxRedirectUris)
		return false;
	for (::Json::ArrayIndex i = 0; i < v.size(); ++i)
	{
		if (!v[i].isString() || v[i].asString().size() > kMaxListValueBytes)
			return false;
		out->push_back(v[i].asString());
	}
	return true;
}

bool listed(const std::vector<std::string> &list, const char *value)
{
	for (size_t i = 0; i < list.size(); ++i)
	{
		if (list[i] == value)
			return true;
	}
	return false;
}

bool responseIsCode(const ClientMetadata &m)
{
	if (!m.has_response_types)
		return true;
	if (m.response_types.empty())
		return false;
	for (size_t i = 0; i < m.response_types.size(); ++i)
	{
		if (m.response_types[i] != "code")
			return false;
	}
	return true;
}

bool parseObject(const std::string &body, ::Json::Value *root)
{
	if (!depthWithin(body, kMaxMetadataDepth))
		return false;
	::Json::CharReaderBuilder b;
	b["collectComments"] = false;
	b["allowComments"] = false;
	b["strictRoot"] = true;
	b["allowDroppedNullPlaceholders"] = false;
	b["allowNumericKeys"] = false;
	b["allowSingleQuotes"] = false;
	b["allowSpecialFloats"] = false;
	b["failIfExtra"] = true;
	b["rejectDupKeys"] = true;
	b["stackLimit"] = 16;
	const std::unique_ptr< ::Json::CharReader> r(b.newCharReader());
	std::string errs;
	bool ok = false;
	try
	{
		ok = r->parse(body.data(), body.data() + body.size(), root, &errs);
	}
	catch (...)
	{
		ok = false;
	}
	return ok && root->isObject();
}

} // namespace

bool depthWithin(const std::string &json, size_t max_depth)
{
	size_t depth = 0;
	bool in_string = false;
	bool escaped = false;
	for (size_t i = 0; i < json.size(); ++i)
	{
		const char c = json[i];
		if (in_string)
		{
			if (escaped)
				escaped = false;
			else if (c == '\\')
				escaped = true;
			else if (c == '"')
				in_string = false;
			continue;
		}
		if (c == '"')
			in_string = true;
		else if (c == '{' || c == '[')
		{
			if (++depth > max_depth)
				return false;
		}
		else if (c == '}' || c == ']')
		{
			if (depth == 0)
				return false;
			--depth;
		}
	}
	return true;
}

MetaRead readClientMetadata(const std::string &body, ClientMetadata *out)
{
	*out = ClientMetadata();
	::Json::Value root;
	if (!parseObject(body, &root))
		return MetaRead::NotAnObject;

	if (!root.isMember("redirect_uris"))
		return MetaRead::BadRedirectUris;
	const ::Json::Value &uris = root["redirect_uris"];
	if (!uris.isArray() || uris.size() == 0 || uris.size() > kMaxRedirectUris)
		return MetaRead::BadRedirectUris;
	for (::Json::ArrayIndex i = 0; i < uris.size(); ++i)
	{
		if (!uris[i].isString() || !redirectUriAcceptable(uris[i].asString()))
			return MetaRead::BadRedirectUris;
		out->redirect_uris.push_back(uris[i].asString());
	}

	bool name_present = false;
	if (!stringMember(root, "client_name", kMaxClientNameBytes, &out->client_name, &name_present) ||
	    !stringMember(root, "client_id", kMaxRedirectUriBytes, &out->client_id, &out->has_client_id) ||
	    !stringMember(root, "token_endpoint_auth_method", kMaxListValueBytes,
	                  &out->token_endpoint_auth_method, &out->has_auth_method) ||
	    !listMember(root, "grant_types", &out->grant_types, &out->has_grant_types) ||
	    !listMember(root, "response_types", &out->response_types, &out->has_response_types) ||
	    !listMember(root, "token_endpoint_auth_methods_supported", &out->auth_methods_supported,
	                &out->has_auth_methods_supported))
		return MetaRead::BadValue;
	return MetaRead::Ok;
}

bool flowsAcceptable(const ClientMetadata &m)
{
	if (m.has_grant_types)
	{
		bool code = false;
		for (size_t i = 0; i < m.grant_types.size(); ++i)
		{
			if (m.grant_types[i] == "authorization_code")
				code = true;
			else if (m.grant_types[i] != "refresh_token")
				return false;
		}
		if (!code)
			return false;
	}
	return responseIsCode(m);
}

bool documentFlowsAcceptable(const ClientMetadata &m)
{
	if (m.has_grant_types && !listed(m.grant_types, "authorization_code"))
		return false;
	return responseIsCode(m);
}

bool documentAllowsPublic(const ClientMetadata &m)
{
	if (m.has_auth_method && m.token_endpoint_auth_method == "none")
		return true;
	if (m.has_auth_methods_supported)
		return listed(m.auth_methods_supported, "none");
	return !m.has_auth_method;
}

std::string displayName(const ClientMetadata &m)
{
	if (!m.client_name.empty())
		return m.client_name;
	Url u;
	if (!m.redirect_uris.empty() && parseUrl(m.redirect_uris[0], &u))
		return u.host.substr(0, kMaxClientNameBytes);
	return "?";
}

} // namespace oauth
} // namespace httpd

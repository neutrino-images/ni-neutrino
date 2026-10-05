/*
 * metadata.cpp - what the OAuth server and the mcp resource say about themselves
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

#include "httpd/oauth/metadata.h"

#include "httpd/json.h"
#include "httpd/mcp/contract.h"
#include "httpd/oauth/scopes.h"

#include <vector>

namespace httpd
{
namespace oauth
{

namespace
{

void writeList(Json &j, const std::vector<std::string> &v)
{
	j.beginArray();
	for (size_t i = 0; i < v.size(); ++i)
		j.value(v[i]);
	j.endArray();
}

void writeOne(Json &j, const char *v)
{
	j.beginArray();
	j.value(v);
	j.endArray();
}

} // namespace

std::string authorizationServerMetadata(const std::string &base)
{
	std::string out;
	Json j(out, 1024);
	j.beginObject();
	j.key("issuer");
	j.value(base);
	j.key("authorization_endpoint");
	j.value(base + "/oauth/authorize");
	j.key("token_endpoint");
	j.value(base + "/oauth/token");
	j.key("registration_endpoint");
	j.value(base + "/oauth/register");
	j.key("revocation_endpoint");
	j.value(base + "/oauth/revoke");
	j.key("scopes_supported");
	writeList(j, serverScopes());
	j.key("response_types_supported");
	writeOne(j, "code");
	j.key("response_modes_supported");
	writeOne(j, "query");
	j.key("grant_types_supported");
	j.beginArray();
	j.value("authorization_code");
	j.value("refresh_token");
	j.endArray();
	j.key("token_endpoint_auth_methods_supported");
	writeOne(j, "none");
	j.key("revocation_endpoint_auth_methods_supported");
	writeOne(j, "none");
	j.key("code_challenge_methods_supported");
	writeOne(j, "S256");
	j.key("authorization_response_iss_parameter_supported");
	j.value(true);
	j.key("client_id_metadata_document_supported");
	j.value(true);
	j.endObject();
	return out;
}

std::string protectedResourceMetadata(const std::string &base)
{
	std::string out;
	Json j(out, 512);
	j.beginObject();
	j.key("resource");
	j.value(mcp::resourceOf(base));
	j.key("authorization_servers");
	writeOne(j, base.c_str());
	j.key("scopes_supported");
	writeList(j, resourceScopes());
	j.key("bearer_methods_supported");
	writeOne(j, "header");
	j.key("resource_name");
	j.value("Neutrino");
	j.endObject();
	return out;
}

} // namespace oauth
} // namespace httpd

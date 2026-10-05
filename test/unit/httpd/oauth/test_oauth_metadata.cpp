/*
 * test_oauth_metadata.cpp - tests for the two metadata documents
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

#include "support/catch.hpp"
#include "httpd/mcp/contract.h"
#include "httpd/oauth/metadata.h"
#include "httpd/oauth/oauthtest.h"

#include <string>

using namespace httpd;
using namespace httpd::oauth;

TEST_CASE("the authorization server metadata says what clients must know", "[oauth-metadata]")
{
	const ::Json::Value m = parsedJson(authorizationServerMetadata("https://tv.example.org"));
	REQUIRE(m["issuer"].asString() == "https://tv.example.org");
	REQUIRE(m["authorization_endpoint"].asString() == "https://tv.example.org/oauth/authorize");
	REQUIRE(m["token_endpoint"].asString() == "https://tv.example.org/oauth/token");
	REQUIRE(m["registration_endpoint"].asString() == "https://tv.example.org/oauth/register");
	REQUIRE(m["revocation_endpoint"].asString() == "https://tv.example.org/oauth/revoke");
	REQUIRE(m["code_challenge_methods_supported"].size() == 1u);
	REQUIRE(m["code_challenge_methods_supported"][0].asString() == "S256");
	REQUIRE(m["token_endpoint_auth_methods_supported"].size() == 1u);
	REQUIRE(m["token_endpoint_auth_methods_supported"][0].asString() == "none");
	REQUIRE(m["client_id_metadata_document_supported"].asBool());
	REQUIRE(m["authorization_response_iss_parameter_supported"].asBool());
	REQUIRE(m["response_types_supported"][0].asString() == "code");
	REQUIRE(m["grant_types_supported"].size() == 2u);
	REQUIRE(m["scopes_supported"].size() == 4u);
	REQUIRE(m["scopes_supported"][3].asString() == "offline_access");
}

TEST_CASE("the protected resource metadata names the mcp url and this server", "[oauth-metadata]")
{
	const ::Json::Value m = parsedJson(protectedResourceMetadata("https://tv.example.org"));
	REQUIRE(m["resource"].asString() == "https://tv.example.org/mcp");
	REQUIRE(m["resource"].asString() == mcp::resourceOf("https://tv.example.org"));
	REQUIRE(m["authorization_servers"].size() == 1u);
	REQUIRE(m["authorization_servers"][0].asString() == "https://tv.example.org");
	REQUIRE(m["bearer_methods_supported"][0].asString() == "header");
	REQUIRE(m["scopes_supported"].size() == 3u);
	for (unsigned i = 0; i < m["scopes_supported"].size(); ++i)
		REQUIRE(m["scopes_supported"][i].asString() != "offline_access");
}

TEST_CASE("both documents are built from the base they are given", "[oauth-metadata]")
{
	const std::string base = "https://tv.example.org:8443";
	const ::Json::Value as = parsedJson(authorizationServerMetadata(base));
	const ::Json::Value prm = parsedJson(protectedResourceMetadata(base));
	REQUIRE(as["issuer"].asString() == base);
	REQUIRE(as["token_endpoint"].asString() == base + "/oauth/token");
	REQUIRE(prm["resource"].asString() == base + "/mcp");
	REQUIRE(prm["authorization_servers"][0].asString() == base);
}

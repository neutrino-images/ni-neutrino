/*
 * clientmeta.h - OAuth client metadata as registration and metadata documents carry it
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

#ifndef __httpd_oauth_clientmeta_h__
#define __httpd_oauth_clientmeta_h__

#include "httpd/oauth/store.h"

#include <cstddef>
#include <string>
#include <vector>

namespace httpd
{
namespace oauth
{

struct ClientMetadata
{
	std::string client_id;
	std::string client_name;
	std::string token_endpoint_auth_method;
	std::vector<std::string> redirect_uris;
	std::vector<std::string> grant_types;
	std::vector<std::string> response_types;
	std::vector<std::string> auth_methods_supported;
	bool has_client_id;
	bool has_auth_method;
	bool has_grant_types;
	bool has_response_types;
	bool has_auth_methods_supported;

	ClientMetadata()
		: has_client_id(false), has_auth_method(false), has_grant_types(false),
		  has_response_types(false), has_auth_methods_supported(false)
	{
	}
};

enum class MetaRead
{
	Ok,
	NotAnObject,
	BadRedirectUris,
	BadValue
};

const size_t kMaxMetadataDepth = 4;

// Checked before parsing, because the parser's own limit throws.
bool depthWithin(const std::string &json, size_t max_depth);

// Members this server does not read are ignored (RFC 7591 section 2).
MetaRead readClientMetadata(const std::string &body, ClientMetadata *out);

// Authorization code with optional refresh, response type code, nothing else.
bool flowsAcceptable(const ClientMetadata &m);

// A metadata document may name more grants; the token endpoint refuses those itself.
bool documentFlowsAcceptable(const ClientMetadata &m);

// The document lets the client go without a secret, whatever it prefers.
bool documentAllowsPublic(const ClientMetadata &m);

std::string displayName(const ClientMetadata &m);

} // namespace oauth
} // namespace httpd

#endif

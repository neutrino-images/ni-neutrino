/*
 * tokens.h - opaque OAuth tokens, their stored digests, and PKCE
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

#ifndef __httpd_oauth_tokens_h__
#define __httpd_oauth_tokens_h__

#include <cstddef>
#include <string>

namespace httpd
{
namespace oauth
{

enum class TokenKind
{
	Access,
	Refresh,
	Code,
	Static
};

const size_t kTokenHexChars = 64;

// The prefix names the kind so a lookup never tries a table the token cannot be in.
std::string mintToken(TokenKind k);
bool looksLike(const std::string &token, TokenKind k);

// What is kept instead of the token. 256 random bits need no salt or stretching.
std::string tokenHash(const std::string &token);

std::string newId();

// RFC 7636 section 4.1: 43 to 128 unreserved characters.
bool verifierWellFormed(const std::string &v);
// An S256 challenge is always 43 base64url characters.
bool challengeWellFormed(const std::string &c);
std::string s256(const std::string &verifier);
bool challengeMatches(const std::string &verifier, const std::string &challenge);

} // namespace oauth
} // namespace httpd

#endif

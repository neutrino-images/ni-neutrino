/*
 * tokens.cpp - opaque OAuth tokens, their stored digests, and PKCE
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

#include "httpd/oauth/tokens.h"

#include "httpd/credentials.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>

namespace httpd
{
namespace oauth
{

namespace
{

const char *prefixOf(TokenKind k)
{
	switch (k)
	{
		case TokenKind::Access:
			return "nia_";
		case TokenKind::Refresh:
			return "nir_";
		case TokenKind::Code:
			return "nic_";
		case TokenKind::Static:
			return "nis_";
	}
	return "nix_";
}

bool sha256(const std::string &in, unsigned char out[32])
{
	unsigned int n = 0;
	return EVP_Digest(in.data(), in.size(), out, &n, EVP_sha256(), NULL) == 1 && n == 32;
}

std::string hex(const unsigned char *p, size_t n)
{
	static const char d[] = "0123456789abcdef";
	std::string out;
	out.reserve(n * 2);
	for (size_t i = 0; i < n; ++i)
	{
		out += d[p[i] >> 4];
		out += d[p[i] & 15];
	}
	return out;
}

std::string base64url(const unsigned char *p, size_t n)
{
	static const char a[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	std::string out;
	size_t i = 0;
	for (; i + 2 < n; i += 3)
	{
		const unsigned v = (p[i] << 16) | (p[i + 1] << 8) | p[i + 2];
		out += a[(v >> 18) & 63];
		out += a[(v >> 12) & 63];
		out += a[(v >> 6) & 63];
		out += a[v & 63];
	}
	if (n - i == 1)
	{
		const unsigned v = p[i] << 16;
		out += a[(v >> 18) & 63];
		out += a[(v >> 12) & 63];
	}
	else if (n - i == 2)
	{
		const unsigned v = (p[i] << 16) | (p[i + 1] << 8);
		out += a[(v >> 18) & 63];
		out += a[(v >> 12) & 63];
		out += a[(v >> 6) & 63];
	}
	return out;
}

bool isAlnum(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

bool isLowerHex(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

} // namespace

std::string mintToken(TokenKind k)
{
	const std::string r = randomToken(kTokenHexChars / 2);
	if (r.size() != kTokenHexChars)
		return std::string();
	return std::string(prefixOf(k)) + r;
}

bool looksLike(const std::string &token, TokenKind k)
{
	const std::string p = prefixOf(k);
	if (token.size() != p.size() + kTokenHexChars || token.compare(0, p.size(), p) != 0)
		return false;
	for (size_t i = p.size(); i < token.size(); ++i)
	{
		if (!isLowerHex(token[i]))
			return false;
	}
	return true;
}

std::string tokenHash(const std::string &token)
{
	unsigned char d[32];
	if (!sha256(token, d))
		return std::string();
	return hex(d, sizeof(d));
}

std::string newId()
{
	const std::string r = randomToken(16);
	return (r.size() == 32) ? r : std::string();
}

bool verifierWellFormed(const std::string &v)
{
	if (v.size() < 43 || v.size() > 128)
		return false;
	for (size_t i = 0; i < v.size(); ++i)
	{
		const char c = v[i];
		if (!isAlnum(c) && c != '-' && c != '.' && c != '_' && c != '~')
			return false;
	}
	return true;
}

bool challengeWellFormed(const std::string &c)
{
	if (c.size() != 43)
		return false;
	for (size_t i = 0; i < c.size(); ++i)
	{
		if (!isAlnum(c[i]) && c[i] != '-' && c[i] != '_')
			return false;
	}
	return true;
}

std::string s256(const std::string &verifier)
{
	unsigned char d[32];
	if (!sha256(verifier, d))
		return std::string();
	return base64url(d, sizeof(d));
}

bool challengeMatches(const std::string &verifier, const std::string &challenge)
{
	if (!verifierWellFormed(verifier) || !challengeWellFormed(challenge))
		return false;
	const std::string got = s256(verifier);
	return got.size() == challenge.size() &&
	       CRYPTO_memcmp(got.data(), challenge.data(), challenge.size()) == 0;
}

} // namespace oauth
} // namespace httpd

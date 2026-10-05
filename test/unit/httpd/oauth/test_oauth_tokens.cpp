/*
 * test_oauth_tokens.cpp - tests for OAuth tokens and PKCE
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
#include "httpd/oauth/tokens.h"

#include <string>

using namespace httpd::oauth;

TEST_CASE("a token carries its kind and nothing that repeats", "[oauth-tokens]")
{
	const std::string a = mintToken(TokenKind::Access);
	const std::string b = mintToken(TokenKind::Access);
	REQUIRE(a.size() == 4 + kTokenHexChars);
	REQUIRE(a.compare(0, 4, "nia_") == 0);
	REQUIRE(a != b);
	REQUIRE(looksLike(a, TokenKind::Access));
	REQUIRE_FALSE(looksLike(a, TokenKind::Refresh));
	REQUIRE(mintToken(TokenKind::Refresh).compare(0, 4, "nir_") == 0);
	REQUIRE(mintToken(TokenKind::Code).compare(0, 4, "nic_") == 0);
	REQUIRE(mintToken(TokenKind::Static).compare(0, 4, "nis_") == 0);
}

TEST_CASE("only the exact token shape is taken for a token", "[oauth-tokens]")
{
	const std::string zeros(kTokenHexChars, '0');
	REQUIRE(looksLike("nia_" + zeros, TokenKind::Access));
	REQUIRE_FALSE(looksLike("nia_" + zeros + "0", TokenKind::Access));
	REQUIRE_FALSE(looksLike("nia_" + zeros.substr(1), TokenKind::Access));
	REQUIRE_FALSE(looksLike("nia_" + std::string(kTokenHexChars, 'A'), TokenKind::Access));
	REQUIRE_FALSE(looksLike("NIA_" + zeros, TokenKind::Access));
	REQUIRE_FALSE(looksLike("", TokenKind::Access));
}

TEST_CASE("a token is stored as its sha-256 digest", "[oauth-tokens]")
{
	REQUIRE(tokenHash("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	REQUIRE(tokenHash("nia_" + std::string(kTokenHexChars, '0')) ==
	        "16ac425f2e2e18c53a8d5651aca90bcd519c79b2139f2053f5e4950d2ebe0ab3");
}

TEST_CASE("ids are thirty two hex characters and differ", "[oauth-tokens]")
{
	const std::string a = newId();
	REQUIRE(a.size() == 32u);
	REQUIRE(a.find_first_not_of("0123456789abcdef") == std::string::npos);
	REQUIRE(a != newId());
}

TEST_CASE("pkce s256 matches the rfc 7636 example", "[oauth-tokens]")
{
	const std::string verifier = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
	const std::string challenge = "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM";
	REQUIRE(s256(verifier) == challenge);
	REQUIRE(challengeMatches(verifier, challenge));

	std::string other = verifier;
	other[0] = 'e';
	REQUIRE_FALSE(challengeMatches(other, challenge));
	std::string bent = challenge;
	bent[42] = 'd';
	REQUIRE_FALSE(challengeMatches(verifier, bent));
}

TEST_CASE("a verifier and a challenge must have their exact shapes", "[oauth-tokens]")
{
	REQUIRE(verifierWellFormed(std::string(43, 'a')));
	REQUIRE(verifierWellFormed(std::string(128, '~')));
	REQUIRE_FALSE(verifierWellFormed(std::string(42, 'a')));
	REQUIRE_FALSE(verifierWellFormed(std::string(129, 'a')));
	REQUIRE_FALSE(verifierWellFormed(std::string(42, 'a') + "+"));
	REQUIRE(challengeWellFormed(std::string(43, '_')));
	REQUIRE_FALSE(challengeWellFormed(std::string(44, 'a')));
	REQUIRE_FALSE(challengeWellFormed(std::string(42, 'a') + "="));
	// A plain challenge equal to its verifier is the downgrade S256 exists to stop.
	REQUIRE_FALSE(challengeMatches(std::string(43, 'a'), std::string(43, 'a')));
}

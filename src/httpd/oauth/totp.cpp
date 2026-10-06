/*
 * totp.cpp - time-based one-time codes (RFC 6238) for the consent page
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

#include "httpd/oauth/totp.h"

#include <cstdio>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace httpd
{
namespace oauth
{

namespace
{

const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

int base32Value(char c)
{
	if (c >= 'A' && c <= 'Z')
		return c - 'A';
	if (c >= 'a' && c <= 'z')
		return c - 'a';
	if (c >= '2' && c <= '7')
		return c - '2' + 26;
	return -1;
}

OpenThreads::Mutex &countLock()
{
	static OpenThreads::Mutex m;
	return m;
}

size_t &comparisons()
{
	static size_t n = 0;
	return n;
}

bool unreserved(unsigned char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
	       c == '-' || c == '.' || c == '_' || c == '~';
}

} // namespace

std::string base32Encode(const std::string &bytes)
{
	std::string out;
	unsigned long buffer = 0;
	unsigned bits = 0;
	for (size_t i = 0; i < bytes.size(); ++i)
	{
		buffer = ((buffer << 8) | (unsigned char) bytes[i]) & 0xffffUL;
		bits += 8;
		while (bits >= 5)
		{
			bits -= 5;
			out += kAlphabet[(buffer >> bits) & 31];
		}
	}
	if (bits > 0)
		out += kAlphabet[(buffer << (5 - bits)) & 31];
	return out;
}

bool base32Decode(const std::string &text, std::string *bytes)
{
	std::string out;
	unsigned long buffer = 0;
	unsigned bits = 0;
	bool padding = false;
	for (size_t i = 0; i < text.size(); ++i)
	{
		const char c = text[i];
		if (c == ' ')
			continue;
		if (c == '=')
		{
			padding = true;
			continue;
		}
		const int v = base32Value(c);
		if (padding || v < 0)
			return false;
		buffer = ((buffer << 5) | (unsigned long) v) & 0xffffUL;
		bits += 5;
		if (bits >= 8)
		{
			bits -= 8;
			out += (char) ((buffer >> bits) & 0xff);
		}
	}
	if (bits >= 5 || (buffer & ((1UL << bits) - 1)) != 0)
		return false;
	bytes->swap(out);
	return true;
}

std::string hotp(const std::string &key, unsigned long long counter, unsigned digits)
{
	if (key.empty() || digits == 0 || digits > 9)
		return std::string();
	unsigned char msg[8];
	for (int i = 7; i >= 0; --i)
	{
		msg[i] = (unsigned char) (counter & 0xff);
		counter >>= 8;
	}
	unsigned char mac[EVP_MAX_MD_SIZE];
	unsigned int len = 0;
	if (HMAC(EVP_sha1(), key.data(), (int) key.size(), msg, sizeof(msg), mac, &len) == NULL || len < 20)
		return std::string();
	const unsigned offset = mac[len - 1] & 0x0f;
	const unsigned long binary = ((unsigned long) (mac[offset] & 0x7f) << 24) |
	                             ((unsigned long) mac[offset + 1] << 16) |
	                             ((unsigned long) mac[offset + 2] << 8) |
	                             (unsigned long) mac[offset + 3];
	unsigned long modulo = 1;
	for (unsigned i = 0; i < digits; ++i)
		modulo *= 10;
	char out[16];
	std::snprintf(out, sizeof(out), "%0*lu", (int) digits, binary % modulo);
	return std::string(out);
}

std::string typedCode(const std::string &typed)
{
	std::string out;
	for (size_t i = 0; i < typed.size(); ++i)
	{
		const char c = typed[i];
		if (c == ' ')
			continue;
		if (c < '0' || c > '9')
			return std::string();
		out += c;
	}
	return out.size() == kTotpDigits ? out : std::string();
}

bool sameCode(const std::string &a, const std::string &b)
{
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> held(countLock());
		++comparisons();
	}
	return !a.empty() && a.size() == b.size() && CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

size_t codeComparisonsForTest()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(countLock());
	return comparisons();
}

TotpCheck checkTotp(const std::string &key, const std::string &typed, time_t now,
                    long long last_step, long long *matched_step)
{
	if (now < kTotpClockFloor)
		return TotpCheck::ClockUnknown;
	const std::string code = typedCode(typed);
	const long long step = (long long) now / kTotpStepSeconds;
	long long hit = -1;
	// All three are compared, so the time taken does not say which one matched.
	for (long long s = step - 1; s <= step + 1; ++s)
	{
		const bool same = sameCode(hotp(key, (unsigned long long) s, kTotpDigits), code);
		if (same && s > last_step && hit < 0)
			hit = s;
	}
	if (hit < 0)
		return TotpCheck::Wrong;
	if (matched_step != NULL)
		*matched_step = hit;
	return TotpCheck::Accepted;
}

std::string otpauthUri(const std::string &secret_base32, const std::string &account)
{
	std::string label;
	for (size_t i = 0; i < account.size(); ++i)
	{
		const unsigned char c = (unsigned char) account[i];
		if (unreserved(c))
		{
			label += (char) c;
			continue;
		}
		char hex[4];
		std::snprintf(hex, sizeof(hex), "%%%02X", c);
		label += hex;
	}
	return "otpauth://totp/Neutrino:" + label + "?secret=" + secret_base32 + "&issuer=Neutrino";
}

} // namespace oauth
} // namespace httpd

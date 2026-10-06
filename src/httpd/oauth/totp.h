/*
 * totp.h - time-based one-time codes (RFC 6238) for the consent page
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

#ifndef __httpd_oauth_totp_h__
#define __httpd_oauth_totp_h__

#include <cstddef>
#include <ctime>
#include <string>

namespace httpd
{
namespace oauth
{

const size_t    kTotpSecretBytes = 20;
const unsigned  kTotpDigits      = 6;
const long long kTotpStepSeconds = 30;
// A box clock before this has not been set yet.
const time_t    kTotpClockFloor  = 1767225600;

// RFC 4648 alphabet, upper case, no padding.
std::string base32Encode(const std::string &bytes);

// Either case, spaces and trailing padding allowed; bytes is left alone on refusal.
bool base32Decode(const std::string &text, std::string *bytes);

// RFC 4226 with HMAC-SHA1, zero padded; empty for an empty key or more than 9 digits.
std::string hotp(const std::string &key, unsigned long long counter, unsigned digits);

// Spaces dropped; empty unless exactly kTotpDigits ASCII digits remain.
std::string typedCode(const std::string &typed);

bool sameCode(const std::string &a, const std::string &b);
size_t codeComparisonsForTest();

enum class TotpCheck
{
	Accepted,
	Wrong,
	ClockUnknown
};

// Checks step-1, step and step+1; a match at or below last_step is a replay.
TotpCheck checkTotp(const std::string &key, const std::string &typed, time_t now,
                    long long last_step, long long *matched_step);

std::string otpauthUri(const std::string &secret_base32, const std::string &account);

} // namespace oauth
} // namespace httpd

#endif

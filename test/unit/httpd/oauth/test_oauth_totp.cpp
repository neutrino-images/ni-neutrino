/*
 * test_oauth_totp.cpp - time-based one-time codes
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
#include "httpd/oauth/totp.h"

#include <string>

using namespace httpd::oauth;

namespace
{

const char kRfcKey[] = "12345678901234567890";
const time_t kNow = 1790000000;

std::string key()
{
	return std::string(kRfcKey);
}

std::string at(long long step)
{
	return hotp(key(), (unsigned long long) step, kTotpDigits);
}

long long stepNow()
{
	return (long long) kNow / kTotpStepSeconds;
}

} // namespace

TEST_CASE("totp answers the SHA-1 rows of RFC 6238 appendix B", "[oauth-totp]")
{
	const struct
	{
		long long   t;
		const char *code;
	} rows[] = {
		{ 59LL, "94287082" }, { 1111111109LL, "07081804" }, { 1111111111LL, "14050471" },
		{ 1234567890LL, "89005924" }, { 2000000000LL, "69279037" }, { 20000000000LL, "65353130" },
	};
	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
	{
		INFO(rows[i].t);
		CHECK(hotp(key(), (unsigned long long) (rows[i].t / kTotpStepSeconds), 8) == rows[i].code);
	}
}

TEST_CASE("hotp answers the rows of RFC 4226 appendix D", "[oauth-totp]")
{
	const char *const want[] = { "755224", "287082", "359152", "969429", "338314",
	                             "254676", "287922", "162583", "399871", "520489" };
	for (unsigned long long c = 0; c < 10; ++c)
	{
		INFO(c);
		CHECK(hotp(key(), c, 6) == want[c]);
	}
}

TEST_CASE("hotp answers nothing for an empty key or a digit count it cannot pad", "[oauth-totp]")
{
	CHECK(hotp(std::string(), 1, 6).empty());
	CHECK(hotp(key(), 1, 0).empty());
	CHECK(hotp(key(), 1, 10).empty());
}

TEST_CASE("base32 encodes the RFC 4648 rows without padding", "[oauth-totp]")
{
	CHECK(base32Encode("") == "");
	CHECK(base32Encode("f") == "MY");
	CHECK(base32Encode("fo") == "MZXQ");
	CHECK(base32Encode("foo") == "MZXW6");
	CHECK(base32Encode("foob") == "MZXW6YQ");
	CHECK(base32Encode("fooba") == "MZXW6YTB");
	CHECK(base32Encode("foobar") == "MZXW6YTBOI");
	CHECK(base32Encode(key()) == "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ");
}

TEST_CASE("base32 decodes either case with spaces and padding", "[oauth-totp]")
{
	std::string out;
	REQUIRE(base32Decode("MZXW6YTBOI======", &out));
	CHECK(out == "foobar");
	REQUIRE(base32Decode("mzxw 6ytb oi", &out));
	CHECK(out == "foobar");
	REQUIRE(base32Decode("GEZD GNBV GY3T QOJQ GEZD GNBV GY3T QOJQ", &out));
	CHECK(out == key());
	REQUIRE(base32Decode("", &out));
	CHECK(out.empty());
}

TEST_CASE("base32 refuses bytes outside its alphabet and stray bits", "[oauth-totp]")
{
	const char *const bad[] = { "MZXW1", "MZXW0", "MZXW8", "MZ-XW", "MZ", "M", "MZX", "MY=X" };
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		INFO(bad[i]);
		std::string out = "kept";
		CHECK_FALSE(base32Decode(bad[i], &out));
		CHECK(out == "kept");
	}
}

TEST_CASE("a typed code keeps its six digits and drops spaces", "[oauth-totp]")
{
	CHECK(typedCode("123456") == "123456");
	CHECK(typedCode("123 456") == "123456");
	CHECK(typedCode(" 123456 ") == "123456");
	CHECK(typedCode("12345").empty());
	CHECK(typedCode("1234567").empty());
	CHECK(typedCode("12a456").empty());
	CHECK(typedCode("12\t3456").empty());
	CHECK(typedCode("\xef\xbc\x91" "23456").empty());
	CHECK(typedCode("").empty());
}

TEST_CASE("a code of the step before the current one or after it is taken and two steps away is not", "[oauth-totp]")
{
	const long long s = stepNow();
	for (long long d = -1; d <= 1; ++d)
	{
		INFO(d);
		long long matched = -1;
		CHECK(checkTotp(key(), at(s + d), kNow, 0, &matched) == TotpCheck::Accepted);
		CHECK(matched == s + d);
	}
	CHECK(checkTotp(key(), at(s - 2), kNow, 0, NULL) == TotpCheck::Wrong);
	CHECK(checkTotp(key(), at(s + 2), kNow, 0, NULL) == TotpCheck::Wrong);
}

TEST_CASE("the window follows the step the clock is in at both of its edges", "[oauth-totp]")
{
	const long long s = stepNow();
	const time_t first = (time_t) (s * kTotpStepSeconds);
	const time_t last = first + (time_t) kTotpStepSeconds - 1;
	CHECK(checkTotp(key(), at(s - 1), first, 0, NULL) == TotpCheck::Accepted);
	CHECK(checkTotp(key(), at(s + 1), last, 0, NULL) == TotpCheck::Accepted);
	CHECK(checkTotp(key(), at(s + 2), last, 0, NULL) == TotpCheck::Wrong);
	CHECK(checkTotp(key(), at(s - 2), first, 0, NULL) == TotpCheck::Wrong);
}

TEST_CASE("a code of the last step used or an earlier one is refused even when right", "[oauth-totp]")
{
	const long long s = stepNow();
	CHECK(checkTotp(key(), at(s), kNow, s, NULL) == TotpCheck::Wrong);
	CHECK(checkTotp(key(), at(s - 1), kNow, s, NULL) == TotpCheck::Wrong);
	CHECK(checkTotp(key(), at(s + 1), kNow, s, NULL) == TotpCheck::Accepted);
	CHECK(checkTotp(key(), at(s), kNow, s - 1, NULL) == TotpCheck::Accepted);
}

TEST_CASE("a code typed with a space is checked as six digits", "[oauth-totp]")
{
	const std::string code = at(stepNow());
	CHECK(checkTotp(key(), code.substr(0, 3) + " " + code.substr(3), kNow, 0, NULL) == TotpCheck::Accepted);
}

TEST_CASE("a box clock before 2026 is said rather than called a wrong code", "[oauth-totp]")
{
	const time_t early = kTotpClockFloor - 1;
	const std::string right = hotp(key(), (unsigned long long) (early / kTotpStepSeconds), 6);
	CHECK(checkTotp(key(), right, early, 0, NULL) == TotpCheck::ClockUnknown);
	CHECK(checkTotp(key(), "000000", early, 0, NULL) == TotpCheck::ClockUnknown);
	const std::string first = hotp(key(), (unsigned long long) (kTotpClockFloor / kTotpStepSeconds), 6);
	CHECK(checkTotp(key(), first, kTotpClockFloor, 0, NULL) == TotpCheck::Accepted);
}

TEST_CASE("codes are compared in constant time and every step of the window is compared", "[oauth-totp]")
{
	CHECK(sameCode("123456", "123456"));
	CHECK_FALSE(sameCode("123456", "123457"));
	CHECK_FALSE(sameCode("123456", "12345"));
	CHECK_FALSE(sameCode("", ""));
	const long long s = stepNow();
	const std::string tries[] = { at(s - 1), at(s + 1), "000000x", at(s + 5) };
	for (size_t i = 0; i < 4; ++i)
	{
		INFO(tries[i]);
		const size_t before = codeComparisonsForTest();
		checkTotp(key(), tries[i], kNow, 0, NULL);
		CHECK(codeComparisonsForTest() - before == 3);
	}
}

TEST_CASE("the otpauth address names the box and the issuer and escapes the label", "[oauth-totp]")
{
	CHECK(otpauthUri("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ", "ni-box") ==
	      "otpauth://totp/Neutrino:ni-box?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ&issuer=Neutrino");
	CHECK(otpauthUri("AB", "my box/1") == "otpauth://totp/Neutrino:my%20box%2F1?secret=AB&issuer=Neutrino");
}

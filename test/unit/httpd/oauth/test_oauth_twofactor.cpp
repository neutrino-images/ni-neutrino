/*
 * test_oauth_twofactor.cpp - the two-factor secret, its last step and a pending setup
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
#include "support/fakes.h"
#include "httpd/oauth/totp.h"
#include "httpd/oauth/twofactor.h"
#include "httpd/webconfig.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include <pthread.h>
#include <unistd.h>

using namespace httpd;
using namespace httpd::oauth;

namespace
{

const char kSecret[] = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";
const time_t kNow = 1790000000;

struct Conf
{
	InstalledDependencies wired;
	std::string path;

	explicit Conf(const std::string &extra = std::string())
	{
		char buf[128];
		static int counter = 0;
		std::snprintf(buf, sizeof(buf), "/tmp/ni-twofactor-%d-%d", (int) getpid(), ++counter);
		path = buf;
		write("port=8081\nusername=root\n" + extra);
		REQUIRE(load(path));
		setTotpClockForTest(kNow);
		forgetPendingTotpForTest();
	}
	~Conf()
	{
		::unlink(path.c_str());
		installTotp(std::string(), 0);
		forgetPendingTotpForTest();
		setTotpClockForTest(0);
		setConfigForTest(defaultWebConfig());
	}
	void write(const std::string &text) const
	{
		std::ofstream f(path.c_str(), std::ios::out | std::ios::trunc);
		f << text;
	}
	std::string text() const
	{
		std::ifstream f(path.c_str());
		std::ostringstream out;
		out << f.rdbuf();
		return out.str();
	}
	size_t count(const std::string &needle) const
	{
		const std::string all = text();
		size_t n = 0;
		for (size_t at = all.find(needle); at != std::string::npos; at = all.find(needle, at + 1))
			++n;
		return n;
	}
};

std::string codeAt(const std::string &secret, time_t t)
{
	std::string key;
	REQUIRE(base32Decode(secret, &key));
	return hotp(key, (unsigned long long) (t / kTotpStepSeconds), kTotpDigits);
}

long long stepAt(time_t t)
{
	return (long long) t / kTotpStepSeconds;
}

bool problemsSay(const std::string &needle)
{
	const std::vector<std::string> &all = configProblems();
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (all[i].find(needle) != std::string::npos)
			return true;
	}
	return false;
}

std::string secretLine()
{
	return std::string("ai_totp_secret=") + kSecret + "\n";
}

struct Saver
{
	std::string path;
	int         rounds;
	int         failed;
};

void *saveSteps(void *arg)
{
	Saver *s = static_cast<Saver *>(arg);
	for (int i = 1; i <= s->rounds; ++i)
		s->failed += saveAiTotp(s->path, kSecret, i) ? 0 : 1;
	return NULL;
}

} // namespace

TEST_CASE("a secret and a step in the file are what is in effect", "[oauth-twofactor]")
{
	Conf c(secretLine() + "ai_totp_last_step=5\n");
	CHECK(totpActive());
	CHECK(totpLastStepForTest() == 5);
}

TEST_CASE("a file without the two-factor keys has it off", "[oauth-twofactor]")
{
	Conf c;
	CHECK_FALSE(totpActive());
	CHECK(useTotpCode(codeAt(kSecret, kNow), true) == CodeUse::NotSetUp);
}

TEST_CASE("a secret line that is no secret this box drew leaves it off and is said without its value", "[oauth-twofactor]")
{
	const char *const lines[] = { "ai_totp_secret=NOT-A-SECRET!\n", "ai_totp_secret=MZXW6YTBOI\n" };
	for (size_t i = 0; i < 2; ++i)
	{
		INFO(lines[i]);
		Conf c(lines[i]);
		CHECK_FALSE(totpActive());
		CHECK(problemsSay("ai_totp_secret"));
		CHECK_FALSE(problemsSay("NOT-A-SECRET"));
		CHECK_FALSE(problemsSay("MZXW6YTBOI"));
	}
}

TEST_CASE("a hand written secret in small letters with spaces still counts", "[oauth-twofactor]")
{
	Conf c("ai_totp_secret=gezd gnbv gy3t qojq gezd gnbv gy3t qojq\n");
	REQUIRE(totpActive());
	CHECK(useTotpCode(codeAt(kSecret, kNow), false) == CodeUse::Accepted);
}

TEST_CASE("a step line that is no number counts every code up to now as used", "[oauth-twofactor]")
{
	Conf c(secretLine() + "ai_totp_last_step=abc\n");
	CHECK(totpActive());
	CHECK(totpLastStepForTest() >= stepAt(kTotpClockFloor));
	CHECK(problemsSay("ai_totp_last_step"));
}

TEST_CASE("a code that is used is kept as the last step in memory and in the file", "[oauth-twofactor]")
{
	Conf c(secretLine());
	const std::string code = codeAt(kSecret, kNow);
	REQUIRE(useTotpCode(code, true) == CodeUse::Accepted);
	CHECK(totpLastStepForTest() == stepAt(kNow));
	char line[64];
	std::snprintf(line, sizeof(line), "ai_totp_last_step=%lld\n", stepAt(kNow));
	CHECK(c.text().find(line) != std::string::npos);
	REQUIRE(load(c.path));
	CHECK(totpLastStepForTest() == stepAt(kNow));
	CHECK(useTotpCode(code, true) == CodeUse::Wrong);
}

TEST_CASE("a right code checked without recording is not used up", "[oauth-twofactor]")
{
	Conf c(secretLine());
	const std::string code = codeAt(kSecret, kNow);
	CHECK(useTotpCode(code, false) == CodeUse::Accepted);
	CHECK(totpLastStepForTest() == 0);
	CHECK(useTotpCode(code, true) == CodeUse::Accepted);
}

TEST_CASE("a used code still counts when the file cannot be written", "[oauth-twofactor]")
{
	Conf c(secretLine());
	::unlink(c.path.c_str());
	const std::string code = codeAt(kSecret, kNow);
	CHECK(useTotpCode(code, true) == CodeUse::Accepted);
	CHECK(totpLastStepForTest() == stepAt(kNow));
	CHECK(useTotpCode(code, true) == CodeUse::Wrong);
}

TEST_CASE("codes before the box clock is set are not judged", "[oauth-twofactor]")
{
	Conf c(secretLine());
	setTotpClockForTest(kTotpClockFloor - 1);
	CHECK(useTotpCode("123456", true) == CodeUse::ClockUnknown);
	CHECK(totpLastStepForTest() == 0);
}

TEST_CASE("saving the AI settings keeps the two-factor lines", "[oauth-twofactor]")
{
	Conf c(secretLine() + "ai_totp_last_step=7\n");
	AiSettings s = currentAiSettings();
	s.allow_lan = false;
	REQUIRE(saveAiSettings(c.path, s));
	CHECK(c.text().find(secretLine()) != std::string::npos);
	CHECK(c.text().find("ai_totp_last_step=7\n") != std::string::npos);
	REQUIRE(load(c.path));
	CHECK(totpActive());
	CHECK(totpLastStepForTest() == 7);
}

TEST_CASE("the two lines are written once however often they are saved", "[oauth-twofactor]")
{
	Conf c;
	REQUIRE(saveAiTotp(c.path, kSecret, 1));
	REQUIRE(saveAiTotp(c.path, kSecret, 2));
	REQUIRE(saveAiTotp(c.path, std::string(), 0));
	CHECK(c.count("ai_totp_secret=") == 1);
	CHECK(c.count("ai_totp_last_step=") == 1);
	CHECK(c.text().find("ai_totp_secret=\n") != std::string::npos);
	const std::string before = c.text();
	CHECK_FALSE(saveAiTotp(c.path, "MZXW6", 0));
	CHECK_FALSE(saveAiTotp(c.path, kSecret, -1));
	CHECK(c.text() == before);
}

TEST_CASE("saves running at once keep every update", "[oauth-twofactor]")
{
	Conf c(secretLine() + "ai_totp_last_step=0\n");
	AiSettings s = currentAiSettings();
	Saver steps = { c.path, 200, 0 };
	pthread_t other;
	REQUIRE(pthread_create(&other, NULL, &saveSteps, &steps) == 0);
	int failed = 0;
	for (int i = 0; i < 200; ++i)
	{
		s.allow_lan = (i % 2) == 0;
		failed += saveAiSettings(c.path, s) ? 0 : 1;
	}
	pthread_join(other, NULL);
	CHECK(failed == 0);
	CHECK(steps.failed == 0);
	CHECK(c.text().find("ai_totp_last_step=200\n") != std::string::npos);
	CHECK(c.text().find("ai_allow_lan=false\n") != std::string::npos);
}

TEST_CASE("a setup is pending until a right code confirms it", "[oauth-twofactor]")
{
	Conf c;
	TotpSetup s;
	REQUIRE(startTotpSetup("ni-box", &s));
	std::string raw;
	REQUIRE(base32Decode(s.secret, &raw));
	CHECK(raw.size() == kTotpSecretBytes);
	CHECK(s.uri == "otpauth://totp/Neutrino:ni-box?secret=" + s.secret + "&issuer=Neutrino");
	CHECK_FALSE(totpActive());
	CHECK(confirmTotpSetup("000000x") == SetupConfirm::Wrong);
	CHECK_FALSE(totpActive());
	CHECK(confirmTotpSetup(codeAt(s.secret, kNow)) == SetupConfirm::Activated);
	CHECK(totpActive());
	CHECK(totpLastStepForTest() == stepAt(kNow));
	CHECK(c.text().find("ai_totp_secret=" + s.secret + "\n") != std::string::npos);
	CHECK(confirmTotpSetup(codeAt(s.secret, kNow + 30)) == SetupConfirm::NoPending);
}

TEST_CASE("two setups draw two secrets and only the later one can be confirmed", "[oauth-twofactor]")
{
	Conf c;
	TotpSetup first;
	TotpSetup second;
	REQUIRE(startTotpSetup("ni-box", &first));
	REQUIRE(startTotpSetup("ni-box", &second));
	CHECK(first.secret != second.secret);
	if (codeAt(first.secret, kNow) != codeAt(second.secret, kNow))
		CHECK(confirmTotpSetup(codeAt(first.secret, kNow)) == SetupConfirm::Wrong);
	CHECK(confirmTotpSetup(codeAt(second.secret, kNow)) == SetupConfirm::Activated);
}

TEST_CASE("a pending setup lasts ten minutes and no longer", "[oauth-twofactor]")
{
	Conf c;
	TotpSetup s;
	REQUIRE(startTotpSetup("ni-box", &s));
	setTotpClockForTest(kNow + kPendingTotpSeconds + 1);
	CHECK(confirmTotpSetup(codeAt(s.secret, kNow + kPendingTotpSeconds + 1)) == SetupConfirm::NoPending);

	setTotpClockForTest(kNow);
	REQUIRE(startTotpSetup("ni-box", &s));
	setTotpClockForTest(kNow + kPendingTotpSeconds);
	CHECK(confirmTotpSetup(codeAt(s.secret, kNow + kPendingTotpSeconds)) == SetupConfirm::Activated);
}

TEST_CASE("a pending setup started before the clock was set is gone once it is", "[oauth-twofactor]")
{
	Conf c;
	setTotpClockForTest(1700000000);
	TotpSetup s;
	REQUIRE(startTotpSetup("ni-box", &s));
	setTotpClockForTest(kNow);
	CHECK(confirmTotpSetup(codeAt(s.secret, kNow)) == SetupConfirm::NoPending);
	CHECK_FALSE(totpActive());
}

TEST_CASE("a confirm before the clock is set says so", "[oauth-twofactor]")
{
	Conf c;
	setTotpClockForTest(kTotpClockFloor - 100);
	TotpSetup s;
	REQUIRE(startTotpSetup("ni-box", &s));
	CHECK(confirmTotpSetup(codeAt(s.secret, kTotpClockFloor - 100)) == SetupConfirm::ClockUnknown);
	CHECK_FALSE(totpActive());
}

TEST_CASE("a confirm that cannot be written turns nothing on and keeps the setup", "[oauth-twofactor]")
{
	Conf c;
	TotpSetup s;
	REQUIRE(startTotpSetup("ni-box", &s));
	::unlink(c.path.c_str());
	CHECK(confirmTotpSetup(codeAt(s.secret, kNow)) == SetupConfirm::NotSaved);
	CHECK_FALSE(totpActive());
	c.write("port=8081\nusername=root\n");
	CHECK(confirmTotpSetup(codeAt(s.secret, kNow)) == SetupConfirm::Activated);
}

TEST_CASE("an active secret stays in effect while a new setup is pending", "[oauth-twofactor]")
{
	Conf c(secretLine());
	TotpSetup s;
	REQUIRE(startTotpSetup("ni-box", &s));
	CHECK(useTotpCode(codeAt(kSecret, kNow), false) == CodeUse::Accepted);
	CHECK(confirmTotpSetup("000000x") == SetupConfirm::Wrong);
	CHECK(useTotpCode(codeAt(kSecret, kNow), false) == CodeUse::Accepted);
	CHECK(c.text().find(secretLine()) != std::string::npos);
}

TEST_CASE("removing two-factor sign-in writes it off and drops a pending setup", "[oauth-twofactor]")
{
	Conf c(secretLine() + "ai_totp_last_step=9\n");
	TotpSetup s;
	REQUIRE(startTotpSetup("ni-box", &s));
	REQUIRE(removeTotp());
	CHECK_FALSE(totpActive());
	CHECK(c.text().find("ai_totp_secret=\n") != std::string::npos);
	CHECK(c.text().find("ai_totp_last_step=0\n") != std::string::npos);
	CHECK(confirmTotpSetup(codeAt(s.secret, kNow)) == SetupConfirm::NoPending);
}

TEST_CASE("removing with the file gone changes nothing", "[oauth-twofactor]")
{
	Conf c(secretLine());
	::unlink(c.path.c_str());
	CHECK_FALSE(removeTotp());
	CHECK(totpActive());
}

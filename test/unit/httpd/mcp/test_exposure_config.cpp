/*
 * test_exposure_config.cpp - the AI keys of the server's own file
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
#include "httpd/mcp/exposure.h"
#include "httpd/webconfig.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

namespace
{

class Temp
{
	public:
		explicit Temp(const char *what)
		{
			char buf[128];
			static int counter = 0;
			std::snprintf(buf, sizeof(buf), "/tmp/ni-aiconf-%s-%d-%d", what, (int) getpid(), ++counter);
			path_ = buf;
			::unlink(path_.c_str());
		}
		~Temp() { ::unlink(path_.c_str()); }
		const std::string &path() const { return path_; }

	private:
		std::string path_;
};

void writeFile(const std::string &path, const std::string &text)
{
	std::ofstream f(path.c_str(), std::ios::out | std::ios::trunc);
	REQUIRE(f.good());
	f << text;
}

std::string readFile(const std::string &path)
{
	std::ifstream f(path.c_str());
	std::ostringstream out;
	out << f.rdbuf();
	return out.str();
}

bool problemMentions(const char *needle)
{
	const std::vector<std::string> &all = httpd::configProblems();
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (all[i].find(needle) != std::string::npos)
			return true;
	}
	return false;
}

size_t linesNaming(const std::string &text, const std::string &key)
{
	size_t n = 0;
	std::istringstream in(text);
	std::string line;
	while (std::getline(in, line))
	{
		if (line.compare(0, key.size() + 1, key + "=") == 0)
			++n;
	}
	return n;
}

struct Restore
{
	InstalledDependencies wired;
	~Restore() { httpd::setConfigForTest(httpd::defaultWebConfig()); }
};

const char kBase[] = "port=8081\nusername=root\n";

} // namespace

TEST_CASE("a box that names no AI key runs with the surface off and not strict", "[exposure]")
{
	Restore back;
	Temp file("absent");
	writeFile(file.path(), kBase);
	REQUIRE(httpd::load(file.path()));

	const httpd::WebConfig &c = httpd::config();
	CHECK_FALSE(c.ai_enabled);
	CHECK(c.ai_public_url.empty());
	CHECK(c.ai_trusted_proxies.empty());
	CHECK(c.ai_allow_lan);
	CHECK_FALSE(c.ai_named);
	CHECK_FALSE(httpd::exposure::policyFrom(c).strict);
}

TEST_CASE("the four AI keys are read in their one spelling", "[exposure]")
{
	Restore back;
	Temp file("read");
	writeFile(file.path(), std::string(kBase) +
	          "trusted_proxies=192.168.1.7/32\n"
	          "ai_enabled=true\n"
	          "ai_public_url=HTTPS://TV.Example.org/\n"
	          "ai_trusted_proxies=192.168.1.5,2001:db8::5\n"
	          "ai_allow_lan=false\n");
	REQUIRE(httpd::load(file.path()));

	const httpd::WebConfig &c = httpd::config();
	CHECK(c.ai_enabled);
	CHECK(c.ai_public_url == "https://tv.example.org");
	CHECK(httpd::exposure::trustedProxyText(c.ai_trusted_proxies) == "192.168.1.5/32,2001:db8::5/128");
	CHECK_FALSE(c.ai_allow_lan);
	CHECK(c.ai_named);

	const httpd::exposure::Policy p = httpd::exposure::policyFrom(c);
	CHECK(p.enabled);
	CHECK(p.strict);
	CHECK_FALSE(p.allow_lan);
	CHECK(p.public_url == "https://tv.example.org");
	CHECK(p.tunnels.size() == 2);
	CHECK(p.web_proxies.size() == 1);
}

TEST_CASE("an AI key that cannot be used turns the surface off and keeps it strict", "[exposure]")
{
	struct Line { const char *text; const char *key; };
	const Line lines[] = {
		{ "ai_public_url=http://tv.example.org\n", "ai_public_url" },
		{ "ai_trusted_proxies=127.0.0.1\n", "ai_trusted_proxies" },
		{ "ai_trusted_proxies=192.168.1.5 # nas\n", "ai_trusted_proxies" },
		{ "AI_TRUSTED_PROXIES=192.168.1.5\n", "ai_trusted_proxies" },
		{ "ai_public_url = https://tv.example.org\n", "ai_public_url" },
		{ "ai_enabled=yes\n", "ai_enabled" },
	};
	size_t compared = 0;
	for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); ++i)
	{
		INFO(lines[i].text);
		Restore back;
		Temp file("bad");
		// ai_enabled first, so a later bad line has to switch it back off.
		const std::string head = (std::string(lines[i].key) == "ai_enabled")
			? std::string() : std::string("ai_enabled=true\n");
		writeFile(file.path(), std::string(kBase) + head + lines[i].text);
		REQUIRE(httpd::load(file.path()));

		const httpd::WebConfig &c = httpd::config();
		CHECK_FALSE(c.ai_enabled);
		CHECK(c.ai_named);
		CHECK(httpd::exposure::policyFrom(c).strict);
		CHECK(problemMentions(lines[i].key));
		++compared;
	}
	CHECK(compared == 6);
}

TEST_CASE("an unreadable LAN switch turns AI access off", "[exposure]")
{
	Restore back;
	Temp file("lan");
	writeFile(file.path(), std::string(kBase) + "ai_enabled=true\nai_allow_lan=maybe\n");
	REQUIRE(httpd::load(file.path()));
	CHECK_FALSE(httpd::config().ai_enabled);
	CHECK_FALSE(httpd::config().ai_allow_lan);
	CHECK(problemMentions("ai_allow_lan"));
}

TEST_CASE("a file that is there and cannot be used closes AI access", "[exposure]")
{
	Restore back;
	Temp good("good");
	writeFile(good.path(), std::string(kBase) + "ai_enabled=true\nai_trusted_proxies=192.168.1.5\n");
	Temp dir("dir");
	REQUIRE(::mkdir(dir.path().c_str(), 0700) == 0);
	Temp big("big");
	writeFile(big.path(), std::string(kBase) + std::string(1 << 20, '#'));

	const std::string *const broken[] = { &dir.path(), &big.path() };
	for (size_t i = 0; i < 2; ++i)
	{
		INFO(*broken[i]);
		httpd::setConfigForTest(httpd::defaultWebConfig());
		CHECK_FALSE(httpd::load(*broken[i]));
		CHECK_FALSE(httpd::config().ai_enabled);
		CHECK(httpd::exposure::policyFrom(httpd::config()).strict);

		REQUIRE(httpd::load(good.path()));
		REQUIRE(httpd::config().ai_enabled);
		CHECK_FALSE(httpd::load(*broken[i]));
		CHECK_FALSE(httpd::config().ai_enabled);
		CHECK(httpd::exposure::policyFrom(httpd::config()).strict);
		CHECK(httpd::config().ai_trusted_proxies.size() == 1);
	}
	::rmdir(dir.path().c_str());

	Temp gone("gone");
	httpd::setConfigForTest(httpd::defaultWebConfig());
	CHECK_FALSE(httpd::load(gone.path()));
	CHECK_FALSE(httpd::exposure::policyFrom(httpd::config()).strict);
}

TEST_CASE("a reload after ni-web.conf was deleted switches AI access off", "[exposure]")
{
	Restore back;
	Temp file("deleted");
	writeFile(file.path(), std::string(kBase) +
	          "ai_enabled=true\nai_trusted_proxies=192.168.1.5\nai_allow_lan=false\n");
	REQUIRE(httpd::load(file.path()));
	REQUIRE(httpd::config().ai_enabled);

	::unlink(file.path().c_str());
	CHECK_FALSE(httpd::load(file.path()));

	const httpd::WebConfig &c = httpd::config();
	CHECK_FALSE(c.ai_enabled);
	CHECK(c.ai_trusted_proxies.empty());
	CHECK(c.ai_allow_lan);
	CHECK_FALSE(c.ai_named);
	CHECK_FALSE(httpd::exposure::policyFrom(c).strict);
}

TEST_CASE("a save writes the four lines once and leaves every other line alone", "[exposure]")
{
	Restore back;
	Temp file("save");
	const std::string before = std::string(kBase) +
		"# a comment somebody wrote\n"
		"lan_read=10.9.0.0/16\n"
		"ai_public_url=https://old.example.org\n"
		" Ai_Enabled = true\n"
		"ai_public_url=https://older.example.org\n";
	writeFile(file.path(), before);
	REQUIRE(httpd::load(file.path()));

	httpd::AiSettings s = httpd::currentAiSettings();
	s.enabled = true;
	s.public_url = "https://tv.example.org";
	std::string why;
	REQUIRE(httpd::exposure::readTrustedProxies("192.168.1.5", &s.trusted_proxies, &why));
	s.allow_lan = false;
	REQUIRE(httpd::saveAiSettings(file.path(), s));

	const std::string after = readFile(file.path());
	CHECK(after.find("# a comment somebody wrote\n") != std::string::npos);
	CHECK(after.find("lan_read=10.9.0.0/16\n") != std::string::npos);
	CHECK(after.find("port=8081\n") != std::string::npos);
	CHECK(after.find("Ai_Enabled") == std::string::npos);
	CHECK(linesNaming(after, "ai_enabled") == 1);
	CHECK(linesNaming(after, "ai_public_url") == 1);
	CHECK(linesNaming(after, "ai_trusted_proxies") == 1);
	CHECK(linesNaming(after, "ai_allow_lan") == 1);
	CHECK(after.find("ai_public_url=https://tv.example.org\n") != std::string::npos);
	CHECK(after.find("ai_trusted_proxies=192.168.1.5/32\n") != std::string::npos);

	REQUIRE(httpd::load(file.path()));
	CHECK(httpd::config().ai_enabled);
	CHECK(httpd::config().ai_public_url == "https://tv.example.org");
	CHECK_FALSE(httpd::config().ai_allow_lan);
}

TEST_CASE("a save of a value the loader would refuse writes nothing", "[exposure]")
{
	Restore back;
	Temp file("refuse");
	writeFile(file.path(), kBase);
	REQUIRE(httpd::load(file.path()));

	httpd::AiSettings s = httpd::currentAiSettings();
	s.public_url = "HTTPS://TV.example.org";
	CHECK_FALSE(httpd::saveAiSettings(file.path(), s));
	CHECK(readFile(file.path()) == kBase);

	s = httpd::currentAiSettings();
	httpd::NetPrefix loop;
	REQUIRE(httpd::parsePrefix("127.0.0.1/32", &loop));
	s.trusted_proxies.push_back(loop);
	CHECK_FALSE(httpd::saveAiSettings(file.path(), s));
	CHECK(readFile(file.path()) == kBase);
}

TEST_CASE("a save never creates a file without a login", "[exposure]")
{
	Restore back;
	Temp file("missing");
	CHECK_FALSE(httpd::saveAiSettings(file.path(), httpd::currentAiSettings()));
	CHECK(::access(file.path().c_str(), F_OK) != 0);
}

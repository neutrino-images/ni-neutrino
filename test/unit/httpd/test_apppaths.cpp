/*
 * test_apppaths.cpp - tests for the web file paths
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
#include "support/counts.h"
#include "support/fakes.h"
#include "support/httpclient.h"

#include <config.h>

#include "httpd/apppaths.h"
#include "httpd/endpoint.h"
#include "httpd/router.h"
#include "httpd/server.h"
#include "httpd/static.h"
#include "httpd/webconfig.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace httpd;

namespace
{

const std::string kRoot = "/tmp/coreapi-app." + std::to_string(getpid());

const char kPage[] = "<!doctype html><title>Neutrino</title>\n";

void writeFile(const std::string &path, const std::string &bytes)
{
	std::ofstream f(path.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
	REQUIRE(f.good());
	f.write(bytes.data(), (std::streamsize) bytes.size());
	f.close();
}

/* The directory the daemon cases serve from, holding the one file the page is
   and nothing else. Built by every case that wants one rather than by one of
   them, because the harness takes a selection and a case reading what the case
   above it left behind reads nothing at all when it runs on its own. */
struct Docroot
{
	InstalledDependencies wired_;

	Docroot()
	{
		REQUIRE(std::system(("rm -rf " + kRoot).c_str()) == 0);
		REQUIRE(::mkdir(kRoot.c_str(), 0755) == 0);
		writeFile(kRoot + "/index.html", kPage);

		WebConfig c = defaultWebConfig();
		c.docroot = kRoot;
		setConfigForTest(c);
		setDocroot(kRoot);
	}

	~Docroot()
	{
		stop();
		setRoutesForTest(NULL);
		setDocroot("");
		setConfigForTest(defaultWebConfig());
		std::system(("rm -rf " + kRoot).c_str());
	}

private:
	Docroot(const Docroot &);
	Docroot &operator=(const Docroot &);
};

ServerConfig loopback()
{
	ServerConfig c = defaultConfig();
	c.port = 0;
	c.bind_address = "127.0.0.1";
	return c;
}

} // namespace

TEST_CASE("a name under an area of the page is the page", "[apppaths]")
{
	// The identifier of a channel and the name of a section, which is what
	// every deep address of this page is made of.
	REQUIRE(answersWithPage("/channels/12ab"));
	REQUIRE(answersWithPage("/settings/video"));
	REQUIRE(answersWithPage("/epg/grid/today"));
}

TEST_CASE("the bare name of an area is the page as well", "[apppaths]")
{
	std::size_t count = 0;
	const char *const *areas = appAreas(&count);
	REQUIRE(count == 8);
	for (std::size_t i = 0; i < count; ++i)
		REQUIRE(answersWithPage("/" + std::string(areas[i])));
}

TEST_CASE("a name outside every area is not the page", "[apppaths]")
{
	REQUIRE_FALSE(answersWithPage("/geheim.txt"));
	REQUIRE_FALSE(answersWithPage("/geheim"));
	// The head is the first segment whole and not a beginning of it, so a
	// name that merely starts like an area is not one.
	REQUIRE_FALSE(answersWithPage("/channelsurfing/12ab"));
	REQUIRE_FALSE(answersWithPage("/vendor/preact.module.js"));
	// Under the API nothing is a page, whatever the name looks like.
	REQUIRE_FALSE(answersWithPage("/api/v1/channels"));
}

TEST_CASE("a last segment carrying a dot is a file and stays missing", "[apppaths]")
{
	/* The mistyped module name is the whole reason this rule is here: answered
	   with a document, a browser reports a syntax error on line 1 of what it
	   took for a module rather than saying the name is not there. */
	REQUIRE_FALSE(answersWithPage("/vendor/pract.js"));
	REQUIRE_FALSE(answersWithPage("/channels/logo.png"));
	REQUIRE_FALSE(answersWithPage("/settings/app.css"));
	// A dot anywhere but in the last segment says nothing about the last one.
	REQUIRE(answersWithPage("/channels/list.alt/12ab"));
}

TEST_CASE("a target written with a separator after it is the directory's", "[apppaths]")
{
	// Two rules for one target would disagree about which of them is on it,
	// and the directory rule in static.cpp is already on this one.
	REQUIRE_FALSE(answersWithPage("/channels/"));
	REQUIRE_FALSE(answersWithPage("/"));
	REQUIRE_FALSE(answersWithPage(""));
	// A target that is not a path at all is nobody's.
	REQUIRE_FALSE(answersWithPage("channels/12ab"));
}

TEST_CASE("what the directory does not hold under an area comes back as the page", "[apppaths]")
{
	Docroot tree;
	setRoutesForTest(NULL);
	REQUIRE(start(loopback()));

	testhttp::Reply deep = testhttp::request(boundPort(), "GET", "/channels/12ab");
	REQUIRE(deep.transport_ok);
	REQUIRE(deep.code == 200);
	REQUIRE(deep.body == kPage);
	REQUIRE(deep.header("Content-Type") == "text/html");

	// A name inside an area that names nothing is still the page: what is not
	// found in it is what the page says, not what this server says.
	testhttp::Reply missing = testhttp::request(boundPort(), "GET", "/channels/nosuchthing");
	REQUIRE(missing.transport_ok);
	REQUIRE(missing.code == 200);
	REQUIRE(missing.body == kPage);
}

TEST_CASE("a file this directory does not hold stays missing whatever it is under", "[apppaths]")
{
	Docroot tree;
	setRoutesForTest(NULL);
	REQUIRE(start(loopback()));

	const char *const gone[] = {
		"/vendor/pract.js",     // the typo this whole rule is about
		"/channels/logo.png",   // a file under an area is still a file
		"/channels/",           // the directory rule answers this one
		"/geheim.txt"           // outside every area
	};
	for (std::size_t i = 0; i < sizeof(gone) / sizeof(gone[0]); ++i)
	{
		testhttp::Reply r = testhttp::request(boundPort(), "GET", gone[i]);
		REQUIRE(r.transport_ok);
		REQUIRE(r.code == 404);
		REQUIRE(r.body != kPage);
		REQUIRE(r.header("Content-Type") == "application/problem+json");
	}
}

TEST_CASE("the page does not answer over a refusal the directory made", "[apppaths]")
{
	/* A dot segment under an area reads as a page by the rule above, because
	   its last segment carries no dot. What keeps it from being answered with
	   one is the order: the directory is asked first and turns the target down,
	   and a refusal is not the missing answer the page is offered for. */
	REQUIRE(answersWithPage("/now/../../etc/hostname"));

	Docroot tree;
	setRoutesForTest(NULL);
	REQUIRE(start(loopback()));

	testhttp::Reply r = testhttp::request(boundPort(), "GET", "/now/%2e%2e/%2e%2e/etc/hostname");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 400);
	REQUIRE(r.body != kPage);
}

namespace
{

/* How many files the page installs, counted here rather than in a script, because every
   name in counts.txt has to be one a case recorded: the run fails on a name that file
   holds and nothing measured.

   An identity, so a screen that stops being installed is a failure and not a quieter
   build. What the page weighs is deliberately not a number here: a ceiling recorded
   under its own name would be compared with itself, and the measured weight as an
   identity would move on every edit to every file of the page. The weight is held to a
   ceiling by check-web-size.sh, where a promise belongs. */
const char kWebFiles[] = "web files the page installs";

std::string readWhole(const std::string &path)
{
	std::ifstream f(path.c_str(), std::ios::in | std::ios::binary);
	std::ostringstream out;
	out << f.rdbuf();
	return out.str();
}

/* Every file the install rules of data/ni-web carry, walked the way they walk
   it: the document at the root, and everything under app/ and under info/ whose
   name does not begin with Makefile. Nothing else in that directory is
   installed from this tree, and vendor/ is the buildsystem's and is not in it
   at all. */
void collect(const std::string &dir, const std::string &shown, std::vector<std::string> &out)
{
	DIR *d = ::opendir(dir.c_str());
	REQUIRE(d != NULL);
	std::vector<std::string> subdirs;
	for (struct dirent *e = ::readdir(d); e != NULL; e = ::readdir(d))
	{
		const std::string name(e->d_name);
		if (name == "." || name == "..")
			continue;
		if (name.compare(0, 8, "Makefile") == 0)
			continue;

		const std::string full = dir + "/" + name;
		struct stat st;
		std::memset(&st, 0, sizeof(st));
		REQUIRE(::stat(full.c_str(), &st) == 0);
		if (S_ISDIR(st.st_mode))
			subdirs.push_back(name);
		else if (S_ISREG(st.st_mode))
			out.push_back(shown + name);
	}
	::closedir(d);
	for (size_t i = 0; i < subdirs.size(); ++i)
		collect(dir + "/" + subdirs[i], shown + subdirs[i] + "/", out);
}

} // namespace

TEST_CASE("the page is the files it installs", "[apppaths]")
{
	const std::string dir(NI_WEB_PAGE_DIR);

	std::vector<std::string> files;
	files.push_back("index.html");
	collect(dir + "/app", "app/", files);
	/* The display at /info is a second page out of the same directory, with a
	   document and an install rule of its own, so it is walked as well: a file
	   of it left out of this count is a file nothing here would notice. */
	collect(dir + "/info", "info/", files);

	for (size_t i = 0; i < files.size(); ++i)
	{
		// An empty file here is a file that was added and never written, which
		// the install rule would ship and nobody would notice.
		INFO("data/ni-web/" << files[i]);
		REQUIRE(readWhole(dir + "/" + files[i]).size() > 0);
	}

	recordCount(kWebFiles, files.size());
}

/*
 * test_update.cpp - the image update from outside: what is offered, refused, sent and run
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

#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/update.h"

#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/http.h"
#include "httpd/router.h"

#include <neutrinoMessages.h>

#include <cstdio>
#include <string>
#include <vector>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace coreapi;

namespace
{

bool writeFile(const std::string &path, const std::string &body)
{
	int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd < 0)
		return false;
	ssize_t n = write(fd, body.data(), body.size());
	if (close(fd) != 0)
		return false;
	return n == (ssize_t) body.size();
}

/* A directory of its own, put back the way it was found. */
struct Fixture
{
	std::string dir;
	std::string root_before;
	bool        ready;

	Fixture() : ready(false)
	{
		root_before = update::updateRoot();

		char tmpl[] = "/tmp/coreapi_update_XXXXXX";
		if (mkdtemp(tmpl) == NULL)
			return;
		dir = tmpl;
		update::setUpdateRoot(dir);
		ready = true;
	}

	~Fixture()
	{
		update::setUpdateRoot(root_before);
		if (!dir.empty())
		{
			const std::string cmd = "rm -rf '" + dir + "'";
			if (::system(cmd.c_str()) != 0)
				std::fprintf(stderr, "could not remove %s\n", dir.c_str());
		}
	}

	bool put(const std::string &name, const std::string &body = "x")
	{
		return writeFile(dir + "/" + name, body);
	}

	private:
		Fixture(const Fixture &);
		Fixture &operator=(const Fixture &);
};

// The posted blocks are the case's to release: nothing reads them off a queue.
struct Posted
{
	FakeCommandSink &sink;
	explicit Posted(FakeCommandSink &s) : sink(s) {}
	~Posted()
	{
		for (size_t i = 0; i < sink.posted.size(); i++)
			delete[] (unsigned char *) sink.posted[i].second;
	}
};

update::Request request(const std::string &name)
{
	update::Request r;
	r.name = name;
	return r;
}

// What install() handed on, instead of running install.sh on the host.
std::vector<std::string> g_ran;
int g_exit = 0;

int fakeRunner(const std::string &dir, const std::string &file)
{
	g_ran.push_back(dir);
	g_ran.push_back(file);
	return g_exit;
}

struct FakeRun
{
	FakeRun() { g_ran.clear(); g_exit = 0; update::setRunner(&fakeRunner); }
	~FakeRun() { update::setRunner(NULL); }
};

} // namespace

TEST_CASE("an image update is a .bin, and nothing else is offered", "[update]")
{
	REQUIRE(update::isUpdateName("ni-update.bin"));

	// Complete images are the menu's, on every sort of box.
	REQUIRE_FALSE(update::isUpdateName("image.tgz"));
	REQUIRE_FALSE(update::isUpdateName("image.zip"));
	REQUIRE_FALSE(update::isUpdateName("image.img"));
	// The menu shows these and installs none of them.
	REQUIRE_FALSE(update::isUpdateName("readme.txt"));
	// As written, the way the menu reads the ending once a file is chosen.
	REQUIRE_FALSE(update::isUpdateName("UPDATE.BIN"));
}

TEST_CASE("a name that is not one plain entry of the directory is no update", "[update]")
{
	REQUIRE_FALSE(update::isUpdateName(""));
	// The ending alone is not a name.
	REQUIRE_FALSE(update::isUpdateName(".bin"));
	REQUIRE_FALSE(update::isUpdateName("bin"));
	REQUIRE_FALSE(update::isUpdateName("../etc/x.bin"));
	REQUIRE_FALSE(update::isUpdateName("sub/x.bin"));
	REQUIRE_FALSE(update::isUpdateName("/tmp/x.bin"));
	REQUIRE_FALSE(update::isUpdateName(".hidden.bin"));
	// It ends up as an argument of a script.
	REQUIRE_FALSE(update::isUpdateName("-rf.bin"));
	REQUIRE_FALSE(update::isUpdateName("a\nb.bin"));
	REQUIRE_FALSE(update::isUpdateName(std::string(300, 'a') + ".bin"));
}

TEST_CASE("the listing carries the image updates of the directory and nothing else", "[update]")
{
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.put("zz-update.bin", "12345"));
	REQUIRE(fx.put("aa-update.bin", "123"));
	REQUIRE(fx.put("image.zip"));
	REQUIRE(fx.put("image.img"));
	REQUIRE(fx.put("notes.txt"));
	REQUIRE(fx.put(".part.bin"));
	// A directory under an update's name is not an update.
	REQUIRE(mkdir((fx.dir + "/folder.bin").c_str(), 0700) == 0);

	const Result<update::Local> got = update::local();
	REQUIRE(got.ok());
	REQUIRE(got.value().dir == fx.dir);
	REQUIRE(got.value().files.size() == 2);
	// By name, whatever order the directory hands them out in.
	REQUIRE(got.value().files[0].name == "aa-update.bin");
	REQUIRE(got.value().files[0].size == 3);
	REQUIRE(got.value().files[0].modified > 0);
	REQUIRE(got.value().files[1].name == "zz-update.bin");
	REQUIRE(got.value().files[1].size == 5);
}

TEST_CASE("a directory that is not there is a box with nothing to install", "[update]")
{
	Fixture fx;
	REQUIRE(fx.ready);
	update::setUpdateRoot(fx.dir + "/gone");

	const Result<update::Local> got = update::local();
	REQUIRE(got.ok());
	REQUIRE(got.value().files.empty());
	REQUIRE(got.value().dir == fx.dir + "/gone");
}

TEST_CASE("an image update is sent to the loop with its path", "[update]")
{
	InstalledDependencies deps;
	Posted posted(deps.commands);
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.put("ni-update.bin"));

	REQUIRE(update::start(request("ni-update.bin")).ok());
	REQUIRE(deps.commands.posted.size() == 1);
	REQUIRE(deps.commands.posted[0].first == NeutrinoMessages::EVT_LOCAL_UPDATE);

	const update::Asked a = update::asked((const char *) deps.commands.posted[0].second);
	REQUIRE(a.valid);
	REQUIRE(a.path == fx.dir + "/ni-update.bin");
}

TEST_CASE("what cannot be an image update is refused and nothing is sent", "[update]")
{
	InstalledDependencies deps;
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.put("ni-update.bin"));
	REQUIRE(fx.put("image.zip"));
	REQUIRE(fx.put("notes.txt"));

	const Result<void> path = update::start(request("../" + fx.dir + "/ni-update.bin"));
	REQUIRE_FALSE(path.ok());
	REQUIRE(path.error().status == Status::InvalidArgument);
	REQUIRE(path.error().code == ErrorCode::BadName);

	// There, and still not something this installs.
	const Result<void> image = update::start(request("image.zip"));
	REQUIRE_FALSE(image.ok());
	REQUIRE(image.error().code == ErrorCode::BadName);
	const Result<void> text = update::start(request("notes.txt"));
	REQUIRE_FALSE(text.ok());
	REQUIRE(text.error().code == ErrorCode::BadName);

	const Result<void> gone = update::start(request("other.bin"));
	REQUIRE_FALSE(gone.ok());
	REQUIRE(gone.error().status == Status::NotFound);
	REQUIRE(gone.error().code == ErrorCode::NoSuchUpdateFile);

	REQUIRE(mkdir((fx.dir + "/folder.bin").c_str(), 0700) == 0);
	const Result<void> folder = update::start(request("folder.bin"));
	REQUIRE_FALSE(folder.ok());
	REQUIRE(folder.error().code == ErrorCode::NoSuchUpdateFile);

	REQUIRE(deps.commands.posted.empty());
}

TEST_CASE("a box that is recording is not updated", "[update]")
{
	InstalledDependencies deps;
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.put("ni-update.bin"));
	deps.recordings.add(7, 0x1234, "/media/hdd/movie.ts");

	const Result<void> r = update::start(request("ni-update.bin"));
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Conflict);
	REQUIRE(r.error().code == ErrorCode::RecordingRunning);
	REQUIRE(deps.commands.posted.empty());
}

TEST_CASE("a box that cannot say whether it records is not updated either", "[update]")
{
	InstalledDependencies deps;
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.put("ni-update.bin"));
	deps.recordings.list_status = Status::Internal;

	const Result<void> r = update::start(request("ni-update.bin"));
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	REQUIRE(deps.commands.posted.empty());
}

TEST_CASE("a refused name is refused before the box is asked what it records", "[update]")
{
	InstalledDependencies deps;
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE_FALSE(update::start(request("nothing.bin")).ok());
	REQUIRE(deps.recordings.reads == 0);
}

TEST_CASE("a loop that does not take the request is said so", "[update]")
{
	InstalledDependencies deps;
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.put("ni-update.bin"));
	deps.commands.answer = Status::Internal;

	const Result<void> r = update::start(request("ni-update.bin"));
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().code == ErrorCode::CommandNotPosted);
}

TEST_CASE("a block this layer did not write is not a request to install", "[update]")
{
	REQUIRE_FALSE(update::asked(NULL).valid);
	REQUIRE_FALSE(update::asked("").valid);
	// A path read against wherever the program happens to stand.
	REQUIRE_FALSE(update::asked("ni-update.bin").valid);
	REQUIRE_FALSE(update::asked("tmp/ni-update.bin").valid);
	REQUIRE_FALSE(update::asked("/").valid);
	// Not an image update, or not the name of one.
	REQUIRE_FALSE(update::asked("/tmp/image.tgz").valid);
	REQUIRE_FALSE(update::asked("/tmp/.bin").valid);
	REQUIRE_FALSE(update::asked("/tmp/-x.bin").valid);
	// What the earlier form of this message carried in front of the path.
	REQUIRE_FALSE(update::asked("111/tmp/ni-update.bin").valid);

	const update::Asked a = update::asked("/tmp/ni-update.bin");
	REQUIRE(a.valid);
	REQUIRE(a.path == "/tmp/ni-update.bin");
}

TEST_CASE("installing runs the script in the directory the file lies in", "[update]")
{
	FakeRun run;
	g_exit = 3;

	REQUIRE(update::install(update::asked("/tmp/ni-update.bin")) == 3);
	REQUIRE(g_ran.size() == 2);
	REQUIRE(g_ran[0] == "/tmp");
	REQUIRE(g_ran[1] == "/tmp/ni-update.bin");

	g_ran.clear();
	REQUIRE(update::install(update::asked("/ni-update.bin")) == 3);
	REQUIRE(g_ran[0] == "/");
}

TEST_CASE("the menu's call hands on what it is given, checked or not", "[update]")
{
	FakeRun run;
	g_exit = 1;

	// The update directory of the settings, and a package the menu installs on a box without ofgwrite.
	REQUIRE(update::install("/media/hdd/update", "/media/hdd/update/package.tgz") == 1);
	REQUIRE(g_ran.size() == 2);
	REQUIRE(g_ran[0] == "/media/hdd/update");
	REQUIRE(g_ran[1] == "/media/hdd/update/package.tgz");
}

TEST_CASE("nothing is run for a request that is not valid", "[update]")
{
	FakeRun run;
	REQUIRE(update::install(update::Asked()) == -1);
	REQUIRE(update::install(update::asked("/tmp/image.zip")) == -1);
	REQUIRE(g_ran.empty());
}

TEST_CASE("the directory a name is joined to is the one that was set", "[update]")
{
	Fixture fx;
	REQUIRE(fx.ready);
	update::setUpdateRoot(fx.dir + "/");
	REQUIRE(update::updateRoot() == fx.dir);
}

/* From here down the same two calls are asked for over the router, because
   what a request meets is settled between the two layers: which level each
   route asks for, what a body may carry, and what comes back. */

namespace
{

struct ShippedRoutes
{
	ShippedRoutes() { httpd::setRoutesForTest(NULL); }
	~ShippedRoutes() { httpd::setRoutesForTest(NULL); }

	private:
		ShippedRoutes(const ShippedRoutes &);
		ShippedRoutes &operator=(const ShippedRoutes &);
};

httpd::Response ask(httpd::Method m, const std::string &path, const std::string &body,
		    httpd::AuthLevel granted, const std::string &peer)
{
	return httpd::dispatch(m, path, std::string(), body, peer, granted);
}

httpd::Response asOwner(httpd::Method m, const std::string &path, const std::string &body)
{
	return ask(m, path, body, httpd::AuthLevel::System, "127.0.0.1");
}

bool has(const std::string &body, const char *piece)
{
	return body.find(piece) != std::string::npos;
}

} // namespace

TEST_CASE("the listing route answers the image updates to a reader", "[update]")
{
	ShippedRoutes shipped;
	InstalledDependencies deps;
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.put("image.zip", "123"));
	REQUIRE(fx.put("ni-update.bin", "1234"));

	const httpd::Response r = ask(httpd::Get, "/api/v1/update/local", "",
				      httpd::AuthLevel::Read, "192.168.1.9");
	REQUIRE(r.code == 200);
	REQUIRE(has(r.body, ("\"dir\":\"" + fx.dir + "\"").c_str()));
	REQUIRE(has(r.body, "\"items\":[{\"name\":\"ni-update.bin\",\"size\":4,\"modified\":"));
	REQUIRE_FALSE(has(r.body, "image.zip"));
}

TEST_CASE("installing is not for a caller who was only granted a read or a write", "[update]")
{
	ShippedRoutes shipped;
	InstalledDependencies deps;
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.put("ni-update.bin"));
	const std::string body = "{\"name\":\"ni-update.bin\"}";

	REQUIRE(ask(httpd::Post, "/api/v1/update/local", body, httpd::AuthLevel::Read, "192.168.1.9").code == 403);
	REQUIRE(ask(httpd::Post, "/api/v1/update/local", body, httpd::AuthLevel::Write, "127.0.0.1").code == 403);
	REQUIRE(deps.commands.posted.empty());
}

TEST_CASE("the install route sends what the body names and answers accepted", "[update]")
{
	ShippedRoutes shipped;
	InstalledDependencies deps;
	Posted posted(deps.commands);
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.put("ni-update.bin"));

	const httpd::Response r = asOwner(httpd::Post, "/api/v1/update/local", "{\"name\":\"ni-update.bin\"}");
	REQUIRE(r.code == 202);
	REQUIRE(deps.commands.posted.size() == 1);

	const update::Asked a = update::asked((const char *) deps.commands.posted[0].second);
	REQUIRE(a.valid);
	REQUIRE(a.path == fx.dir + "/ni-update.bin");
}

TEST_CASE("the install route says why it refuses", "[update]")
{
	ShippedRoutes shipped;
	InstalledDependencies deps;
	Fixture fx;
	REQUIRE(fx.ready);
	REQUIRE(fx.put("ni-update.bin"));
	REQUIRE(fx.put("image.zip"));

	const httpd::Response noname = asOwner(httpd::Post, "/api/v1/update/local", "{}");
	REQUIRE(noname.code == 400);

	const httpd::Response path = asOwner(httpd::Post, "/api/v1/update/local", "{\"name\":\"/etc/passwd\"}");
	REQUIRE(path.code == 400);
	REQUIRE(has(path.body, "bad-name"));

	const httpd::Response image = asOwner(httpd::Post, "/api/v1/update/local", "{\"name\":\"image.zip\"}");
	REQUIRE(image.code == 400);
	REQUIRE(has(image.body, "bad-name"));

	const httpd::Response gone = asOwner(httpd::Post, "/api/v1/update/local", "{\"name\":\"other.bin\"}");
	REQUIRE(gone.code == 404);
	REQUIRE(has(gone.body, "no-such-update-file"));

	deps.recordings.add(7, 0x1234, "/media/hdd/movie.ts");
	const httpd::Response busy = asOwner(httpd::Post, "/api/v1/update/local", "{\"name\":\"ni-update.bin\"}");
	REQUIRE(busy.code == 409);
	REQUIRE(has(busy.body, "recording-running"));

	REQUIRE(deps.commands.posted.empty());
}

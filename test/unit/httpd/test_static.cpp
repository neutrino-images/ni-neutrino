/*
 * test_static.cpp - tests for serving static files
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
#include "support/httpclient.h"

#include <config.h>

#include "httpd/webconfig.h"
#include "httpd/endpoint.h"
#include "httpd/router.h"
#include "httpd/server.h"
#include "httpd/static.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <atomic>

#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <zlib.h>

using namespace httpd;

namespace
{

/* The directory the cases serve from, and beside it a second one whose name begins
   with the first. The pair is what a prefix comparison on the text gets wrong, so they
   are made together and taken away together.

   Named with the process, so a second run of this suite, or one sharing this host with
   a running box, plants its tree somewhere this one never touches. */
const std::string kRootName = "coreapi-doc." + std::to_string(getpid());
const std::string kSiblingName = kRootName + "-evil";
const std::string kRoot = "/tmp/" + kRootName;
const std::string kSibling = "/tmp/" + kSiblingName;

const char kPage[] = "<!doctype html>hello\n";
const char kScript[] = "console.log(1)\n";

std::string headerOf(const Response &r, const std::string &name)
{
	for (size_t i = 0; i < r.headers.size(); ++i)
	{
		if (r.headers[i].first.size() != name.size())
			continue;
		bool same = true;
		for (size_t c = 0; c < name.size() && same; ++c)
		{
			char a = r.headers[i].first[c];
			char b = name[c];
			if (a >= 'A' && a <= 'Z')
				a = (char)(a - 'A' + 'a');
			if (b >= 'A' && b <= 'Z')
				b = (char)(b - 'A' + 'a');
			same = (a == b);
		}
		if (same)
			return r.headers[i].second;
	}
	return std::string();
}

void writeFile(const std::string &path, const std::string &bytes)
{
	std::ofstream f(path.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
	REQUIRE(f.good());
	f.write(bytes.data(), (std::streamsize) bytes.size());
	f.close();
}

std::string readFile(const std::string &path)
{
	std::ifstream f(path.c_str(), std::ios::in | std::ios::binary);
	std::ostringstream out;
	out << f.rdbuf();
	return out.str();
}

// The stored form, made here rather than shelled out to a program, so a case
// runs on a box that has no compressor installed and so the bytes on disk are
// known to the case that reads them back.
std::string gzipOf(const std::string &plain)
{
	z_stream s;
	std::memset(&s, 0, sizeof(s));
	REQUIRE(deflateInit2(&s, Z_BEST_SPEED, Z_DEFLATED, 16 + MAX_WBITS, 8,
	                     Z_DEFAULT_STRATEGY) == Z_OK);

	s.next_in = (Bytef *) const_cast<char *>(plain.data());
	s.avail_in = (uInt) plain.size();

	std::string out;
	char buf[32768];
	for (;;)
	{
		s.next_out = (Bytef *) buf;
		s.avail_out = (uInt) sizeof(buf);
		const int rc = deflate(&s, Z_FINISH);
		out.append(buf, sizeof(buf) - (size_t) s.avail_out);
		if (rc == Z_STREAM_END)
			break;
		REQUIRE(rc == Z_OK);
	}
	deflateEnd(&s);
	return out;
}

void removeTrees()
{
	// A link planted inside is unlinked and not walked, which is what this
	// removal has to be true of before a case is allowed to plant one.
	REQUIRE(std::system(("rm -rf " + kRoot + " " + kSibling).c_str()) == 0);
}

/* What every case builds for itself, and takes away whichever line it leaves through.

   Every case rather than one of them: the harness takes a selection, so a case reading
   what the case above it left behind passes only in the order it was written.

   The configuration is installed beside the directory because the server reads the one
   and serves out of the other, and a case that drove the socket without the
   configuration would be driving a server that had just replaced the directory this
   set. */
struct Docroot
{
	// Declared first, so the daemon starts after the seams are filled and stops
	// before they are cleared.
	InstalledDependencies wired_;

	Docroot()
	{
		removeTrees();
		REQUIRE(::mkdir(kRoot.c_str(), 0755) == 0);
		REQUIRE(::mkdir((std::string(kRoot) + "/assets").c_str(), 0755) == 0);
		writeFile(std::string(kRoot) + "/index.html", kPage);
		writeFile(std::string(kRoot) + "/assets/app.9f3.js.gz", gzipOf(kScript));

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
		std::system(("rm -rf " + kRoot + " " + kSibling).c_str());
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

TEST_CASE("a file under the root is served", "[static]")
{
	Docroot tree;
	Response r = serveStatic("/index.html", "", "");
	REQUIRE(r.code == 200);
	REQUIRE(r.body == kPage);
	REQUIRE(r.content_type == "text/html");
}

TEST_CASE("a traversal out of the root is refused", "[static]")
{
	Docroot tree;
	REQUIRE(serveStatic("/../../etc/hostname", "", "").code == 400);
	REQUIRE(serveStatic("/assets/../../../etc/hostname", "", "").code == 400);
	REQUIRE(serveStatic("//etc/hostname", "", "").code == 400);
}

TEST_CASE("a dot segment spelled as an escape is refused too", "[static]")
{
	/* The check is on what the target says and not on how it was typed, which
	   is only true when the decoding happens in front of it. Written out
	   because the case above passes either way: nothing decodes a target that
	   carries no escape. */
	Docroot tree;
	REQUIRE(serveStatic("/%2e%2e/%2e%2e/etc/hostname", "", "").code == 400);
	REQUIRE(serveStatic("/assets/%2E%2E/%2E%2E/%2E%2E/etc/hostname", "", "").code == 400);
	REQUIRE(serveStatic("/assets%2f..%2f..%2f..%2fetc/hostname", "", "").code == 400);
}

TEST_CASE("a name written with an escape reaches the file it names", "[static]")
{
	// The other half of the decoding, so that refusing everything would not
	// read as a pass above.
	Docroot tree;
	writeFile(std::string(kRoot) + "/a b.css", "body{}\n");

	Response r = serveStatic("/a%20b.css", "", "");
	REQUIRE(r.code == 200);
	REQUIRE(r.body == "body{}\n");
	REQUIRE(r.content_type == "text/css");
}

TEST_CASE("a target this server cannot read is refused rather than guessed at", "[static]")
{
	Docroot tree;
	// A truncated escape and one that is not hex are two spellings of one file
	// name only if something picks which, and nothing here picks.
	REQUIRE(serveStatic("/index%2", "", "").code == 400);
	REQUIRE(serveStatic("/index%zz.html", "", "").code == 400);
	// A target that is not a path at all.
	REQUIRE(serveStatic("index.html", "", "").code == 400);
}

TEST_CASE("a target carrying a zero byte is refused", "[static]")
{
	Docroot tree;
	const std::string cut = std::string("/index.html\0.js", 15);
	REQUIRE(serveStatic(cut, "", "").code == 400);
	REQUIRE(serveStatic("/index.html%00.js", "", "").code == 400);
}

TEST_CASE("a symlink pointing out of the root is not followed", "[static]")
{
	// The check is on the resolved path, not on the text, because the docroot
	// is writable on a box and a planted link would otherwise escape.
	Docroot tree;
	REQUIRE(::symlink("/etc", (std::string(kRoot) + "/escape").c_str()) == 0);
	REQUIRE(serveStatic("/escape/hostname", "", "").code == 400);

	// The escaped name is itself a directory (/etc is), and asked for bare: this is the
	// one shape the index fallback's own redirect answers for a name inside the root,
	// and it has to lose to the escape check rather than run ahead of it. A 301 here
	// would confirm to whoever planted this link that the name resolves to a real
	// directory somewhere.
	REQUIRE(serveStatic("/escape", "", "").code == 400);
}

TEST_CASE("a sibling whose name begins with the root is outside it", "[static]")
{
	// A prefix comparison on the text lets a sibling whose name merely begins
	// with the root's own through. The resolved path has to end at the root or
	// continue with a separator.
	Docroot tree;
	REQUIRE(::mkdir(kSibling.c_str(), 0755) == 0);
	writeFile(kSibling + "/x", "secret\n");
	REQUIRE(serveStatic("/../" + kSiblingName + "/x", "", "").code == 400);
}

TEST_CASE("a link to a sibling whose name begins with the root is outside it", "[static]")
{
	/* The case above is answered by the rule that refuses a dot segment before
	   anything is resolved, so it says nothing about the comparison that runs after.
	   This one carries no dot segment and no escape: it is a link planted in a
	   writable directory, and it is the only case here that goes red when that
	   comparison is written as a prefix. */
	Docroot tree;
	REQUIRE(::mkdir(kSibling.c_str(), 0755) == 0);
	writeFile(kSibling + "/x", "secret\n");
	REQUIRE(::symlink(kSibling.c_str(), (kRoot + "/near").c_str()) == 0);

	Response r = serveStatic("/near/x", "", "");
	REQUIRE(r.code == 400);
	REQUIRE(r.body.find("secret") == std::string::npos);
}

TEST_CASE("a client that takes gzip gets the stored file and a Vary", "[static]")
{
	Docroot tree;
	Response r = serveStatic("/assets/app.9f3.js", "gzip, deflate", "");
	REQUIRE(r.code == 200);
	REQUIRE(headerOf(r, "Content-Encoding") == "gzip");
	REQUIRE(headerOf(r, "Vary") == "Accept-Encoding");
	// The type comes from the inner extension, never from the stored one.
	REQUIRE(r.content_type == "text/javascript");
	// As stored and not built again, so that the bytes on the wire are the
	// bytes somebody put in the directory.
	REQUIRE(r.body == readFile(std::string(kRoot) + "/assets/app.9f3.js.gz"));
}

TEST_CASE("a client that does not take gzip gets the same bytes inflated", "[static]")
{
	// Refusing with 406 is not an option: plain curl is the documented path
	// for a plugin.
	Docroot tree;
	Response plain = serveStatic("/assets/app.9f3.js", "", "");
	REQUIRE(plain.code == 200);
	REQUIRE(headerOf(plain, "Content-Encoding").empty());
	REQUIRE(plain.body == kScript);
	REQUIRE(headerOf(plain, "Vary") == "Accept-Encoding");
}

TEST_CASE("an Accept-Encoding that merely contains the word does not count", "[static]")
{
	// "gzip;q=0" is a refusal, and a token inside another word is not a token.
	Docroot tree;
	REQUIRE(headerOf(serveStatic("/assets/app.9f3.js", "gzip;q=0", ""),
	                 "Content-Encoding").empty());
	REQUIRE(headerOf(serveStatic("/assets/app.9f3.js", "notgzipatall", ""),
	                 "Content-Encoding").empty());
	REQUIRE(headerOf(serveStatic("/assets/app.9f3.js", "br, gzip", ""),
	                 "Content-Encoding") == "gzip");
}

TEST_CASE("the weights beside a coding are read and not searched for", "[static]")
{
	/* Driven on the parse itself, because the number of ways of writing a
	   header that must not count is larger than the number of files a case can
	   plant, and every one of them that reads as a yes sends a body the caller
	   said it cannot open. */
	REQUIRE(acceptsGzip("gzip"));
	REQUIRE(acceptsGzip("GZIP"));
	REQUIRE(acceptsGzip(" gzip "));
	REQUIRE(acceptsGzip("br, gzip"));
	REQUIRE(acceptsGzip("gzip;q=1"));
	REQUIRE(acceptsGzip("gzip;q=1.0"));
	REQUIRE(acceptsGzip("gzip; q=0.001"));
	REQUIRE(acceptsGzip("deflate, gzip;q=0.5, *;q=0"));
	REQUIRE(acceptsGzip("*"));
	REQUIRE(acceptsGzip("*;q=0.5"));

	REQUIRE_FALSE(acceptsGzip(""));
	REQUIRE_FALSE(acceptsGzip("identity"));
	REQUIRE_FALSE(acceptsGzip("notgzipatall"));
	REQUIRE_FALSE(acceptsGzip("gzipped"));
	REQUIRE_FALSE(acceptsGzip("x-gzip"));
	REQUIRE_FALSE(acceptsGzip("gzip;q=0"));
	REQUIRE_FALSE(acceptsGzip("gzip;q=0.0"));
	REQUIRE_FALSE(acceptsGzip("gzip;q=0.000"));
	REQUIRE_FALSE(acceptsGzip("GZIP ; Q=0"));
	REQUIRE_FALSE(acceptsGzip("br, gzip;q=0, deflate"));
	// A wildcard says nothing about a coding the same header named.
	REQUIRE_FALSE(acceptsGzip("*, gzip;q=0"));
	REQUIRE_FALSE(acceptsGzip("gzip;q=0, *"));
	REQUIRE_FALSE(acceptsGzip("*;q=0"));
	// A weight nobody can read is a weight whose sense is unknown.
	REQUIRE_FALSE(acceptsGzip("gzip;q=abc"));
	REQUIRE_FALSE(acceptsGzip("gzip;q="));
	REQUIRE_FALSE(acceptsGzip("gzip;q=0.0000"));
	// A caller contradicting itself is read as the refusal.
	REQUIRE_FALSE(acceptsGzip("gzip;q=0, gzip"));
}

TEST_CASE("a weight behind another parameter is still the weight", "[static]")
{
	/* The parameters of one coding are separated by semicolons, and a walk
	   that split them on commas could only ever see the first of them: every
	   refusal written after anything else read as an acceptance, which is the
	   one direction that sends a body the caller cannot open. */
	REQUIRE_FALSE(acceptsGzip("gzip;a=b;q=0"));
	REQUIRE_FALSE(acceptsGzip("gzip;;q=0"));
	REQUIRE_FALSE(acceptsGzip("gzip; a=b ; q=0.0"));
	REQUIRE_FALSE(acceptsGzip("br, gzip;x=1;q=0, deflate"));
	// And a parameter that is not a weight leaves the coding acceptable.
	REQUIRE(acceptsGzip("gzip;a=b"));
	REQUIRE(acceptsGzip("gzip;a=b;q=1"));
}

TEST_CASE("a refusal written behind another parameter is honoured on the wire", "[static]")
{
	Docroot tree;
	REQUIRE(headerOf(serveStatic("/assets/app.9f3.js", "gzip;a=b;q=0", ""),
	                 "Content-Encoding").empty());
	REQUIRE(serveStatic("/assets/app.9f3.js", "gzip;a=b;q=0", "").body == kScript);
}

TEST_CASE("a hashed asset is immutable and the page is not", "[static]")
{
	Docroot tree;
	REQUIRE(headerOf(serveStatic("/assets/app.9f3.js", "gzip", ""), "Cache-Control")
	        == "public, max-age=31536000, immutable");
	REQUIRE(headerOf(serveStatic("/index.html", "", ""), "Cache-Control") == "no-cache");
	REQUIRE_FALSE(headerOf(serveStatic("/index.html", "", ""), "ETag").empty());
}

TEST_CASE("a matching ETag answers 304 with no body", "[static]")
{
	Docroot tree;
	const std::string tag = headerOf(serveStatic("/index.html", "", ""), "ETag");
	Response r = serveStatic("/index.html", "", tag);
	REQUIRE(r.code == 304);
	REQUIRE(r.body.empty());
	// The answer still says what it is answering about, or the caller has
	// nothing to ask with the next time.
	REQUIRE(headerOf(r, "ETag") == tag);
	REQUIRE(headerOf(r, "Vary") == "Accept-Encoding");
}

TEST_CASE("a validator that is not the one held answers the body", "[static]")
{
	// Without this the case above passes for a server that answers 304 to
	// anything carrying the header at all, which is a page that never updates.
	Docroot tree;
	Response r = serveStatic("/index.html", "", "\"not-the-one\"");
	REQUIRE(r.code == 200);
	REQUIRE(r.body == kPage);

	const std::string tag = headerOf(serveStatic("/index.html", "", ""), "ETag");
	// One of a list, and a weak marker, are both the tag that was handed out.
	REQUIRE(serveStatic("/index.html", "", "\"other\", " + tag).code == 304);
	REQUIRE(serveStatic("/index.html", "", "W/" + tag).code == 304);
}

TEST_CASE("the stored form and the answer built out of it are told apart", "[static]")
{
	/* Two bodies under one name need two validators, or a cache that does not
	   honour the header saying the answer varies hands the compressed bytes to
	   the caller that asked for the plain ones. */
	Docroot tree;
	const std::string stored = headerOf(serveStatic("/assets/app.9f3.js", "gzip", ""), "ETag");
	const std::string plain = headerOf(serveStatic("/assets/app.9f3.js", "", ""), "ETag");
	REQUIRE_FALSE(stored.empty());
	REQUIRE_FALSE(plain.empty());
	REQUIRE(stored != plain);

	// And each of them is answered about the representation it belongs to.
	Response r = serveStatic("/assets/app.9f3.js", "gzip", stored);
	REQUIRE(r.code == 304);
	REQUIRE(serveStatic("/assets/app.9f3.js", "", stored).code == 200);
}

TEST_CASE("two representations of one file do not share one validator", "[static]")
{
	/* The shape that catches a mark put on the code path rather than on the
	   coding: both forms installed, at one size and one time, which is what a
	   build that copies the source date produces. Without the encoding in the
	   validator a client holding the compressed bytes asks without saying it
	   takes them, is told its copy is current, and renders gzip as a script. */
	Docroot tree;
	const std::string stored_bytes = gzipOf("console.log(2)\n");
	const std::string plain = std::string(kRoot) + "/assets/coll.js";
	const std::string coll = plain + ".gz";
	writeFile(coll, stored_bytes);
	// Padded to the length of the stored form, so the two differ in nothing
	// the validator reads.
	writeFile(plain, std::string(stored_bytes.size(), 'x'));

	struct timeval when[2];
	when[0].tv_sec = 1577836800;
	when[0].tv_usec = 0;
	when[1] = when[0];
	REQUIRE(::utimes(plain.c_str(), when) == 0);
	REQUIRE(::utimes(coll.c_str(), when) == 0);

	const std::string gz_tag = headerOf(serveStatic("/assets/coll.js", "gzip", ""), "ETag");
	const std::string id_tag = headerOf(serveStatic("/assets/coll.js", "", ""), "ETag");
	REQUIRE_FALSE(gz_tag.empty());
	REQUIRE(gz_tag != id_tag);

	// And neither validator answers for the other representation.
	REQUIRE(serveStatic("/assets/coll.js", "", gz_tag).code == 200);
	REQUIRE(serveStatic("/assets/coll.js", "gzip", id_tag).code == 200);
	// Each still answers for its own.
	REQUIRE(serveStatic("/assets/coll.js", "gzip", gz_tag).code == 304);
	REQUIRE(serveStatic("/assets/coll.js", "", id_tag).code == 304);
}

TEST_CASE("a stored form pointing out of the root does not take the page with it", "[static]")
{
	/* Nobody asked for the stored form by name, so what it resolves to says
	   nothing about the file beside it. Refusing over one would let anybody who
	   can write into the directory take any page off the air by planting a link
	   named after it. */
	Docroot tree;
	writeFile(std::string(kRoot) + "/site.css", "body{}\n");
	REQUIRE(::symlink("/etc/hostname", (std::string(kRoot) + "/site.css.gz").c_str()) == 0);

	Response plain = serveStatic("/site.css", "", "");
	REQUIRE(plain.code == 200);
	REQUIRE(plain.body == "body{}\n");

	Response asked = serveStatic("/site.css", "gzip", "");
	REQUIRE(asked.code == 200);
	REQUIRE(asked.body == "body{}\n");
	REQUIRE(headerOf(asked, "Content-Encoding").empty());

	// And with no plain file beside it, what is answered is absence and not a
	// refusal that says a name in the directory points somewhere.
	REQUIRE(::symlink("/etc/hostname", (std::string(kRoot) + "/gone.css.gz").c_str()) == 0);
	REQUIRE(serveStatic("/gone.css", "gzip", "").code == 404);
}

TEST_CASE("every member of a stored form is answered, or none of it is", "[static]")
{
	/* The format is a sequence of members and concatenating two of these files
	   makes one. Answering the first member alone is a partial body sent as
	   though it were whole, and the identity answer would then differ from the
	   stored one for one target. */
	Docroot tree;
	writeFile(std::string(kRoot) + "/multi.txt.gz", gzipOf("first\n") + gzipOf("second\n"));
	Response both = serveStatic("/multi.txt", "", "");
	REQUIRE(both.code == 200);
	REQUIRE(both.body == "first\nsecond\n");

	// A tail that is not another member is a file this cannot read whole.
	writeFile(std::string(kRoot) + "/tail.txt.gz", gzipOf("first\n") + "TRAILING");
	REQUIRE(serveStatic("/tail.txt", "", "").code == 500);

	writeFile(std::string(kRoot) + "/pad.txt.gz", gzipOf("first\n") + std::string(8, '\0'));
	REQUIRE(serveStatic("/pad.txt", "", "").code == 500);
}

TEST_CASE("the plain file is preferred over expanding the stored one", "[static]")
{
	/* Both forms in the directory and a caller that takes neither compressed:
	   the bytes are the ones on disk and no time is spent expanding a file the
	   directory already holds in that shape. */
	Docroot tree;
	writeFile(std::string(kRoot) + "/assets/both.js", "plain\n");
	writeFile(std::string(kRoot) + "/assets/both.js.gz", gzipOf("stored\n"));

	Response identity = serveStatic("/assets/both.js", "", "");
	REQUIRE(identity.code == 200);
	REQUIRE(identity.body == "plain\n");
	REQUIRE(headerOf(identity, "Content-Encoding").empty());

	Response compressed = serveStatic("/assets/both.js", "gzip", "");
	REQUIRE(compressed.code == 200);
	REQUIRE(headerOf(compressed, "Content-Encoding") == "gzip");
}

TEST_CASE("the stored form is not answered under its own name", "[static]")
{
	/* One file has one name here. Answered under the type of what is inside it
	   the caller would be handed compressed bytes labelled as a script, and
	   answered under a type of its own it would be a second name for one
	   resource with a cache entry of its own. */
	Docroot tree;
	REQUIRE(serveStatic("/assets/app.9f3.js.gz", "gzip", "").code == 404);
	REQUIRE(serveStatic("/assets/app.9f3.js.gz", "", "").code == 404);
}

TEST_CASE("a directory is not a file", "[static]")
{
	Docroot tree;
	// Not a file under either spelling, and the two say different things
	// about it: bare, it is sent to add the separator (see the index
	// fallback's own cases); with the separator already there, it is
	// answered as whatever is named index.html inside it, or as absent when
	// nothing there is.
	Response bare = serveStatic("/assets", "", "");
	REQUIRE(bare.code == 301);
	REQUIRE(headerOf(bare, "Location") == "/assets/");
	// assets/ holds a stored script and nothing named index.html, so the
	// separator alone does not turn it into a page.
	REQUIRE(serveStatic("/assets/", "", "").code == 404);
	// The root is a directory too, and the one this fixture plants an
	// index.html directly under: the fallback reaches it exactly as it
	// would any other, which is what tells this apart from a rule written
	// for subdirectories alone.
	Response root = serveStatic("/", "", "");
	REQUIRE(root.code == 200);
	REQUIRE(root.body == kPage);
}

TEST_CASE("a file written with a separator after it is not a second name for it", "[static]")
{
	/* A target ending in a separator names a directory. Serving the file under
	   it as well would be one answer at two addresses, which is two cache
	   entries, and a browser resolving a relative reference from the second
	   one would resolve it as though the file were a directory. */
	Docroot tree;
	REQUIRE(serveStatic("/index.html", "", "").code == 200);
	REQUIRE(serveStatic("/index.html/", "", "").code == 404);
	REQUIRE(serveStatic("/assets/app.9f3.js/", "", "").code == 404);
}

TEST_CASE("a missing file is 404 and says nothing about the filesystem", "[static]")
{
	Docroot tree;
	Response r = serveStatic("/nope.js", "", "");
	REQUIRE(r.code == 404);
	REQUIRE(r.body.find(kRoot) == std::string::npos);
	REQUIRE(r.content_type == "application/problem+json");
}

TEST_CASE("a refusal says how long it may be kept", "[static]")
{
	/* A 404 is cacheable on a guess, and the 404 this answers is the answer for
	   a page that is not installed yet, so a cache free to invent a lifetime
	   for one goes on answering it after the page is there. */
	Docroot tree;
	REQUIRE(headerOf(serveStatic("/nope.js", "", ""), "Cache-Control") == "no-cache");
	REQUIRE(headerOf(serveStatic("/../../etc/hostname", "", ""), "Cache-Control") == "no-cache");
	REQUIRE(headerOf(serveStatic("/index.html%00", "", ""), "Cache-Control") == "no-cache");
}

TEST_CASE("a 304 states no length of its own", "[static]")
{
	/* A 304 may state the length the answer would have carried, and this one
	   does not, because the transport states its own for what it is handed.
	   Two of them is not a header read twice: a browser refuses the whole
	   answer, and the page stops loading on the second visit. */
	Docroot tree;
	const std::string page = headerOf(serveStatic("/index.html", "", ""), "ETag");
	Response r = serveStatic("/index.html", "", page);
	REQUIRE(r.code == 304);
	REQUIRE(headerOf(r, "Content-Length").empty());
	REQUIRE(std::string(kPage).size() == 21u);

	/* The one that used to be worked out rather than read off the file: the
	   length of a stored form is written nowhere, so stating it meant
	   expanding the whole file on the one answer that carries no body. That
	   work is gone with the header. */
	const std::string tag = headerOf(serveStatic("/assets/app.9f3.js", "", ""), "ETag");
	Response inflated = serveStatic("/assets/app.9f3.js", "", tag);
	REQUIRE(inflated.code == 304);
	REQUIRE(headerOf(inflated, "Content-Length").empty());
	REQUIRE(std::string(kScript).size() == 15u);

	// And the stored one says no more than the expanded one.
	const std::string stored_tag = headerOf(serveStatic("/assets/app.9f3.js", "gzip", ""), "ETag");
	Response stored = serveStatic("/assets/app.9f3.js", "gzip", stored_tag);
	REQUIRE(stored.code == 304);
	REQUIRE(headerOf(stored, "Content-Length").empty());
}

TEST_CASE("a box with no directory configured serves nothing", "[static]")
{
	Docroot tree;
	setDocroot("");
	REQUIRE(serveStatic("/index.html", "", "").code == 404);
	setDocroot("/tmp/coreapi-doc-that-is-not-there");
	REQUIRE(serveStatic("/index.html", "", "").code == 404);
}

TEST_CASE("the default docroot names this server's own directory, and the docs page in it is served compressed either way", "[static]")
{
	/* A fresh box runs on defaultWebConfig() until somebody writes a file, so
	   this is the docroot every box this tree builds actually serves from
	   until then. A default still naming the empty string would leave that
	   box answering 404 for a page this tree installs onto it. */
	REQUIRE(defaultWebConfig().docroot == std::string(PRIVATE_HTTPDDIR) + "/ni-web");

	/* Shaped the way installation leaves it and not the way a plain test fixture
	   would: only the stored form on disk, because that is what the package ships to
	   save the flash the other copy would cost, and under swagger/ rather than at the
	   root. What is under test is the inflate-on-demand path this server takes when
	   nothing but the stored form exists. */
	const std::string root = "/tmp/coreapi-doc-default." + std::to_string(::getpid());
	REQUIRE(std::system(("rm -rf " + root).c_str()) == 0);
	REQUIRE(::mkdir(root.c_str(), 0755) == 0);
	REQUIRE(::mkdir((root + "/swagger").c_str(), 0755) == 0);
	writeFile(root + "/swagger/index.html.gz", gzipOf(kPage));
	setDocroot(root);

	Response stored = serveStatic("/swagger/index.html", "gzip", "");
	REQUIRE(stored.code == 200);
	REQUIRE(headerOf(stored, "Content-Encoding") == "gzip");
	REQUIRE(stored.body == gzipOf(kPage));

	Response inflated = serveStatic("/swagger/index.html", "", "");
	REQUIRE(inflated.code == 200);
	REQUIRE(headerOf(inflated, "Content-Encoding").empty());
	REQUIRE(inflated.body == kPage);

	setDocroot("");
	REQUIRE(std::system(("rm -rf " + root).c_str()) == 0);
}

TEST_CASE("a directory asked for with the separator answers with its own index, stored form and all", "[static]")
{
	/* The one thing this case is written to catch: that the index fallback goes
	   through the very same resolution and negotiation serveStatic already runs a
	   plain name through. A mutation that answered a bare 200 with an empty body, or
	   that skipped the stored form and always inflated, would still pass a case that
	   only checked the code. */
	Docroot tree;
	REQUIRE(::mkdir((std::string(kRoot) + "/swagger").c_str(), 0755) == 0);
	writeFile(std::string(kRoot) + "/swagger/index.html.gz", gzipOf(kPage));

	Response stored = serveStatic("/swagger/", "gzip", "");
	REQUIRE(stored.code == 200);
	REQUIRE(headerOf(stored, "Content-Encoding") == "gzip");
	REQUIRE(stored.body == gzipOf(kPage));

	Response inflated = serveStatic("/swagger/", "", "");
	REQUIRE(inflated.code == 200);
	REQUIRE(headerOf(inflated, "Content-Encoding").empty());
	REQUIRE(inflated.body == kPage);

	// A directory this holds but with nothing named index.html in it answers
	// exactly as a directory it does not hold at all: the two are one answer
	// everywhere else in this file, and the fallback does not carve out an
	// exception for itself.
	REQUIRE(::mkdir((std::string(kRoot) + "/empty").c_str(), 0755) == 0);
	REQUIRE(serveStatic("/empty/", "", "").code == 404);
	REQUIRE(serveStatic("/nowhere-at-all/", "", "").code == 404);
}

TEST_CASE("a directory asked for without the separator is sent to add it, and not served under the wrong name", "[static]")
{
	/* The page inside answers itself with relative names ("vendor/..."), so
	   a caller holding this address without the separator would go on to ask
	   for those against the wrong place. The redirect is what a browser
	   needs before it ever reads a byte of what is inside. */
	Docroot tree;
	REQUIRE(::mkdir((std::string(kRoot) + "/swagger").c_str(), 0755) == 0);
	writeFile(std::string(kRoot) + "/swagger/index.html.gz", gzipOf(kPage));

	Response bare = serveStatic("/swagger", "", "");
	REQUIRE(bare.code == 301);
	REQUIRE(headerOf(bare, "Location") == "/swagger/");
	// Not the page itself: a redirect is the whole of what this answer is
	// allowed to be, and a body here would be a second, unasked-for way of
	// answering the same request.
	REQUIRE(bare.body.empty());

	// A name that is a file and not a directory is answered as the file it
	// is, whether or not it is later asked for with a separator after it
	// (that is the older, narrower rule, checked elsewhere in this file) --
	// the redirect is only for a name this directory actually holds as a
	// directory.
	REQUIRE(serveStatic("/index.html", "", "").code == 200);

	// A name this directory does not hold at all, with or without the
	// separator, is 404 and never a redirect: redirecting it would tell a
	// caller that guessed a directory name that the guess landed, which is
	// the same leak the rest of this file refuses elsewhere.
	REQUIRE(serveStatic("/nowhere-at-all", "", "").code == 404);
}

TEST_CASE("what the pages answer says its type is not to be guessed at", "[static]")
{
	/* The one place this server cannot state a true type is a file in here whose
	   extension the table does not name, which goes out as bytes of no stated kind,
	   and this directory is readable by anyone who reaches the port. So the header is
	   on every answer built here: a rule that applied to some answers is a rule
	   somebody has to decide about per file. The three shapes, because they are built
	   in three places. */
	Docroot tree;
	writeFile(std::string(kRoot) + "/thing.unknown", "bytes\n");

	Response served = serveStatic("/thing.unknown", "", "");
	REQUIRE(served.code == 200);
	REQUIRE(std::string(served.content_type) == "application/octet-stream");
	REQUIRE(headerOf(served, "X-Content-Type-Options") == "nosniff");

	Response missing = serveStatic("/not-here.html", "", "");
	REQUIRE(missing.code == 404);
	REQUIRE(headerOf(missing, "X-Content-Type-Options") == "nosniff");

	Response fresh = serveStatic("/thing.unknown", "", headerOf(served, "ETag"));
	REQUIRE(fresh.code == 304);
	REQUIRE(headerOf(fresh, "X-Content-Type-Options") == "nosniff");
}

TEST_CASE("no page out of here may be framed by a page from somewhere else", "[static]")
{
	/* This is the control panel of a box. A page somewhere else that can put it in a
	   frame can lay its own surface over it and collect a click that lands on a button
	   here, and the second token answers none of that: it stops a request that carries
	   a body, not a click on a page a browser already holds the session for.

	   The directive has to arrive as a header: the page states the rest of what it
	   allows in a meta element, and framing is the one rule a meta element does not
	   carry. All four shapes this file builds, because the rule is about the surface
	   and not about what one answer carries. */
	Docroot tree;
	writeFile(std::string(kRoot) + "/thing.unknown", "bytes\n");
	REQUIRE(::mkdir((std::string(kRoot) + "/swagger").c_str(), 0755) == 0);
	writeFile(std::string(kRoot) + "/swagger/index.html", kPage);

	Response served = serveStatic("/thing.unknown", "", "");
	REQUIRE(served.code == 200);
	REQUIRE(headerOf(served, "Content-Security-Policy") == "frame-ancestors 'self'");
	REQUIRE(headerOf(served, "X-Frame-Options") == "SAMEORIGIN");

	Response missing = serveStatic("/not-here.html", "", "");
	REQUIRE(missing.code == 404);
	REQUIRE(headerOf(missing, "Content-Security-Policy") == "frame-ancestors 'self'");
	REQUIRE(headerOf(missing, "X-Frame-Options") == "SAMEORIGIN");

	Response fresh = serveStatic("/thing.unknown", "", headerOf(served, "ETag"));
	REQUIRE(fresh.code == 304);
	REQUIRE(headerOf(fresh, "Content-Security-Policy") == "frame-ancestors 'self'");
	REQUIRE(headerOf(fresh, "X-Frame-Options") == "SAMEORIGIN");

	/* The redirect included. A browser that is being sent one hop further is
	   a browser about to load the page at the end of it, and an answer that
	   said nothing here would be a hop a framer could aim at instead. */
	Response bare = serveStatic("/swagger", "", "");
	REQUIRE(bare.code == 301);
	REQUIRE(headerOf(bare, "Content-Security-Policy") == "frame-ancestors 'self'");
	REQUIRE(headerOf(bare, "X-Frame-Options") == "SAMEORIGIN");
}

TEST_CASE("a file larger than this server sends is not read into memory", "[static]")
{
	/* The directory is writable on a box, so the size of what is in it is not this
	   server's to assume. Made sparse, because what is being checked is the size the
	   answer would take and not the bytes.

	   Both sides of the boundary and not only the far one: a number checked from one
	   side could be refusing everything, and the file at the ceiling exactly is what
	   says sixteen megabytes is what the guard means. */
	Docroot tree;
	const size_t ceiling = 16u * 1024u * 1024u;
	const std::string big = std::string(kRoot) + "/big.bin";

	{
		const int fd = ::open(big.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
		REQUIRE(fd >= 0);
		REQUIRE(::ftruncate(fd, (off_t)(ceiling + 1)) == 0);
		::close(fd);
	}

	Response over = serveStatic("/big.bin", "", "");
	REQUIRE(over.code == 500);
	REQUIRE(over.body.find("output-too-large") != std::string::npos);
	// Nothing of the file reached the answer.
	REQUIRE(over.body.size() < 1024);

	{
		const int fd = ::open(big.c_str(), O_RDWR | O_TRUNC, 0644);
		REQUIRE(fd >= 0);
		REQUIRE(::ftruncate(fd, (off_t) ceiling) == 0);
		::close(fd);
	}

	Response at = serveStatic("/big.bin", "", "");
	REQUIRE(at.code == 200);
	REQUIRE(at.body.size() == ceiling);
}

TEST_CASE("a stored form that expands past what this sends is stopped while it expands", "[static]")
{
	/* How much a stored form expands into is written by whoever wrote it and
	   cannot be read off the file, so a ceiling applied to the result is a
	   ceiling applied after the memory has already been taken. */
	Docroot tree;
	writeFile(std::string(kRoot) + "/bomb.txt.gz",
	          gzipOf(std::string(20u * 1024u * 1024u, 'a')));

	Response r = serveStatic("/bomb.txt", "", "");
	REQUIRE(r.code == 500);
	REQUIRE(r.body.find("output-too-large") != std::string::npos);
}

TEST_CASE("a stored form that is not one is not answered as a body", "[static]")
{
	Docroot tree;
	writeFile(std::string(kRoot) + "/broken.css.gz", "this is not a stored form\n");

	Response r = serveStatic("/broken.css", "", "");
	REQUIRE(r.code == 500);
	// And a caller that takes it stored still gets the bytes, which is the
	// only reading under which the file is what it says it is.
	REQUIRE(serveStatic("/broken.css", "gzip", "").code == 200);
}

TEST_CASE("the type table covers what the frontend ships", "[static]")
{
	REQUIRE(std::string(contentTypeFor("a.html")) == "text/html");
	REQUIRE(std::string(contentTypeFor("a.js"))   == "text/javascript");
	REQUIRE(std::string(contentTypeFor("a.css"))  == "text/css");
	REQUIRE(std::string(contentTypeFor("a.json")) == "application/json");
	REQUIRE(std::string(contentTypeFor("a.svg"))  == "image/svg+xml");
	REQUIRE(std::string(contentTypeFor("a.woff2"))== "font/woff2");
	REQUIRE(std::string(contentTypeFor("a.png"))  == "image/png");
	// The old server shipped javascript as text/plain, which browsers now
	// refuse to execute from a module tag.
	REQUIRE(std::string(contentTypeFor("a.js")) != "text/plain");
	REQUIRE(std::string(contentTypeFor("a.unknown")) == "application/octet-stream");
}

TEST_CASE("the type is read from under the stored extension", "[static]")
{
	REQUIRE(std::string(contentTypeFor("a.js.gz")) == "text/javascript");
	REQUIRE(std::string(contentTypeFor("a.CSS")) == "text/css");
	REQUIRE(std::string(contentTypeFor("/var/www/assets/app.9f3.js")) == "text/javascript");
	// Nothing underneath is nothing to read a type from.
	REQUIRE(std::string(contentTypeFor("a.gz")) == "application/octet-stream");
	REQUIRE(std::string(contentTypeFor(".gz")) == "application/octet-stream");
	REQUIRE(std::string(contentTypeFor("noextension")) == "application/octet-stream");
	REQUIRE(std::string(contentTypeFor(".bashrc")) == "application/octet-stream");
	REQUIRE(std::string(contentTypeFor("")) == "application/octet-stream");
}

TEST_CASE("the pages are what a path no route names falls through to", "[static]")
{
	Docroot tree;
	setRoutesForTest(NULL);
	REQUIRE(start(loopback()));

	testhttp::Reply r = testhttp::request(boundPort(), "GET", "/index.html");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.body == kPage);
	REQUIRE(r.header("Content-Type") == "text/html");
	REQUIRE(r.header("Vary") == "Accept-Encoding");
	REQUIRE_FALSE(r.header("ETag").empty());
}

TEST_CASE("the directory the server serves from is the one its configuration names", "[static]")
{
	/* The daemon installs it while it is starting, so a case that only planted
	   the files would be driving a server pointed somewhere else. */
	Docroot tree;
	setDocroot("/tmp/coreapi-doc-that-is-not-there");
	setRoutesForTest(NULL);
	REQUIRE(start(loopback()));
	REQUIRE(docroot() == std::string(kRoot));

	testhttp::Reply r = testhttp::request(boundPort(), "GET", "/index.html");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
}

TEST_CASE("what a caller says it takes decides what comes off the socket", "[static]")
{
	Docroot tree;
	setRoutesForTest(NULL);
	REQUIRE(start(loopback()));

	std::vector<std::pair<std::string, std::string> > takes;
	takes.push_back(std::make_pair(std::string("Accept-Encoding"), std::string("gzip")));

	testhttp::Reply stored = testhttp::request(boundPort(), "GET", "/assets/app.9f3.js", takes);
	REQUIRE(stored.transport_ok);
	REQUIRE(stored.code == 200);
	REQUIRE(stored.header("Content-Encoding") == "gzip");
	REQUIRE(stored.header("Content-Type") == "text/javascript");
	REQUIRE(stored.header("Cache-Control") == "public, max-age=31536000, immutable");

	std::vector<std::pair<std::string, std::string> > refuses;
	refuses.push_back(std::make_pair(std::string("Accept-Encoding"), std::string("gzip;q=0")));

	testhttp::Reply plain = testhttp::request(boundPort(), "GET", "/assets/app.9f3.js", refuses);
	REQUIRE(plain.transport_ok);
	REQUIRE(plain.code == 200);
	REQUIRE(plain.header("Content-Encoding").empty());
	REQUIRE(plain.body == kScript);
	REQUIRE(plain.header("Vary") == "Accept-Encoding");
}

TEST_CASE("a 304 off the socket states the length and sends no body", "[static]")
{
	Docroot tree;
	setRoutesForTest(NULL);
	REQUIRE(start(loopback()));

	testhttp::Reply first = testhttp::request(boundPort(), "GET", "/index.html");
	REQUIRE(first.transport_ok);
	REQUIRE(first.code == 200);
	const std::string tag = first.header("ETag");
	REQUIRE_FALSE(tag.empty());

	std::vector<std::pair<std::string, std::string> > holds;
	holds.push_back(std::make_pair(std::string("If-None-Match"), tag));

	testhttp::Reply again = testhttp::request(boundPort(), "GET", "/index.html", holds);
	REQUIRE(again.transport_ok);
	REQUIRE(again.code == 304);
	REQUIRE(again.body.empty());
	/* Counted and not read. header() answers the first of a name, so a case
	   comparing its value passes while a second one stands beside it, which
	   is exactly the answer a browser throws away whole. What is asked here
	   is how many there are. */
	size_t lengths = 0;
	for (size_t i = 0; i < again.headers.size(); ++i)
	{
		std::string name = again.headers[i].first;
		for (size_t j = 0; j < name.size(); ++j)
			name[j] = (char) std::tolower((unsigned char) name[j]);
		if (name == "content-length")
			++lengths;
	}
	REQUIRE(lengths == 1);
	REQUIRE(first.body.size() == 21u);
	REQUIRE(again.header("Vary") == "Accept-Encoding");
}

TEST_CASE("a head of a page is the head of the answer to a get", "[static]")
{
	Docroot tree;
	setRoutesForTest(NULL);
	REQUIRE(start(loopback()));

	testhttp::Reply r = testhttp::request(boundPort(), "HEAD", "/index.html");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.header("Content-Type") == "text/html");
	REQUIRE_FALSE(r.header("ETag").empty());
	REQUIRE(r.body.empty());
}

TEST_CASE("a path the pages do not hold is answered by the pages", "[static]")
{
	Docroot tree;
	setRoutesForTest(NULL);
	REQUIRE(start(loopback()));

	testhttp::Reply r = testhttp::request(boundPort(), "GET", "/nowhere.js");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 404);
	REQUIRE(r.header("Content-Type") == "application/problem+json");
	REQUIRE(r.body.find("no-such-route") != std::string::npos);
	// The document is the one the router sends word for word, and it says how
	// long it may be kept, which the router's does not.
	REQUIRE(r.header("Cache-Control") == "no-cache");

	// And a refusal the pages made reaches the caller as the refusal it is.
	testhttp::Reply out = testhttp::request(boundPort(), "GET", "/%2e%2e/etc/hostname");
	REQUIRE(out.transport_ok);
	REQUIRE(out.code == 400);
}

TEST_CASE("a path under the API is never answered out of the directory", "[static]")
{
	/* Everything under there is the router's, whether or not a route is declared at
	   the name asked for: one that is answers out of the box, and one that is not
	   answers the refusal the router writes. A directory holding a file by either name
	   must answer over neither. The source is installed because the route below reads
	   the box and the facade ends the process for a caller that finds none, which is
	   the order the program keeps as well. */
	FakeChannelSource channels;
	InstalledChannelSource installed(&channels);

	Docroot tree;
	REQUIRE(::mkdir((std::string(kRoot) + "/api").c_str(), 0755) == 0);
	REQUIRE(::mkdir((std::string(kRoot) + "/api/v1").c_str(), 0755) == 0);
	writeFile(std::string(kRoot) + "/api/v1/channels", "not the api\n");
	writeFile(std::string(kRoot) + "/api/v1/nothing", "not the api either\n");

	setRoutesForTest(NULL);
	REQUIRE(start(loopback()));

	testhttp::Reply r = testhttp::request(boundPort(), "GET", "/api/v1/channels");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.body.find("not the api") == std::string::npos);
	REQUIRE(r.header("Content-Type") == "application/json");

	testhttp::Reply gone = testhttp::request(boundPort(), "GET", "/api/v1/nothing");
	REQUIRE(gone.transport_ok);
	REQUIRE(gone.code == 404);
	REQUIRE(gone.body.find("not the api") == std::string::npos);
	REQUIRE(gone.header("Content-Type") == "application/problem+json");
}

TEST_CASE("a method that changes something is not answered by a file", "[static]")
{
	Docroot tree;
	setRoutesForTest(NULL);
	REQUIRE(start(loopback()));

	testhttp::Reply r = testhttp::request(boundPort(), "POST", "/index.html");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 404);
	REQUIRE(r.body.find(kPage) == std::string::npos);
}

namespace
{

/* A second thread putting two different files under one name, which is what
   somebody who can write into the directory can do while the server is
   answering out of it. Both are always under the name and never neither: a
   rename replaces what is there in one step, so the server never sees the name
   missing and every answer it gives is an answer about one of the two. */
struct Swapper
{
	std::string real;
	std::string stage;
	std::string target;
	std::atomic<bool> stop;

	Swapper() : stop(false) {}

private:
	Swapper(const Swapper &);
	Swapper &operator=(const Swapper &);
};

void *swapNames(void *arg)
{
	Swapper *s = (Swapper *) arg;
	while (!s->stop.load())
	{
		::unlink(s->stage.c_str());
		if (::link(s->real.c_str(), s->stage.c_str()) == 0)
			::rename(s->stage.c_str(), s->target.c_str());

		::unlink(s->stage.c_str());
		if (::symlink("/etc/hostname", s->stage.c_str()) == 0)
			::rename(s->stage.c_str(), s->target.c_str());
	}
	return NULL;
}

/* The swap a racer has to win a race to make, made through the seam instead: the
   directory on the way to the file becomes a link to somewhere else, in the moment
   between the name being checked and the file being opened. Once per case and counted,
   so a case asserting a refusal can also assert there was something to refuse. */
std::string swap_dir;
std::string swap_target;
int swaps_made = 0;

void swapDirectoryUnderneath()
{
	if (swaps_made > 0)
		return;
	++swaps_made;
	if (::rename(swap_dir.c_str(), (swap_dir + ".real").c_str()) != 0)
		return;
	::symlink(swap_target.c_str(), swap_dir.c_str());
}

// The seam is cleared however the case leaves, a failed check included: one
// left installed would swap a directory under every case after it.
struct ProbeOff
{
	~ProbeOff() { setResolvedProbeForTest(NULL); }
};

} // namespace

TEST_CASE("a directory swapped under a checked name is not served", "[static]")
{
	/* What the descriptor being held to the device and inode the resolution saw
	   refuses, and the only thing that refuses it. The open refuses a last component
	   that has become a link and cannot see a directory on the way that has: it walks
	   the new link and lands on a file nothing here ever checked. The name is the same
	   name both times, so nothing about reading the name again would say so.

	   Driven through the seam rather than by racing. The rate a race of this shape
	   lands at was measured at 4945 in 400000, and a case at that rate passes when the
	   check it is about has been removed. */
	Docroot tree;

	const std::string dir = std::string(kRoot) + "/d";
	REQUIRE(::mkdir(dir.c_str(), 0755) == 0);
	writeFile(dir + "/page.txt", "inside\n");

	REQUIRE(::mkdir(kSibling.c_str(), 0755) == 0);
	writeFile(std::string(kSibling) + "/page.txt", "elsewhere\n");

	// The control. Without it a refusal below would say nothing: a tree this
	// server would not serve anyway is refused whatever holds the descriptor.
	Response before = serveStatic("/d/page.txt", "", "");
	REQUIRE(before.code == 200);
	REQUIRE(before.body == "inside\n");

	swap_dir = dir;
	swap_target = kSibling;
	swaps_made = 0;

	Response after;
	{
		ProbeOff off;
		setResolvedProbeForTest(&swapDirectoryUnderneath);
		after = serveStatic("/d/page.txt", "", "");
	}

	// The swap landed and what is behind the name now is a file that would
	// otherwise be served whole, so what refuses it is the identity check and
	// nothing about the name.
	REQUIRE(swaps_made == 1);
	struct stat st;
	REQUIRE(::lstat(dir.c_str(), &st) == 0);
	REQUIRE(S_ISLNK(st.st_mode));
	REQUIRE(readFile(dir + "/page.txt") == "elsewhere\n");

	REQUIRE(after.code != 200);
	REQUIRE(after.body.find("elsewhere") == std::string::npos);
	/* Absence and not a refusal of its own, so that whoever swapped it learns
	   nothing about whether the swap landed. */
	REQUIRE(after.code == 404);
}

TEST_CASE("a name swapped between the check and the read is not served", "[static]")
{
	/* The confinement asks the filesystem about a name and then reads a file. If what
	   is read is found by asking a second time, whoever can write into the directory
	   can answer differently the two times. That is not a planted link, which
	   resolution catches, it is a link planted between the two questions, and only the
	   descriptor being held to the identity the first question saw refuses it. */
	Docroot tree;
	const std::string real = std::string(kRoot) + "/real.txt";
	writeFile(real, "real\n");

	Swapper s;
	s.real = real;
	s.stage = std::string(kRoot) + "/stage";
	s.target = std::string(kRoot) + "/race.txt";

	pthread_t racer;
	REQUIRE(::pthread_create(&racer, NULL, &swapNames, &s) == 0);

	int served = 0;
	int escaped = 0;
	for (int i = 0; i < 40000; ++i)
	{
		Response r = serveStatic("/race.txt", "", "");
		if (r.code != 200)
			continue;
		++served;
		if (r.body != "real\n")
			++escaped;
	}

	s.stop.store(true);
	::pthread_join(racer, NULL);

	// Not one answer carried anything but the file that is inside the
	// directory, whatever the name pointed at when it was read.
	REQUIRE(escaped == 0);
	// And the run did serve that file, or the count above counted nothing.
	REQUIRE(served > 0);
}

namespace
{

/* The second root, put back whatever a case leaves through. It is the
   directory the build was configured with and nothing on a box moves it, so a
   case that wants to drive the one name answered out of it has to name a
   directory it made itself. */
struct PublicRoot
{
	explicit PublicRoot(const std::string &dir) { setPublicRootForTest(dir); }
	~PublicRoot() { setPublicRootForTest(PUBLIC_HTTPDDIR); }

private:
	PublicRoot(const PublicRoot &);
	PublicRoot &operator=(const PublicRoot &);
};

const char kUserStyle[] = ":root { --accent: #fd6d13; }\n";

} // namespace

TEST_CASE("the one name outside the docroot is answered out of the writable directory", "[static]")
{
	Docroot tree;
	REQUIRE(::mkdir(kSibling.c_str(), 0755) == 0);
	writeFile(kSibling + "/user.css", kUserStyle);
	PublicRoot second(kSibling);

	/* A file of that name in the docroot as well, so that what answers is
	   decided by the rule and not by which of the two happens to hold one. The
	   page is installed read only and this one is not, which is the whole
	   reason the name is answered from somewhere else. */
	writeFile(kRoot + "/user.css", "/* the installed one */\n");

	setRoutesForTest(NULL);
	REQUIRE(start(loopback()));

	testhttp::Reply r = testhttp::request(boundPort(), "GET", "/user.css");
	REQUIRE(r.transport_ok);
	REQUIRE(r.code == 200);
	REQUIRE(r.body == kUserStyle);
	REQUIRE(r.header("Content-Type") == "text/css");
}

TEST_CASE("no second name reaches the writable directory", "[static]")
{
	/* The list is of one and it is compared whole. Written as a prefix
	   comparison, every one of the three names below would reach a directory a
	   box may write into, which is a way out of the docroot that nothing in the
	   configuration would show. */
	Docroot tree;
	REQUIRE(::mkdir(kSibling.c_str(), 0755) == 0);
	writeFile(kSibling + "/user.cssX", "secret\n");
	writeFile(kSibling + "/other.css", "secret\n");
	REQUIRE(::mkdir((kSibling + "/sub").c_str(), 0755) == 0);
	writeFile(kSibling + "/sub/user.css", "secret\n");
	PublicRoot second(kSibling);

	setRoutesForTest(NULL);
	REQUIRE(start(loopback()));

	const char *const gone[] = { "/user.cssX", "/other.css", "/sub/user.css" };
	for (size_t i = 0; i < sizeof(gone) / sizeof(gone[0]); ++i)
	{
		testhttp::Reply r = testhttp::request(boundPort(), "GET", gone[i]);
		REQUIRE(r.transport_ok);
		REQUIRE(r.code == 404);
		REQUIRE(r.body.find("secret") == std::string::npos);
	}
}

TEST_CASE("the second root is confined exactly as the first one is", "[static]")
{
	// Everything the confinement is written about holds with the named
	// directory in place of the configured one, because the resolution under it
	// already carried the root as a parameter and only where it comes from
	// changed.
	Docroot tree;
	REQUIRE(::mkdir(kSibling.c_str(), 0755) == 0);
	writeFile(kSibling + "/user.css", kUserStyle);
	REQUIRE(::symlink("/etc", (kSibling + "/escape").c_str()) == 0);

	REQUIRE(serveStaticFrom(kSibling, "/user.css", "", "").code == 200);
	REQUIRE(serveStaticFrom(kSibling, "/escape/hostname", "", "").code == 400);
	REQUIRE(serveStaticFrom(kSibling, "/../etc/hostname", "", "").code == 400);
	REQUIRE(serveStaticFrom(kSibling, "/nothing.css", "", "").code == 404);

	// And an empty directory name serves nothing at all, which is what a box
	// that was never configured runs on.
	REQUIRE(serveStaticFrom("", "/user.css", "", "").code == 404);
}

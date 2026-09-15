/*
 * static.cpp - serving files out of the web directory
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

#include "static.h"

/* For the directory the build was configured with, which is where the one name
   answered outside the docroot is read from. */
#include <config.h>

#include "http.h"
#include "status.h"

#include "coreapi/base/errors.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdlib.h>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <zlib.h>

namespace httpd
{

namespace
{

/* What this server reads out of the directory and what it lets a stored form expand
   into. The directory is writable on a box and an answer is built whole in memory,
   so without a ceiling one request for a file somebody planted is the whole of the
   box's memory. The stored form needs the same number for a second reason: a few
   hundred bytes of gzip expand into gigabytes of zeroes. */
const size_t kMaxServedBytes = 16u * 1024u * 1024u;

// What a page is answered with, and what an asset under the hashed directory
// is answered with. The second is only safe because every name under there
// carries the hash of its own content, so a changed file is a changed name.
const char kPageCache[]  = "no-cache";
const char kAssetCache[] = "public, max-age=31536000, immutable";

// The one directory whose names carry a content hash, which is what makes an
// answer from it worth never revalidating.
const char kHashedDir[] = "assets";

std::string &rootRef()
{
	/* Inside a function so this file still needs nothing to have run before main: a
	   string at namespace scope is a constructor the loader runs, and the rest of this
	   directory is written so a translation unit costs nothing until it is called. */
	static std::string root;
	return root;
}

std::string &publicRootRef()
{
	// The directory the build was configured with, held the same way and for
	// the same reason as the one above.
	static std::string root(PUBLIC_HTTPDDIR);
	return root;
}

/* What runs between the resolution and the open, and NULL in a running server.
   Written by a case before anything else is going on and read on the thread that
   answers, the same arrangement the directory above has. */
ResolvedProbe resolved_probe_ = NULL;

bool equalsNoCase(const std::string &a, const char *b)
{
	size_t i = 0;
	for (; i < a.size() && b[i] != '\0'; ++i)
	{
		char x = a[i];
		char y = b[i];
		if (x >= 'A' && x <= 'Z')
			x = (char)(x - 'A' + 'a');
		if (y >= 'A' && y <= 'Z')
			y = (char)(y - 'A' + 'a');
		if (x != y)
			return false;
	}
	return i == a.size() && b[i] == '\0';
}

bool isSpace(char c)
{
	return c == ' ' || c == '\t';
}

std::string trimmed(const std::string &s)
{
	size_t from = 0;
	size_t to = s.size();
	while (from < to && isSpace(s[from]))
		++from;
	while (to > from && isSpace(s[to - 1]))
		--to;
	return s.substr(from, to - from);
}

/* One element of a separated list, and the position the next one starts at. A walk
   rather than a split into a vector because the lists this file reads are a handful
   of elements long and a vector of strings is a heap allocation per header.

   The separator is a parameter because the two levels of one header are separated
   differently, codings by commas and the parameters of one coding by semicolons, and
   the wrong one always stops after one item. */
bool nextElement(const std::string &list, char separator, size_t &at, std::string &out)
{
	if (at > list.size())
		return false;

	const size_t cut = list.find(separator, at);
	if (cut == std::string::npos)
	{
		out = list.substr(at);
		at = list.size() + 1;
	}
	else
	{
		out = list.substr(at, cut - at);
		at = cut + 1;
	}
	return true;
}

/* Whether a weight says the caller will take it. Zero is a refusal and is the whole
   reason this is read at all: a client writing gzip;q=0 has named the coding in order
   to say it cannot use it, and a header searched for the letters reads that as the
   opposite. The grammar allows three decimals and nothing else, and anything outside
   it answers no: guessing the other way sends a body the caller cannot unpack. */
bool weightAboveZero(const std::string &value)
{
	if (value.empty())
		return false;

	size_t i = 0;
	bool above = false;
	if (value[i] == '1')
		above = true;
	else if (value[i] != '0')
		return false;
	++i;

	if (i == value.size())
		return above;

	if (value[i] != '.')
		return false;
	++i;

	size_t decimals = 0;
	for (; i < value.size(); ++i, ++decimals)
	{
		if (decimals == 3)
			return false;
		if (value[i] < '0' || value[i] > '9')
			return false;
		if (value[i] != '0')
			above = true;
	}
	return above;
}

// Whether the weight written beside a coding allows it, and a coding with no
// weight beside it carries a full one.
bool weightAllows(const std::string &params)
{
	size_t at = 0;
	std::string one;
	while (nextElement(params, ';', at, one))
	{
		const std::string p = trimmed(one);
		const size_t eq = p.find('=');
		if (eq == std::string::npos)
			continue;
		if (!equalsNoCase(trimmed(p.substr(0, eq)), "q"))
			continue;
		return weightAboveZero(trimmed(p.substr(eq + 1)));
	}
	return true;
}

/* Whether a name is the stored form of another one. Written once because two
   places ask it and a guard spelled out twice is a guard that drifts. */
bool namesStoredForm(const std::string &name)
{
	return name.size() > 3 && name.compare(name.size() - 3, 3, ".gz") == 0;
}

int hexDigit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/* One escape is two hex digits and nothing else. A truncated or misspelt one is
   refused rather than passed through as the characters it is written with, the two
   readings differing and the request choosing which applies being how one arrives at
   a check written for the other. */
bool percentDecode(const std::string &in, std::string &out)
{
	out.clear();
	out.reserve(in.size());
	for (size_t i = 0; i < in.size(); ++i)
	{
		if (in[i] != '%')
		{
			out += in[i];
			continue;
		}
		if (i + 2 >= in.size())
			return false;
		const int hi = hexDigit(in[i + 1]);
		const int lo = hexDigit(in[i + 2]);
		if (hi < 0 || lo < 0)
			return false;
		out += (char)((hi << 4) | lo);
		i += 2;
	}
	return true;
}

// Why a path was turned down, which decides both the code and what the answer
// says the caller did.
enum Refusal
{
	PathIsFine,
	PathUnreadable,     // an escape this will not read, or a target that is not one
	PathHasZero,
	PathIsRelative,     // a dot segment, which names a place rather than a file
	PathNamesDirectory  // a trailing separator, and no directory is a file
};

/* The names a request asked for, decoded, with every reading this server will not accept
   refused before anything touches the filesystem.

   Decoded whole and then split, and not split and then decoded. An escaped separator is a
   separator here, which is the opposite of what the router does and is right for the
   opposite reason: the router matches a declaration against segments, so a segment that
   became two would reach a route nobody wrote, while this resolves a name and hands the
   result to a check on where it landed. Decoding first puts an escaped dot segment in
   front of the dot segment rule.

   An empty segment in the middle is a doubled separator and is refused: a second spelling
   of the path without it.

   An empty one at the end is the ordinary way of writing a directory, and no directory is
   a file here, so it is answered as absent. Not dropped: dropped, a file asked for with a
   separator after it would be served under a second name, which is a second cache entry
   for one answer and a place a browser resolves a relative reference from as though it
   were a directory. */
Refusal segmentsOf(const std::string &url_path, std::vector<std::string> &out)
{
	out.clear();

	if (url_path.empty() || url_path[0] != '/')
		return PathUnreadable;

	std::string decoded;
	if (!percentDecode(url_path, decoded))
		return PathUnreadable;
	if (decoded.find('\0') != std::string::npos)
		return PathHasZero;
	if (decoded[0] != '/')
		return PathUnreadable;

	std::vector<std::string> raw;
	size_t start = 1;
	for (;;)
	{
		const size_t slash = decoded.find('/', start);
		if (slash == std::string::npos)
		{
			raw.push_back(decoded.substr(start));
			break;
		}
		raw.push_back(decoded.substr(start, slash - start));
		start = slash + 1;
	}

	for (size_t i = 0; i < raw.size(); ++i)
	{
		if (raw[i].empty())
		{
			// The last one and only the last one is the directory spelling.
			// Anywhere else it is a doubled separator.
			if (i + 1 == raw.size())
			{
				// The marker itself carries nothing, so it is dropped before
				// handing the rest back: what is left names the directory a caller
				// asking for its own index is asking inside.
				raw.pop_back();
				out.swap(raw);
				return PathNamesDirectory;
			}
			return PathUnreadable;
		}
		if (raw[i] == "." || raw[i] == "..")
			return PathIsRelative;
	}

	out.swap(raw);
	return PathIsFine;
}

/* Every refusal this file makes, and every one of them says how long it may be kept.

   The 404 is the one that has to: it is heuristically cacheable, so a shared cache
   may invent a freshness lifetime for it, and this 404 is the answer for a page that
   is not installed yet. The refusals that never become an answer by installing a file
   carry it too, one rule for all of them being one rule to keep. */
Response refusalWith(int http_code, coreapi::ErrorCode code, const char *detail)
{
	Response r = problemResponse(http_code, code, detail);
	r.headers.push_back(std::make_pair(std::string("Cache-Control"),
	                                   std::string(kPageCache)));
	/* The type is stated and is meant, and this says so. It matters most here rather
	   than in the API's answers: the one place this server cannot state a true type is a
	   file in this directory whose extension the table does not name, and that goes out
	   as bytes of no stated kind to a caller who needed no credential to ask. The framing
	   refusal comes with it, a refusal built here standing in for a page. */
	addPageHeaders(r);
	return r;
}

Response refuseUnreadable()
{
	return refusalWith(StatusBadRequest, coreapi::ErrorCode::BadPath,
	                   "that target is not a path this server reads");
}

Response refuseZeroByte()
{
	return refusalWith(StatusBadRequest, coreapi::ErrorCode::PathHasZeroByte,
	                   "that target carries a zero byte");
}

Response refuseRelative()
{
	return refusalWith(StatusBadRequest, coreapi::ErrorCode::RelativePath,
	                   "that target names a place rather than a file");
}

/* A name that resolved to somewhere this server does not serve from, which is its own
   answer and deliberately not the one below.

   What reaches this is a link inside the directory pointing out of it, and the two who
   plant such a link are an attacker and whoever installed the pages. The second is far the
   commoner and is the reason the answers are kept apart: told that the target is not under
   what this server serves, that person looks at the link; told that there is no such path,
   that person looks for a file that is right where they put it.

   What it costs is the fact that such a link exists, and that fact is worth nothing to
   either of them. What is not said is what the link points at and what is inside the
   directory.

   The mismatch the identity check answers is the other way about and answers absence,
   because there the caller is racing this server and would learn from the answer whether
   the race landed. */
Response refuseOutside()
{
	return refusalWith(StatusBadRequest, coreapi::ErrorCode::OutsideRoots,
	                   "that target is not under what this server serves");
}

Response refuseMissing()
{
	/* Worded as the router words it, so a path this server does not route and a path
	   this directory does not hold are one answer. A caller that learnt which of the two
	   it had reached would be learning whether a name it guessed is a file this box has,
	   which turns a directory nobody may list into one anybody may walk. */
	return refusalWith(StatusNotFound, coreapi::ErrorCode::NoSuchRoute,
	                   "this server has no such path");
}

Response refuseTooLarge()
{
	return refusalWith(StatusInternalServerError, coreapi::ErrorCode::OutputTooLarge,
	                   "that file is larger than this server sends");
}

Response refuseUnreadableFile()
{
	return refusalWith(StatusInternalServerError, coreapi::ErrorCode::BoxUnreadable,
	                   "that file could not be read");
}

/* Sent for a target that names a real, confined directory without the separator that
   says so. The name a page beneath it is written with is relative, so a browser
   holding this one without the separator reaches for a sibling of the directory
   rather than something inside it.

   Answered off the target as it arrived and not off the resolved name: the caller
   wrote a target and is owed the same one back with the one character added. */
Response redirectToSlash(const std::string &url_path)
{
	Response r;
	r.code = StatusMovedPermanently;
	r.headers.push_back(std::make_pair(std::string("Location"), url_path + "/"));
	r.headers.push_back(std::make_pair(std::string("Cache-Control"), std::string(kPageCache)));
	addPageHeaders(r);
	return r;
}

/* Whether a resolved path is the directory itself or something under it.

   The comparison is on the resolved path and never on the text of the target, because the
   directory is writable on a box and a link planted in it points wherever it likes. And it
   ends at a separator: a prefix comparison alone lets a sibling whose name begins with the
   directory's name through, so a root of /var/www admits /var/wwwsecret.

   The one planted link this cannot catch is a hard one. A hard link inside the directory to
   a file outside it is genuinely a name inside the directory, the file has no one true
   path, and nothing this or any other resolution can ask would say otherwise. Whoever may
   write into that directory can therefore publish any file on the same filesystem that
   they can already read. */
bool insideRoot(const std::string &root, const std::string &path)
{
	if (path == root)
		return true;

	// A root of / is the one whose name is its own separator, and comparing
	// the byte after it would ask for a second one.
	const std::string prefix = (root == "/") ? std::string() : root;
	if (path.size() <= prefix.size())
		return false;
	if (path.compare(0, prefix.size(), prefix) != 0)
		return false;
	return path[prefix.size()] == '/';
}

enum Found
{
	FoundIt,
	NotThere,
	Escaped,
	// Its own answer and not NotThere, because the one caller that asks about the
	// terminal component itself rather than about ".../index.html" underneath it has
	// something to say about a real directory: see redirectToSlash below.
	FoundDirectory
};

/* Resolves one candidate and says where it landed.

   Every other way of failing to resolve answers NotThere: a directory on the way that
   cannot be searched, a loop of links, a name too long for the system, a device or a
   socket somebody put there. None of those is a file this server may send. The
   terminal component being a directory is carried apart from them, in FoundDirectory,
   because unlike the others it is a real, confined place this server knows the true
   name of. A resolution that lands outside answers before what was found is read. */
Found resolveInside(const std::string &root, const std::string &candidate,
                    std::string &resolved, struct stat &st)
{
	if (candidate.find('\0') != std::string::npos)
		return NotThere;

	char *real = ::realpath(candidate.c_str(), NULL);
	if (real == NULL)
		return NotThere;

	std::string found(real);
	::free(real);

	if (!insideRoot(root, found))
		return Escaped;

	if (::stat(found.c_str(), &st) != 0)
		return NotThere;
	if (S_ISDIR(st.st_mode))
		return FoundDirectory;
	// Neither a file nor a directory: a device somebody put there, most
	// likely. Only what can be read as bytes is sent.
	if (!S_ISREG(st.st_mode))
		return NotThere;

	resolved = found;
	return FoundIt;
}

enum ReadResult
{
	ReadOk,
	ReadTooLarge,
	ReadFailed,
	// What was checked is not what is there now. Its own answer because it is
	// the one that must never be served, and because what goes back for it is
	// what goes back for a file that was never there.
	ReadChanged
};

/* A descriptor given back however this leaves. Everything below it allocates, and an
   allocation that cannot be met leaves through a throw, so a close written at the end
   of the function is one that does not always happen. */
struct Fd
{
	int fd;

	explicit Fd(int f) : fd(f) {}
	~Fd()
	{
		if (fd >= 0)
			::close(fd);
	}

private:
	Fd(const Fd &);
	Fd &operator=(const Fd &);
};

/* Opens what resolution checked, and proves it is that and not something that took its
   place.

   The whole confinement rests on this. Resolution asks the filesystem about a name and
   answers whether the file behind it is inside the directory, and the directory is writable
   on a box, so between that question and this open somebody can put anything there: a
   rename of the last component, or of any directory on the way, is enough. Nothing about a
   second open by name can rule that out, and asking fstat whether the descriptor is a
   regular file answers a different question.

   So the descriptor is held to the identity resolution saw. A device and an inode number
   name one file on one filesystem, they do not change under a rename, and a different file
   cannot carry the pair. O_NOFOLLOW is here as well and is not what does the work: it
   refuses only a last component that has become a link.

   The answer for a mismatch is the answer for a file that is not there. Anything that told
   the two apart would tell whoever is racing this whether the swap landed. */
ReadResult openChecked(const std::string &path, const struct stat &checked,
                       int &opened_fd, struct stat &opened)
{
	opened_fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (opened_fd < 0)
	{
		/* Gone, or become a link where a file was checked. Both are the file
		   not being there any more; everything else is a box that cannot read
		   its own pages, which is worth saying out loud. */
		if (errno == ENOENT || errno == ELOOP)
			return ReadChanged;
		return ReadFailed;
	}

	std::memset(&opened, 0, sizeof(opened));
	if (::fstat(opened_fd, &opened) != 0)
		return ReadFailed;

	if (!S_ISREG(opened.st_mode))
		return ReadChanged;
	if (opened.st_dev != checked.st_dev || opened.st_ino != checked.st_ino)
		return ReadChanged;

	return ReadOk;
}

ReadResult readAll(int fd, const struct stat &st, std::string &out)
{
	if (st.st_size < 0 || (unsigned long long) st.st_size > (unsigned long long) kMaxServedBytes)
		return ReadTooLarge;

	out.clear();
	out.reserve((size_t) st.st_size);

	char buf[16384];
	for (;;)
	{
		const ssize_t got = ::read(fd, buf, sizeof(buf));
		if (got < 0)
			return ReadFailed;
		if (got == 0)
			break;
		// A file that grew between the size above and this read is still held
		// to the ceiling, because what is being bounded is the memory this
		// answer takes and not what the file said it would take.
		if (out.size() + (size_t) got > kMaxServedBytes)
			return ReadTooLarge;
		out.append(buf, (size_t) got);
	}
	return ReadOk;
}

/* The bytes a stored form holds. Bounded while it runs and not after: the expanded
   size is written by whoever wrote the file and is not readable from it, so a
   check on the result is a check made after the memory has already been taken. */
ReadResult inflateStored(const std::string &in, std::string &out)
{
	/* The same shape as Fd above and for the reason written out there: the loop below
	   appends to a string and an allocation that cannot be met leaves through a throw.
	   What would be left behind is the window and the tables zlib allocated, tens of
	   kilobytes per request of a box that has a hundred and twenty eight megabytes. */
	struct Stream
	{
		z_stream s;
		bool     started;

		Stream() : started(false) { std::memset(&s, 0, sizeof(s)); }
		~Stream()
		{
			if (started)
				::inflateEnd(&s);
		}

	private:
		Stream(const Stream &);
		Stream &operator=(const Stream &);
	};

	Stream z;
	z_stream &s = z.s;

	// The window size with sixteen added is what tells zlib the stream carries
	// a gzip header rather than a zlib one, which is what a .gz file holds.
	if (inflateInit2(&s, 16 + MAX_WBITS) != Z_OK)
		return ReadFailed;
	z.started = true;

	s.next_in = (Bytef *) const_cast<char *>(in.data());
	s.avail_in = (uInt) in.size();

	out.clear();
	char buf[16384];
	ReadResult answer = ReadFailed;
	for (;;)
	{
		s.next_out = (Bytef *) buf;
		s.avail_out = (uInt) sizeof(buf);

		const int rc = ::inflate(&s, Z_NO_FLUSH);
		if (rc != Z_OK && rc != Z_STREAM_END && rc != Z_BUF_ERROR)
			break;

		const size_t got = sizeof(buf) - (size_t) s.avail_out;
		if (out.size() + got > kMaxServedBytes)
		{
			answer = ReadTooLarge;
			break;
		}
		out.append(buf, got);

		if (rc == Z_STREAM_END)
		{
			/* The end of a member and not necessarily the end of the file. The format is a
			   sequence of them, which is what concatenating two of these files produces, and
			   stopping at the first answers part of a file as though it were the whole. What is
			   left is read as the next member, and a tail that is not one fails there. */
			if (s.avail_in == 0)
			{
				answer = ReadOk;
				break;
			}
			if (::inflateReset(&s) != Z_OK)
				break;
			continue;
		}
		// Nothing came out and nothing is left to go in, which is a stream
		// that stops before it ends. Without this the loop would ask zlib for
		// more of a file that has none.
		if (got == 0 && s.avail_in == 0)
			break;
	}

	if (answer != ReadOk)
		out.clear();
	return answer;
}

/* The bytes of one representation, out of the descriptor that was proven to be
   the checked file. One call for both answers that need them, so that what a
   304 measures and what a 200 sends cannot be built two different ways. */
ReadResult buildBody(int fd, const struct stat &st, bool inflate_it, std::string &out)
{
	std::string raw;
	const ReadResult got = readAll(fd, st, raw);
	if (got != ReadOk)
		return got;

	if (!inflate_it)
	{
		out.swap(raw);
		return ReadOk;
	}
	return inflateStored(raw, out);
}

/* The validator for one representation.

   The size and the time it was last written, and not a hash of the bytes. What needs a
   strong validator is the file that gets revalidated, and the files that get revalidated
   here are the pages: the assets are named after the hash of their own content already and
   are answered as immutable.

   The mark says which representation this is, and it is read off the coding the answer goes
   out under rather than off the path that built it. Two bodies under one URL and one
   validator is a cache handing the stored bytes to a caller that asked for the plain ones
   the moment it does not honour the header that says the answer varies. A mark on the path
   is not enough: a directory holding both forms of one file has the two of them at one size
   and one time often enough, and the copy that preserves the time is the ordinary one. */
std::string validatorFor(const struct stat &st, const char *mark)
{
	char buf[96];
	std::snprintf(buf, sizeof(buf), "\"%llu-%llu%s\"",
	              (unsigned long long) st.st_size,
	              (unsigned long long) st.st_mtime,
	              mark);
	return std::string(buf);
}

/* Whether the caller already holds this representation. An exact match and nothing
   looser: the wrong answer here is a 304 to a caller that needed the bytes, which is
   a page that stays stale until somebody clears a cache by hand. A weak marker is
   dropped because what it marks is how equal two representations have to be. */
bool validatorMatches(const std::string &header, const std::string &tag)
{
	size_t at = 0;
	std::string one;
	while (nextElement(header, ',', at, one))
	{
		std::string candidate = trimmed(one);
		if (candidate == "*")
			return true;
		if (candidate.size() > 2 && (candidate[0] == 'W' || candidate[0] == 'w') &&
		    candidate[1] == '/')
			candidate = candidate.substr(2);
		if (candidate == tag)
			return true;
	}
	return false;
}

void addHeader(Response &r, const char *name, const std::string &value)
{
	r.headers.push_back(std::make_pair(std::string(name), value));
}

} // namespace

void setDocroot(const std::string &dir)
{
	rootRef() = dir;
}

void setResolvedProbeForTest(ResolvedProbe p)
{
	resolved_probe_ = p;
}

std::string docroot()
{
	return rootRef();
}

std::string publicRoot()
{
	return publicRootRef();
}

void setPublicRootForTest(const std::string &dir)
{
	publicRootRef() = dir;
}

bool acceptsGzip(const std::string &accept_encoding)
{
	bool named = false;
	bool named_ok = false;
	bool named_refused = false;
	bool star_ok = false;

	size_t at = 0;
	std::string element;
	while (nextElement(accept_encoding, ',', at, element))
	{
		const size_t semi = element.find(';');
		const std::string token =
			trimmed((semi == std::string::npos) ? element : element.substr(0, semi));
		if (token.empty())
			continue;

		const bool is_gzip = equalsNoCase(token, "gzip");
		const bool is_star = (token == "*");
		if (!is_gzip && !is_star)
			continue;

		const bool takes =
			(semi == std::string::npos) ? true : weightAllows(element.substr(semi + 1));

		if (is_gzip)
		{
			named = true;
			if (takes)
				named_ok = true;
			else
				named_refused = true;
		}
		else if (takes)
		{
			star_ok = true;
		}
	}

	/* A coding named outright settles it and the wildcard answers only for what nothing
	   named, which is what the protocol says of the two. A header naming gzip twice and
	   disagreeing with itself is read as the refusal: it is a caller contradicting itself
	   either way, and of the two readings only one can send a body nobody can open. */
	if (named)
		return named_ok && !named_refused;
	return star_ok;
}

const char *contentTypeFor(const std::string &filename)
{
	std::string name = filename;

	const size_t slash = name.rfind('/');
	if (slash != std::string::npos)
		name = name.substr(slash + 1);

	// The stored form is how the file is kept and not what it holds, so the
	// extension that decides the type is the one underneath it.
	if (namesStoredForm(name))
		name = name.substr(0, name.size() - 3);

	const size_t dot = name.rfind('.');
	// Position zero is a name that begins with a dot rather than a name with an
	// extension, and it has no stem for the extension to belong to.
	if (dot == std::string::npos || dot == 0 || dot + 1 >= name.size())
		return "application/octet-stream";

	const std::string ext = name.substr(dot + 1);

	struct Row
	{
		const char *ext;
		const char *type;
	};
	/* What the frontend ships and what a plugin is likely to drop beside it. No
	   charset travels with any of them: a document says its own encoding and a type
	   here that said otherwise would be the one thing a page cannot overrule. */
	static const Row table[] =
	{
		{ "html",        "text/html" },
		{ "htm",         "text/html" },
		{ "js",          "text/javascript" },
		{ "mjs",         "text/javascript" },
		{ "css",         "text/css" },
		{ "json",        "application/json" },
		{ "map",         "application/json" },
		{ "webmanifest", "application/manifest+json" },
		{ "xml",         "application/xml" },
		{ "txt",         "text/plain" },
		{ "svg",         "image/svg+xml" },
		{ "png",         "image/png" },
		{ "jpg",         "image/jpeg" },
		{ "jpeg",        "image/jpeg" },
		{ "gif",         "image/gif" },
		{ "webp",        "image/webp" },
		{ "ico",         "image/vnd.microsoft.icon" },
		{ "woff2",       "font/woff2" },
		{ "woff",        "font/woff" },
		{ "ttf",         "font/ttf" },
		{ "wasm",        "application/wasm" }
	};

	for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i)
	{
		// Without regard to case, because what a type says is what the bytes
		// are and not how somebody typed the name of the file holding them.
		if (equalsNoCase(ext, table[i].ext))
			return table[i].type;
	}
	return "application/octet-stream";
}

Response serveStatic(const std::string &url_path, const std::string &accept_encoding,
                     const std::string &if_none_match)
{
	return serveStaticFrom(rootRef(), url_path, accept_encoding, if_none_match);
}

Response serveStaticFrom(const std::string &root_dir, const std::string &url_path,
                         const std::string &accept_encoding,
                         const std::string &if_none_match)
{
	const std::string configured = root_dir;
	// A box with no directory configured serves nothing, which is the same
	// answer as a directory holding nothing and is the one that gives away
	// neither.
	if (configured.empty() || configured.find('\0') != std::string::npos)
		return refuseMissing();

	char *real_root = ::realpath(configured.c_str(), NULL);
	if (real_root == NULL)
		return refuseMissing();
	std::string root(real_root);
	::free(real_root);
	// A trailing separator would leave every comparison below asking for a
	// second one.
	while (root.size() > 1 && root[root.size() - 1] == '/')
		root.erase(root.size() - 1);

	std::vector<std::string> seg;
	const Refusal why = segmentsOf(url_path, seg);
	if (why == PathHasZero)
		return refuseZeroByte();
	if (why == PathIsRelative)
		return refuseRelative();
	if (why != PathIsFine && why != PathNamesDirectory)
		return refuseUnreadable();

	/* A target written with the separator asks for whatever this directory answers as
	   its own index, which is the one name added here. Everything from here down reads
	   exactly as it would for a caller that had spelled that name out itself, stored-form
	   negotiation included, because it is that request in every way that matters. */
	const bool directory_shaped = (why == PathNamesDirectory);
	if (directory_shaped)
		seg.push_back("index.html");

	/* A target naming the stored form itself is not answered. The stored form is how
	   this server keeps a file and not a resource of its own, so answering it would be a
	   second name for one file: under the type of what is inside it a caller would be
	   handed compressed bytes labelled as a script, and under a type of its own the rule
	   that the type comes from the extension underneath would hold for one name only. */
	if (!seg.empty() && namesStoredForm(seg[seg.size() - 1]))
		return refuseMissing();

	std::string candidate = (root == "/") ? std::string() : root;
	for (size_t i = 0; i < seg.size(); ++i)
	{
		candidate += "/";
		candidate += seg[i];
	}
	if (candidate.empty())
		candidate = root;

	std::string plain_path;
	struct stat plain_st;
	std::memset(&plain_st, 0, sizeof(plain_st));
	const Found plain = resolveInside(root, candidate, plain_path, plain_st);
	if (plain == Escaped)
		return refuseOutside();

	/* The bare spelling of a real directory, asked for without the appended name above
	   having been added: this is the one request the redirect answers, and asking with
	   the separator already there turns it into the ordinary index lookup instead. */
	if (plain == FoundDirectory && !directory_shaped)
		return redirectToSlash(url_path);

	/* A stored form that leaves the directory is passed over rather than refused, unlike
	   the file itself one line above. The plain name is the request and the stored form
	   is this server's own business that nobody asked for by name, so refusing over one
	   would mean anybody who can write into the directory can take any page off the air
	   by planting a link named after it. */
	std::string stored_path;
	struct stat stored_st;
	std::memset(&stored_st, 0, sizeof(stored_st));
	const bool have_stored =
		resolveInside(root, candidate + ".gz", stored_path, stored_st) == FoundIt;

	if (plain != FoundIt && !have_stored)
		return refuseMissing();

	/* Which of the two the answer is built from. The stored one goes out as it is to a
	   caller that said it takes it. To one that did not, the plain file is read when it
	   is there and the stored one expanded when it is not, a box like this spending real
	   time expanding a bundle it already holds in the other form. */
	const bool send_stored = have_stored && acceptsGzip(accept_encoding);
	const bool inflate_it = !send_stored && (plain != FoundIt);
	const bool from_stored = send_stored || inflate_it;

	const std::string &from = from_stored ? stored_path : plain_path;
	const struct stat &from_st = from_stored ? stored_st : plain_st;

	/* The gap the identity check exists for, made reachable without a race. Nothing
	   installs this in a running server, and the call costs a comparison against a
	   pointer that is never written after the daemon starts. */
	if (resolved_probe_ != NULL)
		resolved_probe_();

	int raw_fd = -1;
	struct stat opened;
	std::memset(&opened, 0, sizeof(opened));
	const ReadResult held = openChecked(from, from_st, raw_fd, opened);
	Fd file(raw_fd);
	if (held == ReadChanged)
		return refuseMissing();
	if (held != ReadOk)
		return refuseUnreadableFile();

	/* Read off the descriptor and not off the resolution, so that the
	   validator describes the bytes this answer is about to carry rather than
	   what the name pointed at a moment earlier. */
	const std::string tag = validatorFor(opened, send_stored ? "-gz" : "");

	/* Read off the decoded names and not off the target as it arrived, so a target
	   spelling the directory with an escape is answered the way the same file is answered
	   when spelled plainly. The immutable answer is the one that has to be right: a page
	   answered as immutable by mistake is one nobody can replace without renaming it. */
	const char *cache =
		(seg.size() >= 2 && seg[0] == kHashedDir) ? kAssetCache : kPageCache;

	if (!if_none_match.empty() && validatorMatches(if_none_match, tag))
	{
		/* No length beside it, although a 304 may state the one the answer would have
		   carried. The transport states its own for what it is handed, which here is nothing,
		   and a response carrying two of them is one a browser refuses outright rather than
		   reads twice: Chrome answers ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_LENGTH and the
		   page does not load at all on the second visit. It also takes the work out, stating
		   the length of a stored form having meant expanding the whole file to learn it. */
		Response r;
		r.code = StatusNotModified;
		// No type and no body: what is being answered is that the caller
		// already holds the representation, and a document describing it
		// again is a body a 304 may not carry.
		addPageHeaders(r);
		addHeader(r, "ETag", tag);
		addHeader(r, "Cache-Control", cache);
		addHeader(r, "Vary", "Accept-Encoding");
		return r;
	}

	std::string bytes;
	const ReadResult built = buildBody(file.fd, opened, inflate_it, bytes);
	if (built == ReadTooLarge)
		return refuseTooLarge();
	if (built == ReadChanged)
		return refuseMissing();
	if (built != ReadOk)
		return refuseUnreadableFile();

	Response r;
	r.code = StatusOk;
	// The name the caller asked for and never the name of the file that was
	// read, because the file that was read may be the stored form and the type
	// of the answer is the type of what comes out of it.
	r.content_type = contentTypeFor(seg.empty() ? std::string() : seg[seg.size() - 1]);
	r.body.swap(bytes);

	addPageHeaders(r);
	addHeader(r, "ETag", tag);
	addHeader(r, "Cache-Control", cache);
	/* Said on every answer this makes, the ones that could not have gone the other way
	   included. Whether a stored form exists is a property of what was installed and not
	   of the request, so a cache that filled its entry while none was there would go on
	   handing the plain bytes to callers that would have taken the stored ones. */
	addHeader(r, "Vary", "Accept-Encoding");
	if (send_stored)
		addHeader(r, "Content-Encoding", "gzip");

	return r;
}

} // namespace httpd

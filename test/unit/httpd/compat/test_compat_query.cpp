/*
 * test_compat_query.cpp - tests for the legacy query parsing
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

// compat/ is not compiled with --disable-legacy-api (src/httpd/Makefile.am).
#include <config.h>

#ifndef DISABLE_LEGACY_API

#include "httpd/compat/query.h"

using httpd::compat::CStringList;

//=============================================================================
// parseParams. Every property below is read straight off
// yhttpd_core/yrequest.cpp:155-178; none of it is a decision this port made.
//=============================================================================

TEST_CASE("a bare parameter arrives under its position", "[compat][query]")
{
	CStringList p;
	httpd::compat::parseParams("on", p);
	REQUIRE(p["1"] == "on");
}

TEST_CASE("the position counter counts the whole list", "[compat][query]")
{
	// standby?off&cec=off, exactly as EPGscan.sh sends it
	CStringList p;
	httpd::compat::parseParams("off&cec=off", p);
	REQUIRE(p["1"]   == "off");
	REQUIRE(p["cec"] == "off");
	REQUIRE(p["3"]   == "cec");
	REQUIRE(p.find("2") == p.end());
}

TEST_CASE("a repeated name is joined with a comma", "[compat][query]")
{
	CStringList p;
	httpd::compat::parseParams("id=1&id=2", p);
	REQUIRE(p["id"] == "1,2");
}

// The branch that decides whether to join or to overwrite asks whether the
// value already stored is empty, not whether the name has been seen before.
// The two calls below differ only in whether the first "a" carried a value,
// and that alone flips the answer for the second one: a name check would
// join both ("," is prepended to a value already there), an emptiness check
// only joins the second, because the first left nothing to join onto.
TEST_CASE("a repeated name overwrites an empty value instead of joining onto it", "[compat][query]")
{
	CStringList empty_first;
	httpd::compat::parseParams("a=&a=x", empty_first);
	REQUIRE(empty_first["a"] == "x");

	CStringList nonempty_first;
	httpd::compat::parseParams("a=1&a=2", nonempty_first);
	REQUIRE(nonempty_first["a"] == "1,2");
}

// zapto?statussectionsd, as pr-auto-timer polls the box's current section;
// setmode?tv, as autoreboot.sh and pr-auto-timer both send it. Neither name
// carries a "=", so both land only under their position, the same as
// standby?on above; these are the other two literal callers the spec names.
TEST_CASE("zapto's target arrives under its position, as pr-auto-timer sends it", "[compat][query]")
{
	CStringList p;
	httpd::compat::parseParams("statussectionsd", p);
	REQUIRE(p["1"] == "statussectionsd");
	REQUIRE(p.find("statussectionsd") == p.end());
}

TEST_CASE("setmode's target arrives under its position, as autoreboot.sh sends it", "[compat][query]")
{
	CStringList p;
	httpd::compat::parseParams("tv", p);
	REQUIRE(p["1"] == "tv");
}

// The positional branch (no "=") decodes the name before storing it, the
// same as the keyed branch does for both name and value. "%2B" has to
// become a literal "+" here, not stay as three characters of percent
// escape a caller would have to decode a second time.
TEST_CASE("a bare parameter is decoded before it is stored under its position", "[compat][query]")
{
	CStringList p;
	httpd::compat::parseParams("%2Bon", p);
	REQUIRE(p["1"] == "+on");
}

// The positional branch also trims the name, the same as the keyed branch
// trims a value. This input carries no percent-encoding, so only the trim
// can be responsible for the result, not the decode above.
TEST_CASE("a bare parameter is trimmed before it is stored under its position", "[compat][query]")
{
	CStringList p;
	httpd::compat::parseParams(" on ", p);
	REQUIRE(p["1"] == "on");
}

// A value is trimmed after being decoded, the same shape as the name above.
// This value carries no percent-encoding either, so only the trim explains
// the result.
TEST_CASE("a value is trimmed after being decoded", "[compat][query]")
{
	CStringList p;
	httpd::compat::parseParams("a= 1 &b=2", p);
	REQUIRE(p["a"] == "1");
}

// A trailing "&" makes the outer split fail on an already-empty remainder,
// and that failing call still runs the rest of the loop body once more: the
// same shape of thing ySplitStringVector documents for its own trailing
// delimiter, here falling out of ySplitStringExact's "not found" branch
// instead. Nothing in this tree sends a trailing "&" on purpose, but nothing
// stops a hand-edited plugin config from having a stray one either.
TEST_CASE("a trailing ampersand leaves a phantom empty entry at the next position", "[compat][query]")
{
	CStringList p;
	httpd::compat::parseParams("on&", p);
	REQUIRE(p.size() == 2);
	REQUIRE(p["1"] == "on");
	REQUIRE(p["2"] == "");
}

// Nothing short-circuits an empty query string: the loop body still runs
// once, the same as it does for the trailing ampersand above, leaving one
// phantom entry with an empty name under position one rather than leaving
// ParamList untouched.
TEST_CASE("an empty query string still leaves a phantom entry at position one", "[compat][query]")
{
	CStringList p;
	httpd::compat::parseParams("", p);
	REQUIRE(p.size() == 1);
	REQUIRE(p["1"] == "");
}

//=============================================================================
// analyzeURL. Source is yrequest.cpp:214-243.
//=============================================================================

TEST_CASE("the split at the question mark happens before decoding", "[compat][query]")
{
	CStringList u, p;
	httpd::compat::analyzeURL("/control/message?popup=a%3Fb", u, p);
	REQUIRE(u["filename"] == "message");
	REQUIRE(p["popup"]    == "a?b");
}

// The case above encodes the "?" inside a query *value*, which sits to the
// right of the real separator either way decoding runs, so it does not by
// itself discriminate a parser that decodes before splitting from one that
// does not: both cut at the same, leftmost "?". An encoded "?" in the *path*
// does discriminate, because decoding it first manufactures a leftmost "?"
// that was never the real separator.
TEST_CASE("an encoded question mark in the path is not mistaken for the query start", "[compat][query]")
{
	CStringList u, p;
	httpd::compat::analyzeURL("/foo%3Fbar?baz=1", u, p);
	REQUIRE(u["filename"] == "foo?bar");
	REQUIRE(p["baz"]      == "1");
}

TEST_CASE("a url ending in a slash falls back to index.html", "[compat][query]")
{
	CStringList u, p;
	httpd::compat::analyzeURL("/control/", u, p);
	REQUIRE(u["path"]         == "/control/");
	REQUIRE(u["filename"]     == "index.html");
	REQUIRE(u["filenamepure"] == "index");
	REQUIRE(u["fileext"]      == "html");
}

// The end-of-path check catches two different shapes: a url ending in "/",
// and a url exactly one character long, whatever that one character is.
// "/control/" above only exercises the first; a one-character url with no
// slash at all is the only thing that reaches the second on its own.
TEST_CASE("a url exactly one character long also falls back to index.html", "[compat][query]")
{
	CStringList u, p;
	httpd::compat::analyzeURL("x", u, p);
	REQUIRE(u["path"]     == "x");
	REQUIRE(u["filename"] == "index.html");
}

// ySplitStringLast finds no "/" at all in a url with none, and the fallback
// states the path as "/" itself rather than trusting what ySplitStringLast
// already wrote there on its own. More than one character long, so the
// one-character fallback above cannot be why this holds instead.
TEST_CASE("a url with no slash at all still gets a root path", "[compat][query]")
{
	CStringList u, p;
	httpd::compat::analyzeURL("foo", u, p);
	REQUIRE(u["path"] == "/");
}

TEST_CASE("a filename splits into its pure name and its extension", "[compat][query]")
{
	CStringList u, p;
	httpd::compat::analyzeURL("/control/message.txt", u, p);
	REQUIRE(u["filenamepure"] == "message");
	REQUIRE(u["fileext"]      == "txt");
}

TEST_CASE("a filename with no extension leaves fileext empty", "[compat][query]")
{
	CStringList u, p;
	httpd::compat::analyzeURL("/control/message", u, p);
	REQUIRE(u["filenamepure"] == "message");
	REQUIRE(u["fileext"]      == "");
}

// ySplitStringLast splits on the last "." rather than the first; a name
// with only one dot cannot tell the two apart.
TEST_CASE("a filename with more than one dot splits at the last one", "[compat][query]")
{
	CStringList u, p;
	httpd::compat::analyzeURL("/control/archive.tar.gz", u, p);
	REQUIRE(u["filenamepure"] == "archive.tar");
	REQUIRE(u["fileext"]      == "gz");
}

// Nothing in this tree reads UrlData["fullurl"] today; the old server's
// readers of it (mod_weblog, mod_sendfile, mod_cache, yconnection) all stay
// behind. Pinned anyway, so the day a reader is added here it inherits an
// already-kept promise rather than a fresh guess: decoded, and with a
// trailing CR or LF removed.
TEST_CASE("fullurl is the decoded, CR/LF-trimmed url", "[compat][query]")
{
	CStringList u, p;
	httpd::compat::analyzeURL("/control/na%2Bme", u, p);
	REQUIRE(u["fullurl"] == "/control/na+me");
}

// Without a "?" there is no query to strip, but the url is still decoded
// before it is handed back under UrlData["url"] (and from there into
// "path"/"filename"); a caller reading either raw would still see the
// percent-encoding.
TEST_CASE("a url with no query is still decoded", "[compat][query]")
{
	CStringList u, p;
	httpd::compat::analyzeURL("/control/na%2Bme", u, p);
	REQUIRE(u["filename"] == "na+me");
}

// analyzeURL clears ParamList itself rather than trusting an empty map from
// the caller; a caller that reuses the same map across two different urls
// depends on the old entries actually going away.
TEST_CASE("analyzeURL clears ParamList before filling it, even if the caller reused it", "[compat][query]")
{
	CStringList u, p;
	p["stale"] = "leftover from an earlier url";
	httpd::compat::analyzeURL("/control/b?x=2", u, p);
	REQUIRE(p.find("stale") == p.end());
	REQUIRE(p["x"] == "2");
}

//=============================================================================
// isPositionKey. ConfigCGI's submit path (controlapi.cpp) uses this to leave
// parseParams' own "<position>" -> "<name>" bookkeeping entries out of what it
// writes to a config file; without it, config=neutrino&action=submit adds the
// phantom lines "2=config" and "4=action" to neutrino.conf on every save.
//=============================================================================

TEST_CASE("a position key from a real submit is recognised", "[compat][query]")
{
	// config=neutrino&action=submit, exactly as the old settings page sends
	// it: parseParams files "config" at position 2 and "action" at position 4.
	CStringList p;
	httpd::compat::parseParams("config=neutrino&action=submit", p);
	REQUIRE(p["2"] == "config");
	REQUIRE(p["4"] == "action");
	REQUIRE(httpd::compat::isPositionKey("2"));
	REQUIRE(httpd::compat::isPositionKey("4"));
}

TEST_CASE("a submitted field name is never taken for a position key", "[compat][query]")
{
	REQUIRE_FALSE(httpd::compat::isPositionKey("config"));
	REQUIRE_FALSE(httpd::compat::isPositionKey("action"));
	REQUIRE_FALSE(httpd::compat::isPositionKey("hdd_standby"));
}

TEST_CASE("isPositionKey rejects what is not made only of digits", "[compat][query]")
{
	REQUIRE_FALSE(httpd::compat::isPositionKey(""));
	REQUIRE_FALSE(httpd::compat::isPositionKey("-1"));
	REQUIRE_FALSE(httpd::compat::isPositionKey("1a"));
	REQUIRE_FALSE(httpd::compat::isPositionKey("a1"));
	REQUIRE(httpd::compat::isPositionKey("0"));
	REQUIRE(httpd::compat::isPositionKey("42"));
}

#endif // DISABLE_LEGACY_API

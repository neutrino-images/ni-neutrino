/*
 * test_compat_hook.cpp - tests for the legacy hook
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

#include "httpd/compat/hook.h"

using httpd::compat::CyhookHandler;
using httpd::compat::Serializer;
using httpd::compat::HANDLED_NONE;
using httpd::compat::HANDLED_READY;
using httpd::compat::HANDLED_REDIRECTION;
using httpd::compat::HANDLED_REWRITE;
using httpd::compat::HANDLED_SENDFILE;
using httpd::compat::HTTP_MOVED_TEMPORARILY;
using httpd::compat::HTTP_NIL;
using httpd::compat::HTTP_NOT_FOUND;
using httpd::compat::HTTP_OK;
using httpd::compat::M_GET;
using httpd::compat::M_HEAD;
using httpd::compat::M_UNKNOWN;

//=============================================================================
// Construction. What controlapi.cpp reads before it writes anything of its
// own; a field left at whatever the platform happened to leave on the stack
// is not "handled none" or "method unknown", it is whichever bit pattern was
// already there.
//=============================================================================

TEST_CASE("a fresh handler starts unhandled, of unknown method, with no status stated", "[compat][hook]")
{
	CyhookHandler hh;
	REQUIRE(hh.status == HANDLED_NONE);
	REQUIRE(hh.httpStatus == HTTP_NIL);
	REQUIRE(hh.Method == M_UNKNOWN);
	REQUIRE(hh.ContentLength == 0);
	// -1, not 0: hook.h's own comment beside the field spells out what that value means,
	// the same sentinel yhook.cpp's session_init() resets it to on every request. 0 is a
	// real modification time, one that formats to 00:00:00 GMT, 1 Jan 1970 rather than
	// "no timestamp", and this case used to hold that wrong value as the fresh-handler
	// default.
	REQUIRE(hh.LastModified == (time_t) -1);
	REQUIRE(hh.keep_alive == false);
	REQUIRE(hh.cached == false);
	REQUIRE(hh.ResponseMimeType == "");
	REQUIRE(hh.NewURL == "");
	REQUIRE(hh.Sendfile == "");
	// inherited from Serializer; a derived-class redeclaration of any of
	// these would compile and would answer with a second, disconnected copy
	REQUIRE(hh.yresult == "");
	REQUIRE(hh.ParamList.empty());
}

// A redeclaration of ParamList in this class would compile without a word from the
// compiler and would answer every read and write through the derived class's own,
// disconnected copy, while getOutType() would keep reading the base class's copy, which
// nothing here would ever fill. Comparing the address through each name catches that
// directly.
TEST_CASE("ParamList is the same object seen through the base class and through this one", "[compat][hook]")
{
	CyhookHandler hh;
	Serializer &base = hh;
	REQUIRE(&hh.ParamList == &base.ParamList);
}

//=============================================================================
// The redirection family, all three of which controlapi.cpp calls by name.
//=============================================================================

TEST_CASE("SendFile names the file and asks for it to be sent", "[compat][hook]")
{
	CyhookHandler hh;
	hh.SendFile("/tmp/x.bin");
	REQUIRE(hh.NewURL == "/tmp/x.bin");
	REQUIRE(hh.status == HANDLED_SENDFILE);
}

// SendFile only names the file and the handling status; the http status
// code for a file being sent is decided elsewhere (by whatever called
// SetHeader before reaching this point), and this method leaves it exactly
// where it found it.
TEST_CASE("SendFile leaves httpStatus untouched", "[compat][hook]")
{
	CyhookHandler hh;
	hh.httpStatus = HTTP_NOT_FOUND;
	hh.SendFile("/tmp/x.bin");
	REQUIRE(hh.httpStatus == HTTP_NOT_FOUND);
}

TEST_CASE("SendRedirect answers moved-temporarily and names the new location", "[compat][hook]")
{
	CyhookHandler hh;
	hh.SendRedirect("/Y_Timer_List.yhtm");
	REQUIRE(hh.httpStatus == HTTP_MOVED_TEMPORARILY);
	REQUIRE(hh.NewURL == "/Y_Timer_List.yhtm");
	REQUIRE(hh.status == HANDLED_REDIRECTION);
}

TEST_CASE("SendRewrite names the new location without touching the status code", "[compat][hook]")
{
	CyhookHandler hh;
	hh.httpStatus = HTTP_OK;
	hh.SendRewrite("/control/message");
	REQUIRE(hh.NewURL == "/control/message");
	REQUIRE(hh.status == HANDLED_REWRITE);
	REQUIRE(hh.httpStatus == HTTP_OK);
}

//=============================================================================
// Header and error state.
//=============================================================================

TEST_CASE("SetHeader's two-argument form leaves the handling status alone", "[compat][hook]")
{
	CyhookHandler hh;
	hh.status = HANDLED_READY;
	hh.SetHeader(HTTP_OK, "text/xml; charset=UTF-8");
	REQUIRE(hh.httpStatus == HTTP_OK);
	REQUIRE(hh.ResponseMimeType == "text/xml; charset=UTF-8");
	REQUIRE(hh.status == HANDLED_READY);
}

TEST_CASE("SetHeader's three-argument form also sets the handling status", "[compat][hook]")
{
	CyhookHandler hh;
	hh.SetHeader(HTTP_NOT_FOUND, "text/plain", HANDLED_READY);
	REQUIRE(hh.httpStatus == HTTP_NOT_FOUND);
	REQUIRE(hh.ResponseMimeType == "text/plain");
	REQUIRE(hh.status == HANDLED_READY);
}

TEST_CASE("SetError answers text/html and leaves the handling status where it was", "[compat][hook]")
{
	CyhookHandler hh;
	hh.status = HANDLED_READY;
	hh.SetError(HTTP_NOT_FOUND);
	REQUIRE(hh.httpStatus == HTTP_NOT_FOUND);
	REQUIRE(hh.ResponseMimeType == "text/html");
	REQUIRE(hh.status == HANDLED_READY);
}

TEST_CASE("SetError's two-argument form also sets the handling status", "[compat][hook]")
{
	// controlapi.cpp:342, an unknown endpoint name
	CyhookHandler hh;
	hh.SetError(httpd::compat::HTTP_NOT_IMPLEMENTED, httpd::compat::HANDLED_NOT_IMPLEMENTED);
	REQUIRE(hh.httpStatus == httpd::compat::HTTP_NOT_IMPLEMENTED);
	REQUIRE(hh.status == httpd::compat::HANDLED_NOT_IMPLEMENTED);
}

// SetError always answers text/html, even over a mime type this handler had
// already set for something else: an error page replaces the answer that
// was being built, not just the status code attached to it.
TEST_CASE("SetError resets an already-set mime type back to text/html", "[compat][hook]")
{
	CyhookHandler hh;
	hh.SetHeader(HTTP_OK, "text/xml; charset=UTF-8");
	hh.SetError(HTTP_NOT_FOUND);
	REQUIRE(hh.ResponseMimeType == "text/html");
}

//=============================================================================
// GetContentLength answers one of two different things depending on status,
// and only one of them is yresult.
//=============================================================================

TEST_CASE("GetContentLength answers the result length outside a sendfile", "[compat][hook]")
{
	CyhookHandler hh;
	hh.addResult("12345");
	REQUIRE(hh.GetContentLength() == 5);
}

TEST_CASE("GetContentLength answers ContentLength during a sendfile", "[compat][hook]")
{
	CyhookHandler hh;
	hh.addResult("12345");
	hh.SendFile("/tmp/x.bin");
	hh.ContentLength = 999;
	REQUIRE(hh.GetContentLength() == 999);
}

//=============================================================================
// The two-argument addResult hides the one-argument form Serializer declares
// unless "using Serializer::addResult;" brings it back; both are exercised so
// a regression in either compiles into a case that fails instead of one that
// silently resolves to the wrong overload.
//=============================================================================

TEST_CASE("addResult's one-argument form still reaches Serializer's", "[compat][hook]")
{
	CyhookHandler hh;
	hh.addResult("a");
	REQUIRE(hh.yresult == "a");
	REQUIRE(hh.status == HANDLED_NONE);
}

TEST_CASE("addResult's two-argument form also sets the handling status", "[compat][hook]")
{
	CyhookHandler hh;
	hh.addResult("a", HANDLED_READY);
	REQUIRE(hh.yresult == "a");
	REQUIRE(hh.status == HANDLED_READY);
}

//=============================================================================
// ParamList_exist walks positions "1".."size()", the same counting
// parseParams writes them under, not the map's own keys.
//=============================================================================

TEST_CASE("ParamList_exist finds a value at any position", "[compat][hook]")
{
	CyhookHandler hh;
	hh.ParamList["1"] = "tv";
	REQUIRE(hh.ParamList_exist("tv"));
	REQUIRE_FALSE(hh.ParamList_exist("radio"));
}

TEST_CASE("ParamList_exist does not look at a keyed entry, only at positions", "[compat][hook]")
{
	CyhookHandler hh;
	// "format" is a key, not a position; a value stored only there must not
	// answer as present the way a positional one would
	hh.ParamList["format"] = "json";
	REQUIRE_FALSE(hh.ParamList_exist("json"));
}

// parseParams writes the first positional entry under "1", never under "0". A loop
// starting at "0" instead would still find a target stored at "1" once it got there, so
// that alone would not turn red; what it leaves behind does. ParamList[key] inserts an
// empty entry for a key that was not already there, so a loop that visits "0" leaves one
// behind, and this checks for exactly that.
TEST_CASE("ParamList_exist starts counting positions at one, not zero", "[compat][hook]")
{
	CyhookHandler hh;
	hh.ParamList["1"] = "tv";
	REQUIRE_FALSE(hh.ParamList_exist("nonexistent"));
	REQUIRE(hh.ParamList.find("0") == hh.ParamList.end());
}

//=============================================================================
// outStart states the response head through setOutHeader, which this class overrides to
// fold the head into httpStatus/ResponseMimeType rather than the fields Serializer falls
// back to. Removing "virtual void setOutHeader(...)" from hook.h compiles clean, and
// outStart still runs end to end; it just never touches httpStatus or ResponseMimeType,
// and every answer goes out with the handler's fields at whatever they last held. That
// is the one trap here with no compiler diagnostic on either side of it.
//=============================================================================

TEST_CASE("outStart states its head through the handler's own fields, not the fallback ones", "[compat][hook]")
{
	CyhookHandler hh;
	REQUIRE(hh.httpStatus == HTTP_NIL);
	REQUIRE(hh.ResponseMimeType == "");
	REQUIRE(hh.outStart() == httpd::compat::plain);
	REQUIRE(hh.httpStatus == HTTP_OK);
	REQUIRE(hh.ResponseMimeType == "text/plain; charset=UTF-8");
}

TEST_CASE("outStart's xml head also lands on the handler's own fields", "[compat][hook]")
{
	CyhookHandler hh;
	hh.ParamList["format"] = "xml";
	REQUIRE(hh.outStart() == httpd::compat::xml);
	REQUIRE(hh.httpStatus == HTTP_OK);
	REQUIRE(hh.ResponseMimeType == "text/xml; charset=UTF-8");
}

#endif // DISABLE_LEGACY_API

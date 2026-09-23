/*
 * test_compat_serializer.cpp - tests for the legacy serialiser
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

#include "httpd/compat/hookout.h"
#include "httpd/compat/helper.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <unistd.h>

using httpd::compat::Serializer;
using httpd::compat::TOutType;
using httpd::compat::plain;
using httpd::compat::html;
using httpd::compat::xml;
using httpd::compat::json;

namespace
{

/* The query is split here rather than by the parser that belongs to the layer
   above, because that parser is not written yet and because a case that fed
   the serialiser through it would be comparing two things at once. The four
   queries below are the ones real consumers send. */
TOutType outTypeFor(const std::string &query)
{
	Serializer s;
	std::string rest = query;
	while (!rest.empty())
	{
		const std::string::size_type amp = rest.find('&');
		const std::string one = rest.substr(0, amp);
		rest = (amp == std::string::npos) ? "" : rest.substr(amp + 1);

		const std::string::size_type eq = one.find('=');
		if (eq == std::string::npos)
			s.ParamList[one] = "";
		else
			s.ParamList[one.substr(0, eq)] = one.substr(eq + 1);
	}
	return s.getOutType();
}

std::string readBack(const std::string &path)
{
	std::string out;
	FILE *f = std::fopen(path.c_str(), "r");
	if (f == NULL)
		return out;
	int c;
	while ((c = std::fgetc(f)) != EOF)
		out += (char) c;
	std::fclose(f);
	return out;
}

} // namespace

//=============================================================================
// The response head outStart states
//=============================================================================

TEST_CASE("outStart answers plain and names text/plain", "[compat][serializer]")
{
	Serializer s;
	// nothing is stated before outStart runs, so what it states is its own
	REQUIRE(s.outHttpStatus == -1);
	REQUIRE(s.outMimeType == "");
	REQUIRE(s.outStart() == plain);
	REQUIRE(s.outMimeType == "text/plain; charset=UTF-8");
	REQUIRE(s.outHttpStatus == 200);
	// what the argument defaults to, which only a call without one says
	REQUIRE(s.nonPair == false);
}

TEST_CASE("outStart names text/plain for json as well", "[compat][serializer]")
{
	// not an oversight of this copy: the old server answers JSON as text/plain
	Serializer s;
	REQUIRE(s.outHttpStatus == -1);
	REQUIRE(s.outMimeType == "");
	s.ParamList["format"] = "json";
	REQUIRE(s.outStart() == json);
	REQUIRE(s.outMimeType == "text/plain; charset=UTF-8");
	REQUIRE(s.outHttpStatus == 200);
}

TEST_CASE("outStart names text/xml for xml", "[compat][serializer]")
{
	// the status is the same in both branches; only the type follows the kind
	Serializer s;
	REQUIRE(s.outHttpStatus == -1);
	REQUIRE(s.outMimeType == "");
	s.ParamList["format"] = "xml";
	REQUIRE(s.outStart() == xml);
	REQUIRE(s.outMimeType == "text/xml; charset=UTF-8");
	REQUIRE(s.outHttpStatus == 200);
}

TEST_CASE("outStart never answers html", "[compat][serializer]")
{
	// html is reachable only by naming it; no query produces it
	Serializer s;
	REQUIRE(s.outHttpStatus == -1);
	REQUIRE(s.outMimeType == "");
	s.ParamList["format"] = "html";
	REQUIRE(s.outStart() == plain);
	REQUIRE(s.outMimeType == "text/plain; charset=UTF-8");
	REQUIRE(s.outHttpStatus == 200);
}

TEST_CASE("outStart carries its argument into nonPair", "[compat][serializer]")
{
	Serializer s;
	REQUIRE(s.outStart(true) == plain);
	REQUIRE(s.nonPair == true);
	REQUIRE(s.outPair("bouquet", "1", false) == "1\n");
}

//=============================================================================
// The format switch, against the queries real consumers send
//=============================================================================

TEST_CASE("the second form of the format switch is honoured", "[compat][serializer]")
{
	// pr-auto-timer asks with xml=true, not with format=xml
	REQUIRE(outTypeFor("xml=true&details=true&channelid=1") == xml);
	REQUIRE(outTypeFor("format=json&channelid=1")           == json);
	REQUIRE(outTypeFor("channelid=1")                       == plain);
	// an empty value is not the switch
	REQUIRE(outTypeFor("xml=")                              == plain);
}

TEST_CASE("the second form works for json too", "[compat][serializer]")
{
	REQUIRE(outTypeFor("json=true")   == json);
	REQUIRE(outTypeFor("json=1")      == json);
	REQUIRE(outTypeFor("json=")       == plain);
	REQUIRE(outTypeFor("format=xml")  == xml);
	REQUIRE(outTypeFor("format=html") == plain);
	REQUIRE(outTypeFor("")            == plain);
}

TEST_CASE("json is read before xml when a query names both", "[compat][serializer]")
{
	REQUIRE(outTypeFor("xml=true&json=true") == json);
}

TEST_CASE("the format switch compares the whole name", "[compat][serializer]")
{
	// a value that begins with one of the two names is not that name, and a
	// query the switch does not recognise is answered as plain
	REQUIRE(outTypeFor("format=jsonp")  == plain);
	REQUIRE(outTypeFor("format=xmlrpc") == plain);
	REQUIRE(outTypeFor("format=js")     == plain);
	REQUIRE(outTypeFor("format=JSON")   == plain);
	REQUIRE(outTypeFor("format=json")   == json);
	REQUIRE(outTypeFor("format=xml")    == xml);
}

//=============================================================================
// outIndent, which is empty and stays empty
//=============================================================================

TEST_CASE("outIndent is empty in plain", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(plain);
	REQUIRE(s.outIndent() == "");
}

TEST_CASE("outIndent is empty in html", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(html);
	REQUIRE(s.outIndent() == "");
}

TEST_CASE("outIndent is empty in xml", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(xml);
	REQUIRE(s.outIndent() == "");
}

TEST_CASE("outIndent is empty in json", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(json);
	REQUIRE(s.outIndent() == "");
}


TEST_CASE("outSingle ends the content with a newline in plain", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(plain);
	REQUIRE(s.outSingle("ok") == "ok\n");
}

TEST_CASE("outSingle ends the content with a newline in html", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(html);
	REQUIRE(s.outSingle("ok") == "ok\n");
}

TEST_CASE("outSingle ends the content with a newline in xml", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(xml);
	REQUIRE(s.outSingle("<a/>") == "<a/>\n");
}

TEST_CASE("outSingle ends the content with a newline in json", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(json);
	REQUIRE(s.outSingle("{}") == "{}\n");
}


TEST_CASE("outPair writes key=value in plain", "[compat][serializer]")
{
	httpd::compat::Serializer s;
	s.setOutType(httpd::compat::plain);
	REQUIRE(s.outPair("bouquet", "1", false) == "bouquet=1\n");
}

TEST_CASE("outPair writes key=value in html", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(html);
	REQUIRE(s.outPair("bouquet", "1", false) == "bouquet=1\n");
}

TEST_CASE("outPair writes the value alone when nonPair is set", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(plain);
	s.nonPair = true;
	REQUIRE(s.outPair("bouquet", "1", false) == "1\n");
}

TEST_CASE("outPair turns a quote into an apostrophe in json", "[compat][serializer]")
{
	httpd::compat::Serializer s;
	s.setOutType(httpd::compat::json);
	REQUIRE(s.outPair("title", "a \"b\" c", true) == "\"title\": \"a 'b' c\",\n");
}

TEST_CASE("outPair leaves the comma off the last pair in json", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(json);
	REQUIRE(s.outPair("title", "Eins", false) == "\"title\": \"Eins\"\n");
}

TEST_CASE("outPair closes on the first word of the key in xml", "[compat][serializer]")
{
	httpd::compat::Serializer s;
	s.setOutType(httpd::compat::xml);
	REQUIRE(s.outPair("channel id=\"7\"", "Eins", false)
	        == "<channel id=\"7\">Eins</channel>\n");
}

TEST_CASE("outPair ignores the comma in xml", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(xml);
	REQUIRE(s.outPair("title", "Eins", true) == "<title>Eins</title>\n");
}


TEST_CASE("outArray hands the content back unchanged in plain", "[compat][serializer]")
{
	// no newline of its own here, unlike the two structured branches
	Serializer s;
	s.setOutType(plain);
	REQUIRE(s.outArray("bouquets", "a\nb\n", false) == "a\nb\n");
}

TEST_CASE("outArray hands the content back unchanged in html", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(html);
	REQUIRE(s.outArray("bouquets", "a\nb\n", false) == "a\nb\n");
}

TEST_CASE("outArray brackets the content in xml", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(xml);
	REQUIRE(s.outArray("bouquets num=\"2\"", "a\n", false)
	        == "<bouquets num=\"2\">\na\n</bouquets>\n");
}

TEST_CASE("outArray brackets the content in json", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(json);
	REQUIRE(s.outArray("bouquets", "a,b", true) == "\"bouquets\": [a,b],\n");
	Serializer t;
	t.setOutType(json);
	REQUIRE(t.outArray("bouquets", "a,b", false) == "\"bouquets\": [a,b]\n");
}

TEST_CASE("outArray and outObject leave the comma off when nobody says next",
          "[compat][serializer]")
{
	// the two argument form, which is the one the old server calls in most
	// places. A default is only ever stated by a call that leaves it out
	Serializer s;
	s.setOutType(json);
	REQUIRE(s.outArray("bouquets", "a,b") == "\"bouquets\": [a,b]\n");
	REQUIRE(s.outObject("epg", "\"a\": \"1\"") == "\"epg\": {\"a\": \"1\"}\n");
	s.setOutType(xml);
	REQUIRE(s.outArray("bouquets", "a\n") == "<bouquets>\na\n</bouquets>\n");
	REQUIRE(s.outObject("epg", "x\n") == "<epg>\nx\n</epg>\n");
}


TEST_CASE("outArrayItem hands the content back unchanged in plain", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(plain);
	REQUIRE(s.outArrayItem("bouquet", "x=1\n", false) == "x=1\n");
}

TEST_CASE("outArrayItem hands the content back unchanged in html", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(html);
	REQUIRE(s.outArrayItem("bouquet", "x=1\n", false) == "x=1\n");
}

TEST_CASE("outArrayItem brackets the content in xml", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(xml);
	REQUIRE(s.outArrayItem("bouquet id=\"3\"", "x\n", false)
	        == "<bouquet id=\"3\">\nx\n</bouquet>\n");
}

TEST_CASE("outArrayItem writes a nameless object in json", "[compat][serializer]")
{
	// the key names the closing tag in xml and nothing at all in json
	Serializer s;
	s.setOutType(json);
	REQUIRE(s.outArrayItem("bouquet", "\"a\": \"1\"", true) == "{\"a\": \"1\"},\n");
	Serializer t;
	t.setOutType(json);
	REQUIRE(t.outArrayItem("bouquet", "\"a\": \"1\"", false) == "{\"a\": \"1\"}\n");
}


TEST_CASE("outObject hands the content back unchanged in plain", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(plain);
	REQUIRE(s.outObject("epg", "x=1\n", false) == "x=1\n");
}

TEST_CASE("outObject hands the content back unchanged in html", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(html);
	REQUIRE(s.outObject("epg", "x=1\n", false) == "x=1\n");
}

TEST_CASE("outObject brackets the content in xml", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(xml);
	REQUIRE(s.outObject("epg id=\"9\"", "x\n", false) == "<epg id=\"9\">\nx\n</epg>\n");
}

TEST_CASE("outObject brackets the content in json", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(json);
	REQUIRE(s.outObject("epg", "\"a\": \"1\"", true) == "\"epg\": {\"a\": \"1\"},\n");
	Serializer t;
	t.setOutType(json);
	REQUIRE(t.outObject("epg", "\"a\": \"1\"", false) == "\"epg\": {\"a\": \"1\"}\n");
}


TEST_CASE("outValue hands the content back unchanged in plain", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(plain);
	REQUIRE(s.outValue("a<b&c") == "a<b&c");
}

TEST_CASE("outValue hands the content back unchanged in html", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(html);
	REQUIRE(s.outValue("a<b&c") == "a<b&c");
}

TEST_CASE("outValue wraps the content in CDATA in xml", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(xml);
	REQUIRE(s.outValue("a<b&c") == "<![CDATA[a<b&c]]>");
	REQUIRE(s.outValue("a<b&c", true) == "<![CDATA[a<b&c]]>");
}

TEST_CASE("outValue escapes the content in xml when CDATA is declined", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(xml);
	REQUIRE(s.outValue("a<b>c&d\"e'f", false) == "a&lt;b&gt;c&amp;d&quot;e&apos;f");
}

TEST_CASE("outValue reads a byte that is not UTF-8 as latin 1 in xml", "[compat][serializer]")
{
	// a lone 0xe4 is not valid UTF-8 and becomes the two bytes of U+00E4
	Serializer s;
	s.setOutType(xml);
	REQUIRE(s.outValue("\xe4", false) == "\xc3\xa4");
	// already valid UTF-8 is left alone rather than read as latin 1 twice
	REQUIRE(s.outValue("\xc3\xa4", false) == "\xc3\xa4");
}

TEST_CASE("outValue escapes the content for json", "[compat][serializer]")
{
	// every byte the writer names a short escape for, not only the three that
	// turn up often. The two rare ones are control characters as well, so a
	// branch of theirs that went missing would be answered by the numeric form
	// below rather than by nothing, and only their own bytes say which came out
	Serializer s;
	s.setOutType(json);
	REQUIRE(s.outValue("a\"b\\c") == "a\\\"b\\\\c");
	REQUIRE(s.outValue("a\nb") == "a\\nb");
	REQUIRE(s.outValue("a\tb") == "a\\tb");
	REQUIRE(s.outValue("a\rb") == "a\\rb");
	REQUIRE(s.outValue("a\bb") == "a\\bb");
	REQUIRE(s.outValue("a\fb") == "a\\fb");
	REQUIRE(s.outValue("a\nb\tc\rd") == "a\\nb\\tc\\rd");
	REQUIRE(s.outValue("a\x01" "b") == "a\\u0001b");
}

TEST_CASE("outValue hands a byte above ASCII back as it is in json", "[compat][serializer]")
{
	// the road every umlaut in a channel name takes. The test the writer asks
	// is written against char, so 0xe4 reaches it as -28 where char is signed
	// and as 228 where it is not, and both answer no: the first because it is
	// negative, the second because it is above the range. Read off both ways
	// on x86 and on armv7l rather than reasoned about, because which of the
	// two the box has is the platform's to say
	Serializer s;
	s.setOutType(json);
	REQUIRE(s.outValue("\x7f") == "\x7f");
	REQUIRE(s.outValue("\x80") == "\x80");
	REQUIRE(s.outValue("\xe4") == "\xe4");
	REQUIRE(s.outValue("\xff") == "\xff");
	// one byte in, one byte out; the numeric form would make it six
	REQUIRE(s.outValue("\xe4").size() == 1u);
	REQUIRE(s.outValue("\xc3\xa4") == "\xc3\xa4");
	// a name as it reaches a channel list
	REQUIRE(s.outValue("Das Erste \xc3\xa4\xc3\xb6\xc3\xbc")
	        == "Das Erste \xc3\xa4\xc3\xb6\xc3\xbc");
}

TEST_CASE("outValue writes the numeric escape in upper case in json", "[compat][serializer]")
{
	// what the old server puts on the wire, digits and all. The one case above
	// cannot see this: nothing in 0x01 is a letter
	Serializer s;
	s.setOutType(json);
	REQUIRE(s.outValue("\x0b") == "\\u000B");
	REQUIRE(s.outValue("\x1a") == "\\u001A");
	REQUIRE(s.outValue("\x1f") == "\\u001F");
}


TEST_CASE("outNext is empty in plain", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(plain);
	REQUIRE(s.outNext() == "");
}

TEST_CASE("outNext is empty in html", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(html);
	REQUIRE(s.outNext() == "");
}

TEST_CASE("outNext is empty in xml", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(xml);
	REQUIRE(s.outNext() == "");
}

TEST_CASE("outNext is a comma in json", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(json);
	REQUIRE(s.outNext() == ",");
}

//=============================================================================
// SendOk, SendError, SendResult
//=============================================================================

TEST_CASE("SendOk answers three different ways", "[compat][serializer]")
{
	httpd::compat::Serializer s;
	s.setOutType(httpd::compat::plain);  s.SendOk();
	REQUIRE(s.result() == "ok");
	s.reset(); s.setOutType(httpd::compat::xml);  s.SendOk();
	REQUIRE(s.result() == "<success>true</success>");
	s.reset(); s.setOutType(httpd::compat::json); s.SendOk();
	REQUIRE(s.result() == "{\"success\": \"true\"}");
}

TEST_CASE("SendOk answers ok in html", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(html);
	s.SendOk();
	REQUIRE(s.result() == "ok");
}

TEST_CASE("SendError without a text answers four ways", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(plain); s.SendError();
	REQUIRE(s.result() == "error");
	s.reset(); s.setOutType(html); s.SendError();
	REQUIRE(s.result() == "error");
	s.reset(); s.setOutType(xml); s.SendError();
	REQUIRE(s.result() == "<success>false</success>");
	s.reset(); s.setOutType(json); s.SendError();
	REQUIRE(s.result() == "{\"success\": \"false\"}");
}

TEST_CASE("SendError with a text answers four ways", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(plain); s.SendError("no channel");
	REQUIRE(s.result() == "error=no channel");
	s.reset(); s.setOutType(html); s.SendError("no channel");
	REQUIRE(s.result() == "error=no channel");
	s.reset(); s.setOutType(xml); s.SendError("no channel");
	REQUIRE(s.result() == "<success>false<error>no channel</error></success>");
	s.reset(); s.setOutType(json); s.SendError("no channel");
	REQUIRE(s.result() == "{\"success\": \"false\", \"error\":{\"msg\": \"no channel\"}}");
}

TEST_CASE("SendResult ends its line with CRLF in all four", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(plain); s.SendResult("x=1");
	REQUIRE(s.result() == "x=1\r\n");
	s.reset(); s.setOutType(html); s.SendResult("x=1");
	REQUIRE(s.result() == "x=1\r\n");
	s.reset(); s.setOutType(xml); s.SendResult("<x>1</x>");
	REQUIRE(s.result() == "<x>1</x>\r\n");
	s.reset(); s.setOutType(json); s.SendResult("\"x\": \"1\"");
	REQUIRE(s.result() == "{\"success\": \"true\", \"data\":{\"x\": \"1\"}}\r\n");
}

//=============================================================================
// What writes into the result
//=============================================================================

TEST_CASE("WriteLn ends a line the way the old server does", "[compat][serializer]")
{
	httpd::compat::Serializer s;
	s.WriteLn("x");
	REQUIRE(s.result() == "x\r\n");
}

TEST_CASE("WriteLn ends a line with CRLF whatever the output type", "[compat][serializer]")
{
	// the line ending does not follow the output type, and the pairs the same
	// answer carries end in a bare newline, so one answer mixes both
	Serializer s;
	s.setOutType(plain); s.WriteLn("x");
	REQUIRE(s.result() == "x\r\n");
	s.reset(); s.setOutType(html); s.WriteLn("x");
	REQUIRE(s.result() == "x\r\n");
	s.reset(); s.setOutType(xml); s.WriteLn("x");
	REQUIRE(s.result() == "x\r\n");
	s.reset(); s.setOutType(json); s.WriteLn("x");
	REQUIRE(s.result() == "x\r\n");
}

TEST_CASE("Write ends no line at all", "[compat][serializer]")
{
	Serializer s;
	s.Write("a");
	s.Write(std::string("b"));
	REQUIRE(s.result() == "ab");
}

TEST_CASE("a pair and a written line mix the two endings", "[compat][serializer]")
{
	// this is why the consumers run their answers through dos2unix
	Serializer s;
	s.setOutType(plain);
	s.Write(s.outPair("a", "1", false));
	s.WriteLn("done");
	REQUIRE(s.result() == "a=1\ndone\r\n");
}

TEST_CASE("printf formats into the result", "[compat][serializer]")
{
	Serializer s;
	s.printf("%s=%d\n", "bouquet", 7);
	REQUIRE(s.result() == "bouquet=7\n");
}

TEST_CASE("printf cuts at the buffer the old server uses", "[compat][serializer]")
{
	// 4096 bytes, of which the last is the terminator
	Serializer s;
	s.printf("%s", std::string(5000, 'x').c_str());
	REQUIRE(s.result() == std::string(4095, 'x'));
}

TEST_CASE("SendHTMLHeader writes the bytes the old server writes", "[compat][serializer]")
{
	Serializer s;
	s.SendHTMLHeader("Neutrino");
	REQUIRE(s.result() ==
		"<html>\n<head><title>Neutrino</title>\n\r\n"
		"<meta http-equiv=\"cache-control\" content=\"no-cache\" />\r\n"
		"<meta http-equiv=\"expires\" content=\"0\" />\n</head>\n<body>\n\r\n");
}

TEST_CASE("SendHTMLFooter writes the bytes the old server writes", "[compat][serializer]")
{
	Serializer s;
	s.SendHTMLFooter();
	REQUIRE(s.result() == "</body>\n</html>\n\n\r\n");
}

TEST_CASE("reset empties the result and the output state", "[compat][serializer]")
{
	Serializer s;
	s.setOutType(json);
	s.nonPair = true;
	s.Write("x");
	s.ParamList["format"] = "xml";
	s.outStart();
	s.reset();
	REQUIRE(s.result() == "");
	REQUIRE(s.outType == plain);
	REQUIRE(s.nonPair == false);
	// the head the run before it stated, which a second run would otherwise
	// carry on answering with
	REQUIRE(s.outHttpStatus == -1);
	REQUIRE(s.outMimeType == "");
}

//=============================================================================
// The helpers the serialiser is built on, where a consumer can see them
//=============================================================================

TEST_CASE("ySplitString cuts at the first character of the delimiter set", "[compat][helper]")
{
	std::string left, right;
	REQUIRE(httpd::compat::ySplitString("channel id=\"7\"", " ", left, right) == true);
	REQUIRE(left == "channel");
	REQUIRE(right == "id=\"7\"");

	REQUIRE(httpd::compat::ySplitString("channel", " ", left, right) == false);
	REQUIRE(left == "channel");
	REQUIRE(right == "");
}

TEST_CASE("json_out_success wraps the content the way the old server does", "[compat][helper]")
{
	REQUIRE(httpd::compat::json_out_success("\"a\": \"1\"")
	        == "{\"success\": \"true\", \"data\":{\"a\": \"1\"}}");
}

TEST_CASE("json_out_error names the text under msg's sibling", "[compat][helper]")
{
	// the error key here is "text" and SendError writes "msg"; both ship
	REQUIRE(httpd::compat::json_out_error("no channel")
	        == "{\"success\": \"false\", \"error\":{\"text\": \"no channel\"}}");
}

TEST_CASE("json_out_pair drops a quote in the key and bends one in the value", "[compat][helper]")
{
	REQUIRE(httpd::compat::json_out_pair("a\"b", "c\"d") == "\"ab\": \"c'd\"");
}

TEST_CASE("decodeString reads the plus as a space", "[compat][helper]")
{
	REQUIRE(httpd::compat::decodeString("a+b%20c%3Fd") == "a b c?d");
}

TEST_CASE("decodeString eats three characters whatever the two after the sign are",
          "[compat][helper]")
{
	// nothing readable in them means the number is nought, so the escape
	// becomes a null byte and the two characters are gone with it. A consumer
	// sending a bare percent sign in a value gets this and not the sign back
	REQUIRE(httpd::compat::decodeString("a%zzb") == std::string("a\0b", 3));
	REQUIRE(httpd::compat::decodeString("%zz") == std::string("\0", 1));
	// half of a readable pair is read as far as it goes
	REQUIRE(httpd::compat::decodeString("a%2zb") == std::string("a\x02" "b", 3));
}

TEST_CASE("decodeString leaves an escape that runs off the end alone",
          "[compat][helper]")
{
	// one short of a whole escape is not an escape, and the byte after the
	// string is not the sign's second digit
	REQUIRE(httpd::compat::decodeString("a%2") == "a%2");
	REQUIRE(httpd::compat::decodeString("a%") == "a%");
	REQUIRE(httpd::compat::decodeString("%") == "%");
	// one more character and it is one, which is where the boundary sits
	REQUIRE(httpd::compat::decodeString("a%20") == "a ");
}

TEST_CASE("encodeString writes every non alphanumeric as an entity", "[compat][helper]")
{
	// the terminator it appends is part of the string it returns
	REQUIRE(httpd::compat::encodeString("a b") == std::string("a&#32;b\0", 8));
	// a digit is alphanumeric and stays, which "a b" cannot say
	REQUIRE(httpd::compat::encodeString("a1 b") == std::string("a1&#32;b\0", 9));
	// and a byte above ASCII is numbered as unsigned, so it stays an entity a
	// browser can read. This is what a bouquet name with an umlaut goes through
	REQUIRE(httpd::compat::encodeString("\xe4") == std::string("&#228;\0", 7));
	REQUIRE(httpd::compat::encodeString("\xff") == std::string("&#255;\0", 7));
}

TEST_CASE("itoa and itoh write the widths the old server writes", "[compat][helper]")
{
	REQUIRE(httpd::compat::itoa(7) == "7");
	REQUIRE(httpd::compat::itoh(7) == "0x000007");
	// lower case, and nothing below 7 has a letter in it to say so. These are
	// the pids and ids the old server writes into an answer
	REQUIRE(httpd::compat::itoh(0xabc) == "0x000abc");
	REQUIRE(httpd::compat::itoh(0xffffff) == "0xffffff");
	// six places is a floor and not a ceiling
	REQUIRE(httpd::compat::itoh(0x1234567) == "0x1234567");
	// read as unsigned, which only a value above the signed range says
	REQUIRE(httpd::compat::itoa(4294967295u) == "4294967295");
	REQUIRE(httpd::compat::itoa(2147483648u) == "2147483648");
}

TEST_CASE("trim takes whitespace off both ends", "[compat][helper]")
{
	REQUIRE(httpd::compat::trim("  a b \r\n") == "a b");
	REQUIRE(httpd::compat::trim("   ") == "");
	// the tab is in the default set as well, and every query value goes
	// through here, so a tab written as %09 is dropped rather than carried
	REQUIRE(httpd::compat::trim("\ta\t") == "a");
	REQUIRE(httpd::compat::trim("\t \r\n") == "");
	// only the ends, never the middle
	REQUIRE(httpd::compat::trim(" a\tb ") == "a\tb");
}

TEST_CASE("minmax holds a value between its ends", "[compat][helper]")
{
	REQUIRE(httpd::compat::minmax(5, 0, 3) == 3);
	REQUIRE(httpd::compat::minmax(-1, 0, 3) == 0);
	REQUIRE(httpd::compat::minmax(2, 0, 3) == 2);
}

TEST_CASE("string_printf cuts at the same buffer the writer does", "[compat][helper]")
{
	REQUIRE(httpd::compat::string_printf("%s-%d", "a", 7) == "a-7");
	REQUIRE(httpd::compat::string_printf("%s", std::string(5000, 'x').c_str())
	        == std::string(4095, 'x'));
}

TEST_CASE("replace walks past what it just wrote", "[compat][helper]")
{
	// a replacement carrying the pattern would otherwise never end
	std::string s = "a";
	httpd::compat::replace(s, "a", "aa");
	REQUIRE(s == "aa");

	std::string t = "aXbXc";
	httpd::compat::replace(t, "X", "YY");
	REQUIRE(t == "aYYbYYc");

	std::string u = "abc";
	httpd::compat::replace(u, "z", "y");
	REQUIRE(u == "abc");
}

TEST_CASE("ySplitStringExact wants the whole delimiter", "[compat][helper]")
{
	// the difference to ySplitString, which cuts at any one of its characters
	std::string left, right;
	REQUIRE(httpd::compat::ySplitStringExact("a::b", "::", left, right) == true);
	REQUIRE(left == "a");
	REQUIRE(right == "b");

	REQUIRE(httpd::compat::ySplitString("a::b", "::", left, right) == true);
	REQUIRE(left == "a");
	REQUIRE(right == ":b");

	// not found hands the whole string back on the left. The query reader gets
	// the last parameter of every request through this branch, so an empty
	// left here loses it
	REQUIRE(httpd::compat::ySplitStringExact("abc", "&", left, right) == false);
	REQUIRE(left == "abc");
	REQUIRE(right == "");
	REQUIRE(httpd::compat::ySplitStringExact("", "&", left, right) == false);
	REQUIRE(left == "");
	REQUIRE(right == "");
	// a delimiter that is only half there is not there
	REQUIRE(httpd::compat::ySplitStringExact("a:b", "::", left, right) == false);
	REQUIRE(left == "a:b");
}

TEST_CASE("ySplitStringLast cuts at the last one", "[compat][helper]")
{
	std::string left, right;
	REQUIRE(httpd::compat::ySplitStringLast("a/b/c", "/", left, right) == true);
	REQUIRE(left == "a/b");
	REQUIRE(right == "c");

	REQUIRE(httpd::compat::ySplitStringLast("abc", "/", left, right) == false);
	REQUIRE(left == "abc");
	REQUIRE(right == "");

	// it cuts at the last of the characters and not at the last whole string,
	// which only a delimiter longer than one character can say
	REQUIRE(httpd::compat::ySplitStringLast("a::b", "::", left, right) == true);
	REQUIRE(left == "a:");
	REQUIRE(right == "b");
	REQUIRE(httpd::compat::ySplitStringLast("a/b.c", "./", left, right) == true);
	REQUIRE(left == "a/b");
	REQUIRE(right == "c");
}

TEST_CASE("ySplitStringVector keeps the empty piece a trailing delimiter leaves",
          "[compat][helper]")
{
	httpd::compat::CStringArray v = httpd::compat::ySplitStringVector("a,b,c", ",");
	REQUIRE(v.size() == 3);
	REQUIRE(v[0] == "a");
	REQUIRE(v[1] == "b");
	REQUIRE(v[2] == "c");

	httpd::compat::CStringArray w = httpd::compat::ySplitStringVector("a,", ",");
	REQUIRE(w.size() == 2);
	REQUIRE(w[0] == "a");
	REQUIRE(w[1] == "");

	httpd::compat::CStringArray e = httpd::compat::ySplitStringVector("", ",");
	REQUIRE(e.size() == 1);
	REQUIRE(e[0] == "");
}

TEST_CASE("string_tolower and nocase_compare fold the case", "[compat][helper]")
{
	REQUIRE(httpd::compat::string_tolower("AbC1") == "abc1");
	REQUIRE(httpd::compat::nocase_compare('a', 'A') == true);
	REQUIRE(httpd::compat::nocase_compare('a', 'b') == false);
}

TEST_CASE("json_out_quote_convert bends a quote", "[compat][helper]")
{
	REQUIRE(httpd::compat::json_out_quote_convert("a\"b\"c") == "a'b'c");
}

TEST_CASE("isControlCharacter answers for neither end of what it names",
          "[compat][helper]")
{
	// the comment beside it says [0,32]; the code says (0,31]
	REQUIRE(httpd::compat::isControlCharacter('\0') == false);
	REQUIRE(httpd::compat::isControlCharacter('\x01') == true);
	REQUIRE(httpd::compat::isControlCharacter('\x1f') == true);
	REQUIRE(httpd::compat::isControlCharacter(' ') == false);
}

TEST_CASE("correctTime holds every field between its ends", "[compat][helper]")
{
	struct tm zt;
	std::memset(&zt, 0, sizeof(zt));
	zt.tm_year = 400;
	zt.tm_mon = 40;
	zt.tm_mday = 0;
	zt.tm_hour = 40;
	zt.tm_min = 90;
	zt.tm_sec = 90;
	zt.tm_isdst = 1;
	httpd::compat::correctTime(&zt);
	REQUIRE(zt.tm_year == 129);
	REQUIRE(zt.tm_mon == 11);
	REQUIRE(zt.tm_mday == 1);
	REQUIRE(zt.tm_hour == 23);
	REQUIRE(zt.tm_min == 59);
	REQUIRE(zt.tm_sec == 59);
	// left for mktime to work out rather than stated
	REQUIRE(zt.tm_isdst == -1);
}

TEST_CASE("correctTime holds every field at its other end too", "[compat][helper]")
{
	// the case above drives five fields up and one down, so each field has an
	// end nothing was holding. A recording's start and stop time go through here
	struct tm zt;
	std::memset(&zt, 0, sizeof(zt));
	zt.tm_year = -5;
	zt.tm_mon = -3;
	zt.tm_mday = 400;
	zt.tm_hour = -1;
	zt.tm_min = -1;
	zt.tm_sec = -1;
	zt.tm_isdst = 0;
	httpd::compat::correctTime(&zt);
	REQUIRE(zt.tm_year == 0);
	REQUIRE(zt.tm_mon == 0);
	REQUIRE(zt.tm_mday == 31);
	REQUIRE(zt.tm_hour == 0);
	REQUIRE(zt.tm_min == 0);
	REQUIRE(zt.tm_sec == 0);
	REQUIRE(zt.tm_isdst == -1);
}

TEST_CASE("timeString writes hours and minutes in the local zone", "[compat][helper]")
{
	// the zone is the process's, so it is named here and put back. A zone with
	// an offset is what makes this say anything: under UTC the local and the
	// universal answer are the same string, and the EPG start times this writes
	// are read in the box's own zone
	const char *was = getenv("TZ");
	const std::string keep = (was != NULL) ? std::string(was) : std::string();
	setenv("TZ", "CET-1", 1);
	tzset();

	REQUIRE(httpd::compat::timeString(3661) == "02:01");
	REQUIRE(httpd::compat::timeString(0) == "01:00");

	setenv("TZ", "UTC0", 1);
	tzset();

	REQUIRE(httpd::compat::timeString(3661) == "01:01");
	REQUIRE(httpd::compat::timeString(0) == "00:00");

	if (was != NULL)
		setenv("TZ", keep.c_str(), 1);
	else
		unsetenv("TZ");
	tzset();
}

TEST_CASE("write_to_file writes, appends and says when it cannot", "[compat][helper]")
{
	// the name carries the process, because two runs of this suite at once
	// would otherwise write and read one another's file
	char name[64];
	std::snprintf(name, sizeof(name), "/tmp/ni-web-compat-helper-probe-%ld.txt",
	              (long) getpid());
	const std::string path(name);
	unlink(path.c_str());

	REQUIRE(httpd::compat::write_to_file(path, "one") == true);
	REQUIRE(readBack(path) == "one");

	REQUIRE(httpd::compat::write_to_file(path, "two", true) == true);
	REQUIRE(readBack(path) == "onetwo");

	// the third argument defaults to overwriting, which is the one an answer
	// that reuses a name depends on
	REQUIRE(httpd::compat::write_to_file(path, "three") == true);
	REQUIRE(readBack(path) == "three");

	// nothing to write is still a write: the file is emptied and the answer is
	// still yes, which is what a caller replacing a file with an empty one gets
	REQUIRE(httpd::compat::write_to_file(path, "") == true);
	REQUIRE(readBack(path) == "");

	REQUIRE(httpd::compat::write_to_file("/no/such/directory/probe.txt", "x") == false);

	unlink(path.c_str());
}

#endif // DISABLE_LEGACY_API

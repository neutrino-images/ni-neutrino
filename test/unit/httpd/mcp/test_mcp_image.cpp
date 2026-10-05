/*
 * test_mcp_image.cpp - a flagged route answering a picture, as image content
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

#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/mcp/composed.h"
#include "httpd/mcp/contract.h"
#include "httpd/mcp/picture.h"
#include "httpd/mcp/routetools.h"
#include "httpd/mcp/toolguard.h"
#include "httpd/mcp/wiring.h"

#include "mcpfakes.h"
#include "toolcaller.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

#include <jpeglib.h>

using namespace httpd;

namespace
{

// A JPEG of that size; noisy pictures stay large after scaling, flat ones shrink.
std::string jpegOf(unsigned w, unsigned h, bool noisy)
{
	jpeg_compress_struct c;
	jpeg_error_mgr err;
	c.err = jpeg_std_error(&err);
	jpeg_create_compress(&c);
	unsigned char *mem = NULL;
	unsigned long size = 0;
	jpeg_mem_dest(&c, &mem, &size);
	c.image_width = w;
	c.image_height = h;
	c.input_components = 3;
	c.in_color_space = JCS_RGB;
	jpeg_set_defaults(&c);
	jpeg_set_quality(&c, 95, TRUE);
	jpeg_start_compress(&c, TRUE);
	std::vector<unsigned char> row(w * 3);
	srand(7);
	while (c.next_scanline < h)
	{
		for (unsigned x = 0; x < w * 3; ++x)
			row[x] = noisy ? (unsigned char) (rand() & 255) : (unsigned char) (x % 200);
		JSAMPROW r = &row[0];
		jpeg_write_scanlines(&c, &r, 1);
	}
	jpeg_finish_compress(&c);
	const std::string out((const char *) mem, size);
	jpeg_destroy_compress(&c);
	free(mem);
	return out;
}

std::string &pictureFile() { static std::string p; return p; }
std::string &pictureType() { static std::string t; return t; }

std::string writeTemp(const std::string &bytes)
{
	char tmpl[] = "/tmp/mcp_image_XXXXXX";
	const int fd = mkstemp(tmpl);
	if (fd < 0)
		return std::string();
	const ssize_t n = ::write(fd, bytes.data(), bytes.size());
	::close(fd);
	return n == (ssize_t) bytes.size() ? std::string(tmpl) : std::string();
}

Response logo(const Request &)
{
	const int fd = ::open(pictureFile().c_str(), O_RDONLY | O_CLOEXEC);
	Response out;
	out.code = StatusOk;
	out.content_type = pictureType();
	answerFromDescriptor(out, fd);
	return out;
}

const Param kLogoParams[] = {
	HTTPD_SEGMENT("id", ParamType::ChannelId, "the channel, hexadecimal"),
};

const RouteRefusal kLogoRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchLogo, "the box has no picture for that channel"),
};

const Endpoint kLogo[] = {
	{ Method::Get, "/api/v1/things/{id}/logo", AuthLevel::Read, "a picture", NULL,
	  HTTPD_PARAMS(kLogoParams), NULL, &logo, false, Answers200 | Answers206, HTTPD_REFUSALS(kLogoRefusals) },
};

const ToolFlag kLogoTools[] = {
	HTTPD_TOOL_IMAGE(Method::Get, "/api/v1/things/{id}/logo", "thing_logo", "The picture of one thing."),
};

const RouteTable kLogoTable = { HTTPD_TABLE_WITH_TOOLS("things", kLogo, kLogoTools) };
const RouteTable *const kTables[] = { &kLogoTable };

mcp::RouteTools &tools()
{
	static mcp::RouteTools t(kTables, 1, mcp::composedTable());
	return t;
}

} // namespace

TEST_CASE("an image flag is a tool with no output schema that answers a picture", "[image]")
{
	REQUIRE(tools().refusal().empty());
	const std::vector<mcp::ToolDef> all = tools().list();
	const mcp::ToolDef *d = NULL;
	for (size_t i = 0; i < all.size(); ++i)
		d = all[i].name == "thing_logo" ? &all[i] : d;
	REQUIRE(d != NULL);
	REQUIRE(d->image);
	REQUIRE(d->output.empty());

	pictureFile() = writeTemp(std::string("\x89PNG\r\n\x1a\n" "data", 12));
	pictureType() = "image/png";
	coreapi::Result<mcp::JsonText> r = tools().call(callerAt(AuthLevel::Read), "thing_logo", "{\"id\":\"283d\"}");
	REQUIRE(r.ok());
	REQUIRE(r.value() == "{\"mime_type\":\"image/png\",\"data\":\"" +
	                     mcp::encodeBase64(std::string("\x89PNG\r\n\x1a\n" "data", 12)) + "\"}");
	unlink(pictureFile().c_str());
}

TEST_CASE("a vector logo and a picture over the cap are refused", "[image]")
{
	pictureFile() = writeTemp("<svg xmlns=\"http://www.w3.org/2000/svg\"/>");
	pictureType() = "image/svg+xml";
	coreapi::Result<mcp::JsonText> svg = tools().call(callerAt(AuthLevel::Read), "thing_logo", "{\"id\":\"283d\"}");
	REQUIRE_FALSE(svg.ok());
	REQUIRE(svg.error().code == coreapi::ErrorCode::NoSuchLogo);
	unlink(pictureFile().c_str());

	pictureFile() = writeTemp(std::string(mcp::kMaxImageText, 'x'));
	pictureType() = "image/png";
	coreapi::Result<mcp::JsonText> big = tools().call(callerAt(AuthLevel::Read), "thing_logo", "{\"id\":\"283d\"}");
	REQUIRE_FALSE(big.ok());
	REQUIRE(big.error().code == coreapi::ErrorCode::OutputTooLarge);
	unlink(pictureFile().c_str());
}

TEST_CASE("a content type other than png jpeg or gif is refused and so is an empty file", "[image]")
{
	pictureFile() = writeTemp("not a picture at all");
	pictureType() = "application/octet-stream";
	coreapi::Result<mcp::JsonText> odd = tools().call(callerAt(AuthLevel::Read), "thing_logo", "{\"id\":\"283d\"}");
	REQUIRE_FALSE(odd.ok());
	REQUIRE(odd.error().code == coreapi::ErrorCode::NoSuchLogo);
	unlink(pictureFile().c_str());

	pictureFile() = writeTemp(std::string());
	pictureType() = "image/png";
	coreapi::Result<mcp::JsonText> empty = tools().call(callerAt(AuthLevel::Read), "thing_logo", "{\"id\":\"283d\"}");
	REQUIRE_FALSE(empty.ok());
	REQUIRE(empty.error().code == coreapi::ErrorCode::NoSuchLogo);
	unlink(pictureFile().c_str());
}

namespace
{

// One image tool through the whole endpoint.
class PictureTools : public mcp::ToolSource
{
	public:
		std::vector<mcp::ToolDef> list()
		{
			mcp::ToolDef d = mcpfake::tool("look", AuthLevel::Read, true, false, true,
			                               "{\"type\":\"object\"}", "");
			d.image = true;
			return std::vector<mcp::ToolDef>(1, d);
		}
		coreapi::Result<mcp::JsonText> call(const mcp::Caller &, const std::string &, const mcp::JsonText &)
		{
			return coreapi::ok(std::string("{\"mime_type\":\"image/jpeg\",\"data\":\"/9j/\",\"width\":1280,\"height\":720}"));
		}
		std::string hint(const std::string &, coreapi::ErrorCode) const { return std::string(); }
};

// Answers a kind of JSON an image tool must not, to prove the endpoint refuses it instead of
// reading a member of something that is not an object.
class OddPictureTools : public mcp::ToolSource
{
	public:
		std::string answer;
		std::vector<mcp::ToolDef> list()
		{
			mcp::ToolDef d = mcpfake::tool("look", AuthLevel::Read, true, false, true,
			                               "{\"type\":\"object\"}", "");
			d.image = true;
			return std::vector<mcp::ToolDef>(1, d);
		}
		coreapi::Result<mcp::JsonText> call(const mcp::Caller &, const std::string &, const mcp::JsonText &)
		{
			return coreapi::ok(answer);
		}
		std::string hint(const std::string &, coreapi::ErrorCode) const { return std::string(); }
};

} // namespace

TEST_CASE("the endpoint sends an image tool's answer as image content", "[image]")
{
	mcpfake::Wired wired;
	static PictureTools pictures;
	const mcp::Wiring w = { &pictures, &mcpfake::verify };
	mcp::install(w);

	const Response listed = mcpfake::roundTrip(mcpfake::modernHead("tools/list"),
	                                           mcpfake::modernBody("1", "tools/list", ""));
	const mcp::JsonValue l = mcpfake::parsed(listed.body);
	REQUIRE(l["result"]["tools"][0]["name"].asString() == "look");
	REQUIRE_FALSE(l["result"]["tools"][0].isMember("outputSchema"));

	const Response called = mcpfake::roundTrip(mcpfake::modernHead("tools/call", "look"),
	                                           mcpfake::modernBody("1", "tools/call", "\"name\":\"look\""));
	const mcp::JsonValue c = mcpfake::parsed(called.body);
	REQUIRE(c["result"]["content"][0]["type"].asString() == "image");
	REQUIRE(c["result"]["content"][0]["mimeType"].asString() == "image/jpeg");
	REQUIRE(c["result"]["content"][0]["data"].asString() == "/9j/");
	REQUIRE(c["result"]["content"][1]["text"].asString() == "1280x720 image/jpeg");
	REQUIRE_FALSE(c["result"].isMember("structuredContent"));
	REQUIRE_FALSE(c["result"]["isError"].asBool());
}

TEST_CASE("the endpoint sends an image tool's answer as image content in the older era", "[image]")
{
	mcpfake::Wired wired;
	static PictureTools pictures;
	const mcp::Wiring w = { &pictures, &mcpfake::verify };
	mcp::install(w);

	const Response called = mcpfake::roundTrip(mcpfake::legacyHead(),
	                                           mcpfake::legacyBody("1", "tools/call", "\"name\":\"look\""));
	const mcp::JsonValue c = mcpfake::parsed(called.body);
	REQUIRE(c["result"]["content"][0]["type"].asString() == "image");
	REQUIRE(c["result"]["content"][0]["mimeType"].asString() == "image/jpeg");
	REQUIRE(c["result"]["content"][0]["data"].asString() == "/9j/");
	REQUIRE(c["result"]["content"][1]["text"].asString() == "1280x720 image/jpeg");
	REQUIRE_FALSE(c["result"]["isError"].asBool());
}

TEST_CASE("an image tool answering something that is not an object is refused and not an assertion", "[image]")
{
	mcpfake::Wired wired;
	static OddPictureTools pictures;
	const mcp::Wiring w = { &pictures, &mcpfake::verify };
	mcp::install(w);

	pictures.answer = "[1,2,3]";
	Response called = mcpfake::roundTrip(mcpfake::modernHead("tools/call", "look"),
	                                     mcpfake::modernBody("1", "tools/call", "\"name\":\"look\""));
	mcp::JsonValue c = mcpfake::parsed(called.body);
	REQUIRE(c["result"]["isError"].asBool());

	pictures.answer = "\"plain text\"";
	called = mcpfake::roundTrip(mcpfake::modernHead("tools/call", "look"),
	                            mcpfake::modernBody("1", "tools/call", "\"name\":\"look\""));
	c = mcpfake::parsed(called.body);
	REQUIRE(c["result"]["isError"].asBool());
}

TEST_CASE("screenshot answers the screen scaled to fit as a JPEG picture", "[image][screenshot]")
{
	FakeScreenshotSource screen;
	InstalledScreenshotSource in_screen(&screen);
	screen.content = jpegOf(1920, 1080, false);
	mcp::ToolSource &box = mcp::boxTools();
	coreapi::Result<mcp::JsonText> r = box.call(callerAt(AuthLevel::Read), "screenshot", "{\"video\":false}");
	REQUIRE(r.ok());
	const mcp::JsonValue v = mcpfake::parsed(r.value());
	REQUIRE(v["mime_type"].asString() == "image/jpeg");
	REQUIRE(v["width"].asUInt() <= 1280);
	REQUIRE(v["data"].asString().size() <= mcp::kMaxImageText);
	REQUIRE(screen.last_format == coreapi::PictureFormat::Jpeg);
	REQUIRE(screen.last_osd);
	REQUIRE_FALSE(screen.last_video);
}

TEST_CASE("a screen that cannot be read or decoded is refused as not captured", "[image][screenshot]")
{
	FakeScreenshotSource screen;
	InstalledScreenshotSource in_screen(&screen);
	screen.screen_status = coreapi::Status::Internal;
	mcp::ToolSource &box = mcp::boxTools();
	REQUIRE(box.call(callerAt(AuthLevel::Read), "screenshot", "{}").error().code ==
	        coreapi::ErrorCode::ScreenNotCaptured);
	screen.screen_status = coreapi::Status::Ok;
	screen.content = "not a picture";
	REQUIRE(box.call(callerAt(AuthLevel::Read), "screenshot", "{}").error().code ==
	        coreapi::ErrorCode::ScreenNotCaptured);
}

namespace
{

// Asks for a second picture while the first is being taken.
struct NestedScreen : public FakeScreenshotSource
{
	coreapi::Error inner;

	coreapi::Status captureScreen(bool osd, bool video, coreapi::PictureFormat format, const std::string &path)
	{
		if (screen_shots == 0)
		{
			coreapi::Result<mcp::JsonText> r = mcp::boxTools().call(callerAt(AuthLevel::Read), "screenshot", "{}");
			if (!r.ok())
				inner = r.error();
		}
		return FakeScreenshotSource::captureScreen(osd, video, format, path);
	}
};

} // namespace

TEST_CASE("a picture asked for while another is taken is refused busy, as the tool declares", "[image][screenshot]")
{
	NestedScreen screen;
	InstalledScreenshotSource in_screen(&screen);
	screen.content = jpegOf(64, 48, false);
	REQUIRE(mcp::boxTools().call(callerAt(AuthLevel::Read), "screenshot", "{}").ok());
	REQUIRE(screen.inner.code == coreapi::ErrorCode::ScreenNotCaptured);
	REQUIRE(screen.inner.message.find("already taking") != std::string::npos);

	const RouteTable &c = mcp::composedTable();
	bool declared = false;
	for (size_t e = 0; e < c.count; ++e)
	{
		if (std::string(c.endpoints[e].path) != "/mcp/tools/screenshot")
			continue;
		for (size_t r = 0; r < c.endpoints[e].refusal_count; ++r)
			declared = declared || (c.endpoints[e].refusals[r].status == coreapi::Status::Busy &&
			                        c.endpoints[e].refusals[r].code == coreapi::ErrorCode::ScreenNotCaptured);
	}
	REQUIRE(declared);
}

TEST_CASE("a capture that starts like a JPEG but breaks off is too large to answer", "[image][screenshot]")
{
	FakeScreenshotSource screen;
	InstalledScreenshotSource in_screen(&screen);
	screen.content = std::string("\xFF\xD8", 2) + std::string(500, 'x');
	mcp::ToolSource &box = mcp::boxTools();
	REQUIRE(box.call(callerAt(AuthLevel::Read), "screenshot", "{}").error().code ==
	        coreapi::ErrorCode::OutputTooLarge);
}

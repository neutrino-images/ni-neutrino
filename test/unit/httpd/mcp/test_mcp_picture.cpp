/*
 * test_mcp_picture.cpp - base64 and a JPEG scaled to fit an answer
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

#include "httpd/mcp/picture.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

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

} // namespace

TEST_CASE("base64 is the standard alphabet with padding", "[picture]")
{
	REQUIRE(mcp::encodeBase64("") == "");
	REQUIRE(mcp::encodeBase64("f") == "Zg==");
	REQUIRE(mcp::encodeBase64("fo") == "Zm8=");
	REQUIRE(mcp::encodeBase64("foo") == "Zm9v");
	REQUIRE(mcp::encodeBase64(std::string("\xff\xfe\x00", 3)) == "//4A");
}

TEST_CASE("a full HD picture comes back at most 1280 wide and within the cap", "[picture]")
{
	const std::string in = jpegOf(1920, 1080, false);
	std::string out;
	unsigned w = 0;
	unsigned h = 0;
	REQUIRE(mcp::fitJpeg(in, 1280, mcp::kMaxImageText, out, w, h));
	REQUIRE(w <= 1280);
	REQUIRE(w >= 960);
	REQUIRE(h * 16 == w * 9);
	REQUIRE(mcp::encodeBase64(out).size() <= mcp::kMaxImageText);
	REQUIRE((unsigned char) out[0] == 0xff);
	REQUIRE((unsigned char) out[1] == 0xd8);
}

TEST_CASE("a small picture keeps its size", "[picture]")
{
	std::string out;
	unsigned w = 0;
	unsigned h = 0;
	REQUIRE(mcp::fitJpeg(jpegOf(640, 360, false), 1280, mcp::kMaxImageText, out, w, h));
	REQUIRE(w == 640);
	REQUIRE(h == 360);
}

TEST_CASE("a picture that cannot be made to fit is refused", "[picture]")
{
	std::string out;
	unsigned w = 0;
	unsigned h = 0;
	REQUIRE_FALSE(mcp::fitJpeg(jpegOf(3840, 2160, true), 1280, 4096, out, w, h));
	REQUIRE_FALSE(mcp::fitJpeg("not a picture at all", 1280, mcp::kMaxImageText, out, w, h));
	const std::string big = jpegOf(3840, 2160, true);
	REQUIRE(mcp::fitJpeg(big, 1280, mcp::kMaxImageText, out, w, h));
	REQUIRE(w <= 1280);
	REQUIRE(mcp::encodeBase64(out).size() <= mcp::kMaxImageText);
}

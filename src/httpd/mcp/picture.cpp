/*
 * picture.cpp - base64 and a JPEG scaled to fit an answer
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

#include "httpd/mcp/picture.h"

#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <jpeglib.h>

namespace httpd
{
namespace mcp
{

namespace
{

struct Failed
{
	jpeg_error_mgr pub;
	std::jmp_buf   back;
};

// libjpeg reports a broken picture by calling this, which must not return.
void stop(j_common_ptr c)
{
	std::longjmp(((Failed *) c->err)->back, 1);
}

const int kQualities[] = { 80, 65, 50 };

// Holds the setjmp; the struct and its error manager live in the caller's frame.
bool runDecodeScaled(jpeg_decompress_struct *d, const std::string &jpeg, unsigned num,
                     std::vector<unsigned char> &rgb, unsigned &w, unsigned &h)
{
	if (setjmp(((Failed *) d->err)->back))
	{
		jpeg_destroy_decompress(d);
		return false;
	}
	jpeg_create_decompress(d);
	jpeg_mem_src(d, (unsigned char *) jpeg.data(), jpeg.size());
	if (jpeg_read_header(d, TRUE) != JPEG_HEADER_OK)
	{
		jpeg_destroy_decompress(d);
		return false;
	}
	d->scale_num = num;
	d->scale_denom = 8;
	d->out_color_space = JCS_RGB;
	jpeg_start_decompress(d);
	w = d->output_width;
	h = d->output_height;
	rgb.resize((size_t) w * h * 3);
	while (d->output_scanline < d->output_height)
	{
		JSAMPROW row = &rgb[(size_t) d->output_scanline * w * 3];
		jpeg_read_scanlines(d, &row, 1);
	}
	jpeg_finish_decompress(d);
	jpeg_destroy_decompress(d);
	return true;
}

bool decodeScaled(const std::string &jpeg, unsigned num, std::vector<unsigned char> &rgb,
                  unsigned &w, unsigned &h)
{
	jpeg_decompress_struct d;
	Failed err;
	d.err = jpeg_std_error(&err.pub);
	err.pub.error_exit = stop;
	return runDecodeScaled(&d, jpeg, num, rgb, w, h);
}

// Holds the setjmp; the struct, its error manager and mem/size live in the caller's frame.
bool runCompress(jpeg_compress_struct *c, const std::vector<unsigned char> &rgb, unsigned w, unsigned h,
                 int quality, unsigned char **mem, unsigned long *size)
{
	if (setjmp(((Failed *) c->err)->back))
	{
		jpeg_destroy_compress(c);
		return false;
	}
	jpeg_create_compress(c);
	jpeg_mem_dest(c, mem, size);
	c->image_width = w;
	c->image_height = h;
	c->input_components = 3;
	c->in_color_space = JCS_RGB;
	jpeg_set_defaults(c);
	jpeg_set_quality(c, quality, TRUE);
	jpeg_start_compress(c, TRUE);
	while (c->next_scanline < h)
	{
		JSAMPROW row = (JSAMPROW) &rgb[(size_t) c->next_scanline * w * 3];
		jpeg_write_scanlines(c, &row, 1);
	}
	jpeg_finish_compress(c);
	jpeg_destroy_compress(c);
	return true;
}

bool compress(const std::vector<unsigned char> &rgb, unsigned w, unsigned h, int quality,
              unsigned char **mem, unsigned long *size)
{
	jpeg_compress_struct c;
	Failed err;
	c.err = jpeg_std_error(&err.pub);
	err.pub.error_exit = stop;
	return runCompress(&c, rgb, w, h, quality, mem, size);
}

bool encode(const std::vector<unsigned char> &rgb, unsigned w, unsigned h, int quality, std::string &out)
{
	unsigned char *mem = NULL;
	unsigned long size = 0;
	const bool ok = compress(rgb, w, h, quality, &mem, &size);
	if (ok)
		out.assign((const char *) mem, size);
	free(mem);
	return ok;
}

bool runWidthOf(jpeg_decompress_struct *d, const std::string &jpeg, unsigned &w)
{
	if (setjmp(((Failed *) d->err)->back))
	{
		jpeg_destroy_decompress(d);
		return false;
	}
	jpeg_create_decompress(d);
	jpeg_mem_src(d, (unsigned char *) jpeg.data(), jpeg.size());
	const bool ok = jpeg_read_header(d, TRUE) == JPEG_HEADER_OK;
	w = ok ? d->image_width : 0;
	jpeg_destroy_decompress(d);
	return ok;
}

bool widthOf(const std::string &jpeg, unsigned &w)
{
	jpeg_decompress_struct d;
	Failed err;
	d.err = jpeg_std_error(&err.pub);
	err.pub.error_exit = stop;
	return runWidthOf(&d, jpeg, w);
}

} // namespace

std::string encodeBase64(const std::string &bytes)
{
	static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string out;
	out.reserve((bytes.size() + 2) / 3 * 4);
	size_t i = 0;
	for (; i + 2 < bytes.size(); i += 3)
	{
		const unsigned n = ((unsigned char) bytes[i] << 16) | ((unsigned char) bytes[i + 1] << 8) |
		                   (unsigned char) bytes[i + 2];
		out += kAlphabet[(n >> 18) & 63];
		out += kAlphabet[(n >> 12) & 63];
		out += kAlphabet[(n >> 6) & 63];
		out += kAlphabet[n & 63];
	}
	if (i < bytes.size())
	{
		const bool two = i + 1 < bytes.size();
		const unsigned n = ((unsigned char) bytes[i] << 16) | (two ? ((unsigned char) bytes[i + 1] << 8) : 0);
		out += kAlphabet[(n >> 18) & 63];
		out += kAlphabet[(n >> 12) & 63];
		out += two ? kAlphabet[(n >> 6) & 63] : '=';
		out += '=';
	}
	return out;
}

bool fitJpeg(const std::string &jpeg, unsigned max_width, size_t max_text, std::string &out,
             unsigned &width, unsigned &height)
{
	unsigned full = 0;
	if (!widthOf(jpeg, full) || full == 0)
		return false;
	unsigned num = 8;
	while (num > 1 && full * num / 8 > max_width)
		--num;
	for (; num >= 1; --num)
	{
		std::vector<unsigned char> rgb;
		unsigned w = 0;
		unsigned h = 0;
		if (!decodeScaled(jpeg, num, rgb, w, h))
			return false;
		for (size_t q = 0; q < sizeof(kQualities) / sizeof(kQualities[0]); ++q)
		{
			std::string made;
			if (!encode(rgb, w, h, kQualities[q], made))
				return false;
			if ((made.size() + 2) / 3 * 4 <= max_text)
			{
				out.swap(made);
				width = w;
				height = h;
				return true;
			}
		}
	}
	return false;
}

} // namespace mcp
} // namespace httpd

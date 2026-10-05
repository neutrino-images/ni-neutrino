/*
 * picture.h - base64 and a JPEG scaled to fit an answer
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

#ifndef __httpd_mcp_picture_h__
#define __httpd_mcp_picture_h__

#include <cstddef>
#include <string>

namespace httpd
{
namespace mcp
{

// At most this many bytes of base64 in one image answer.
const size_t kMaxImageText = 300 * 1024;

std::string encodeBase64(const std::string &bytes);

bool fitJpeg(const std::string &jpeg, unsigned max_width, size_t max_text, std::string &out,
             unsigned &width, unsigned &height);

} // namespace mcp
} // namespace httpd

#endif

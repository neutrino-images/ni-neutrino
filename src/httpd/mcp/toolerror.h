/*
 * toolerror.h - a refused tool call as text a model can act on
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

#ifndef __httpd_mcp_toolerror_h__
#define __httpd_mcp_toolerror_h__

#include "coreapi/base/result.h"

#include <string>

namespace httpd
{
namespace mcp
{

// The code a client can branch on first, then the words, then the hint.
std::string errorText(const coreapi::Error &e, const std::string &hint);

} // namespace mcp
} // namespace httpd

#endif

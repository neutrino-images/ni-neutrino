/*
 * channelname.h - channels and bouquets by the names people say
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

#ifndef __httpd_mcp_channelname_h__
#define __httpd_mcp_channelname_h__

#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include <string>

namespace httpd
{
namespace mcp
{

// Trimmed and ASCII lowercased; bytes above ASCII compare as they are.
std::string folded(const std::string &s);

// The TV list, then the radio list.
coreapi::Result<coreapi::ChannelList> everyChannel();

coreapi::Result<coreapi::ChannelInfo> resolveChannel(const std::string &text);
coreapi::Result<coreapi::ChannelList> bouquetChannelsNamed(const std::string &text);

} // namespace mcp
} // namespace httpd

#endif

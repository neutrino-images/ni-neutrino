/*
 * aiguides.h - what a person pastes into a tunnel or a client to reach the box
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

#ifndef __httpd_mcp_aiguides_h__
#define __httpd_mcp_aiguides_h__

#include <string>
#include <vector>

namespace httpd
{

namespace exposure
{

struct GuidePlace
{
	std::string              public_url;
	std::string              box;        // http://host[:port] the page reached, empty when unknown
	std::vector<std::string> paths;
	bool                     enabled;
	bool                     allow_lan;
};

// The page keys its words on id and on each warning.
struct TunnelGuide
{
	std::string              id;
	std::string              file;
	std::string              snippet;
	std::vector<std::string> warnings;
	bool                     ready;
};

struct ClientGuide
{
	std::string id;
	std::string needs;    // "public" or "token"
	std::string url;
	std::string command;
	bool        ready;
};

std::vector<TunnelGuide> tunnelGuides(const GuidePlace &p);
std::vector<ClientGuide> clientGuides(const GuidePlace &p);

} // namespace exposure

} // namespace httpd

#endif

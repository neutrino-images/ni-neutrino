/*
 * routetools.h - tools answered by their own routes, in process
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

#ifndef __httpd_mcp_routetools_h__
#define __httpd_mcp_routetools_h__

#include "httpd/endpoint.h"
#include "httpd/mcp/contract.h"

#include <cstddef>
#include <string>
#include <vector>

namespace httpd
{
namespace mcp
{

// A flag set that breaks a rule offers nothing, not the part that passed.
class RouteTools : public ToolSource
{
	public:
		RouteTools(const RouteTable *const *tables, size_t table_count,
		           const RouteTable &composed);

		const std::string &refusal() const;

		std::vector<ToolDef> list();
		coreapi::Result<JsonText> call(const Caller &c, const std::string &name,
		                               const JsonText &args);
		std::string hint(const std::string &name, coreapi::ErrorCode code) const;

	private:
		struct Entry
		{
			ToolDef           def;
			const RouteTable *table;
			const Endpoint   *route;
			const ToolFlag   *flag;
		};

		void add(const RouteTable &t);
		const Entry *find(const std::string &name) const;
		// Whether the tool is in one of the caller's groups and within its level.
		bool offers(const Caller &c, const char *name) const;

		std::vector<Entry> entries_;
		std::string        refusal_;
};

} // namespace mcp
} // namespace httpd

#endif

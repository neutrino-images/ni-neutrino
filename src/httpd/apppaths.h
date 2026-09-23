/*
 * apppaths.h - where the web files live, and which of them may be served
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

#ifndef __httpd_apppaths_h__
#define __httpd_apppaths_h__

#include <cstddef>
#include <string>

namespace httpd
{

/* Whether a target the directory does not hold is one the page draws.

   The page routes on real paths, so /channels/12ab is a name a person may type, bookmark
   or reload, and nothing is installed under it. What answers it is the document, drawn
   for that path by the script in it.

   Not a fallback on everything, and that is the whole of this file. A page answered for
   every miss answers /vendor/pract.js with 200 and a document, and a browser then reports
   a syntax error on line 1 of what it took for a module rather than saying the name is
   not there. It would also make the directory walkable.

   So the areas are named, here and in the source rather than in a configuration, the same
   way a route table is named in routes.cpp: a fallback that can be configured is a
   misconfiguration nobody notices.

   The answer is 200 and not 404: /channels/nosuchthing is a page the application draws. */
bool answersWithPage(const std::string &path);

/* The areas themselves, as the list rather than as something a caller
   assembles. Read by the cases and by the check that holds this list and the
   navigation of the page to being the same seven names. */
const char *const *appAreas(std::size_t *count);

} // namespace httpd

#endif

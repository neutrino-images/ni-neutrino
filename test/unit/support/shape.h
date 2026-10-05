/*
 * shape.h - an answer held to the shape its route declares
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

#ifndef __test_shape_h__
#define __test_shape_h__

#include "httpd/schema.h"

#include "jsoncpp/json/json.h"

#include <string>

bool isHexIdentifier(const std::string &s);
bool typeMatches(const ::Json::Value &v, httpd::FieldType t);
const char *typeName(httpd::FieldType t);

void checkShape(const ::Json::Value &v, const httpd::Schema &s, const std::string &where);

#endif

/*
 * consent.h - the page where the box's owner signs in and decides
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

#ifndef __httpd_oauth_consent_h__
#define __httpd_oauth_consent_h__

#include "httpd/endpoint.h"
#include "httpd/http.h"

#include <string>

namespace httpd
{
namespace oauth
{

enum class Lang
{
	De,
	En
};

// Highest q among de* and en*, German when neither is named.
Lang pickLanguage(const std::string &accept_language);

struct ConsentInput
{
	Method      method;
	std::string query;
	std::string body;
	std::string content_type;
	std::string cookie;
	std::string accept_language;
	std::string peer;
	Origin      origin;

	ConsentInput() : method(UnknownMethod), origin(Origin::Tunnel) {}
};

Response answerConsent(const ConsentInput &in);

std::string htmlEscape(const std::string &s);

} // namespace oauth
} // namespace httpd

#endif

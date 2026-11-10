/*
 * script misc
 *
 * The two calls of the misc module that carry work of their own, written here
 * for both bindings: script_misc.h says what of it is shared and what a binding
 * still reads itself.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "script_misc.h"

#include <config.h>

int ScriptMisc::strFind(const std::string &text, const std::string &pattern, int pos, int n)
{
	size_t from = (pos < 0) ? 0 : (size_t)pos;
	size_t ret = (n < 0) ? text.find(pattern, from) : text.find(pattern.c_str(), from, (size_t)n);

	return (ret == std::string::npos) ? -1 : (int)ret;
}

std::string ScriptMisc::strSub(const std::string &text, int pos, int len)
{
	if (pos < 0 || (size_t)pos > text.length())
		return std::string();

	if (len < 0)
		return text.substr((size_t)pos);

	return text.substr((size_t)pos, (size_t)len);
}

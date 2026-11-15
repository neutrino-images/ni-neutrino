/*
 * script deprecated
 *
 * The notices both bindings printed: script_deprecated.h says what a binding
 * hands in.
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

#include "script_deprecated.h"

#include <config.h>
#include <cstdio>

#include <global.h>
#include <system/localize.h>

void ScriptDeprecated::paramBool(const char *language, const std::string &where,
		const char *val, const char *trueWord)
{
	printf("[%s Script] \33[1;31m%s\33[0m %s (\33[31m\"%s\"\33[0m)\n"
	       "                      %s \33[32m%s\33[0m.\n"
	       "                      (%s)\n",
	       language,
	       g_Locale->getText(LOCALE_LUA_BOOLPARAM_DEPRECATED1),
	       g_Locale->getText(LOCALE_LUA_BOOLPARAM_DEPRECATED2), val,
	       g_Locale->getText(LOCALE_LUA_BOOLPARAM_DEPRECATED3), trueWord,
	       where.c_str());
}

/* The two renamings read alike but for one word of the locale. */
static void renamed(const char *language, const std::string &where,
		neutrino_locale_t what, const char *oldName, const char *newName)
{
	printf("[%s Script] \33[1;31m%s\33[0m %s \33[33m%s\33[0m %s \33[1;33m%s\33[0m.\n"
	       "                      (%s)\n",
	       language,
	       g_Locale->getText(LOCALE_LUA_FUNCTION_DEPRECATED1),
	       g_Locale->getText(what), oldName,
	       g_Locale->getText(LOCALE_LUA_FUNCTION_DEPRECATED3), newName,
	       where.c_str());
}

void ScriptDeprecated::param(const char *language, const std::string &where,
		const char *oldParam, const char *newParam)
{
	renamed(language, where, LOCALE_LUA_PARAMETER_DEPRECATED2, oldParam, newParam);
}

void ScriptDeprecated::function(const char *language, const std::string &where,
		const char *oldFunc, const char *newFunc)
{
	renamed(language, where, LOCALE_LUA_FUNCTION_DEPRECATED2, oldFunc, newFunc);
}

void ScriptDeprecated::hideParameter(const char *language, const std::string &where)
{
	printf("\33[1;31m[%s script warning]\33[0m %s: Obsolete parameter for hide() in use, please remove!\n",
	       language, where.c_str());
}

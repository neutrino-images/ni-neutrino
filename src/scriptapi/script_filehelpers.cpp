/*
 * the file work both bindings do themselves
 *
 * (C) 2015 M. Liebmann (micha-bbg), and the two bindings this stands in for.
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

#include <config.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <sys/stat.h>

#include <system/helpers.h>

#include "script_filehelpers.h"

bool ScriptFileHelpers::Chmod(const std::string &file, int mode, std::string &error)
{
	error = "";

	/* Hack for convert lua number to octal */
	std::string mode_s = itoa(mode, 10);
	mode_t mode_o = (mode_t)(strtol(mode_s.c_str(), (char **)NULL, 8) & 0x0FFF);

	if (chmod(file.c_str(), mode_o) != 0)
	{
		error = strerror(errno);
		return false;
	}
	return true;
}

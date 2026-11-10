/*
 * the file work both bindings do themselves
 *
 * lua_filehelpers.cpp and py_filehelpers.cpp are thin over CFileHelpers where
 * CFileHelpers does the work - cp is that. Where it does not, both bindings
 * call the system themselves, and did so twice: chmod, ln, touch, rmdir,
 * mkdir, readlink, exist and isEmpty. What of that is one piece of work stands
 * here; what is reading a call of its own language stays with the binding, and
 * so does reporting an error in the shape of that language.
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

#ifndef _SCRIPT_FILEHELPERS_H
#define _SCRIPT_FILEHELPERS_H

#include <string>

class ScriptFileHelpers
{
	public:
		/* f.chmod(file, mode): the mode is the number a script writes, read as
		 * the octal digits it looks like - the rule both bindings wrote out with
		 * the same "Hack" over it. Answers whether it worked, and where it did
		 * not, the word of the system for what went wrong. */
		static bool Chmod(const std::string &file, int mode, std::string &error);
};

#endif //_SCRIPT_FILEHELPERS_H

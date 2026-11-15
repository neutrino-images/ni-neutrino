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
#include <ctime>

class ScriptFileHelpers
{
	public:
		/* What went wrong, and where in neutrino it was noticed: the two
		 * things a binding prints behind the place its script stands at. */
		struct Error
		{
			std::string	msg;
			std::string	file;
			int		line;
			Error() : line(0) {}
		};

		/* f.chmod(file, mode): the mode is the number a script writes, read as
		 * the octal digits it looks like - the rule both bindings wrote out with
		 * the same "Hack" over it. Answers whether it worked, and where it did
		 * not, the word of the system for what went wrong. */
		static bool Chmod(const std::string &file, int mode, std::string &error);

		/* Every call below answers whether it worked and fills the error
		 * where it did not. */

		/* f.cp(from, to, flags) */
		static bool Cp(const char *from, const char *to, const char *flags, Error &error);

		/* f.touch(file[, time]): creates the file where it is missing, and
		 * sets its times - to now, or to the time given. A file just created
		 * keeps the time it got unless one was given. */
		static bool Touch(const char *file, bool haveTime, time_t newTime, Error &error);

		/* f.rmdir(dir) */
		static bool Rmdir(const char *dir, Error &error);

		/* f.mkdir(dir[, mode]): 0755 without a mode; one given is read as
		 * the octal digits it looks like, as chmod reads it. */
		static bool Mkdir(const char *dir, bool haveMode, int mode, Error &error);

		/* f.readlink(link): what the link points at. */
		static bool Readlink(const char *link, std::string &target, Error &error);

		/* f.ln(src, link, flags): 's' a symlink - the only kind there is -
		 * and 'f' over a name that already stands there. */
		static bool Ln(const char *src, const char *link, const char *flags, Error &error);

		/* f.exist(file, flags): 'f' a regular file, 'l' a symlink, 'd' a
		 * directory, the first of them the flags name. A name that is not
		 * there is no error, whatever the flags: the result is false. */
		static bool Exist(const char *file, const char *flags, bool &result, Error &error);

		/* f.empty(file): whether a regular file holds nothing, following a
		 * symlink. Anything else, and a name that is not there, is an error. */
		static bool IsEmpty(const char *file, bool &result, Error &error);
};

#endif //_SCRIPT_FILEHELPERS_H

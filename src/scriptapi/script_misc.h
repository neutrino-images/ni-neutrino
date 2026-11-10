/*
 * script misc
 *
 * The little calls a script has beside its widgets, in the language the scripts
 * are written in: neither this nor anything it calls knows which interpreter
 * asked for it. A binding reads its own arguments and hands them over.
 *
 * What both bindings wrote down twice is here once - and one of them had a hole
 * where the other had a guard: the lua strSub handed its position straight to
 * std::string::substr, which ends the program on one past the end of the text.
 * The guard the python binding wrote stands here now, so the lua side loses the
 * hole together with the duplication.
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

#ifndef _SCRIPTMISC_H
#define _SCRIPTMISC_H

#include <string>

/* What a script has beside its widgets. */
class ScriptMisc
{
	public:
		/* Where the pattern stands in the text, from pos over n characters
		 * of the pattern, or -1 where it does not stand in it. A position
		 * before the text means its beginning, n below zero all of it. */
		static int		strFind(const std::string &text, const std::string &pattern, int pos, int n);

		/* The part of the text from pos over len characters; len below zero
		 * is all that follows from pos. A position outside the text answers
		 * nothing - it does not end the program. */
		static std::string	strSub(const std::string &text, int pos, int len);
};

#endif /* _SCRIPTMISC_H */

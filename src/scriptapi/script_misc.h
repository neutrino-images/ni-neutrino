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

		/* The messages a script may post to the box, by the number it names
		 * them with. Both bindings hand this number out as POSTMSG.STANDBY_ON. */
		enum
		{
			POSTMSG_STANDBY_ON = 1
		};

		/* Sets the volume, held between 0 and 100. Nothing happens where the
		 * volume belongs to the HDMI CEC device. */
		static void		setVolume(int vol);

		/* The revision of the box, and its hardware as vendor_name. */
		static unsigned int	getRevision(std::string &hw);

		/* Whether the API of a binding - apiMajor.apiMinor - is new enough
		 * for the version a script asks for. Where it is not, msg holds what
		 * the binding shows the user, in the language of the box. */
		static bool		versionOk(int major, int minor, int apiMajor, int apiMinor, std::string &msg);

		/* Posts one of the messages above; a number that names none is
		 * dropped, and the answer says so. */
		static bool		postMsg(int msg);

		/* The time of day in seconds, with its fraction. */
		static double		getTimeOfDay();

		/* Runs the flash update in the mode named. */
		static void		flashUpdate(const char *mode);
};

#endif /* _SCRIPTMISC_H */

/*
 * script constants
 *
 * The tables of names a script reads its numbers from - RC['ok'], COL['WHITE'],
 * FONT['MENU'], DIR['CONFIGDIR'] and the rest. Both bindings carried the same
 * lists, a few hundred lines each, and a name added to one had to be added to
 * the other. Here they stand once; a binding walks them and writes each into
 * the shape of its language, a table there and a dict here.
 *
 * What is not here is what differs: APIVERSION, which is the version of the
 * binding itself, and the plugin directories of the other language.
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

#ifndef _SCRIPTCONSTANTS_H
#define _SCRIPTCONSTANTS_H

#include <vector>

class ScriptConstants
{
	public:
		struct Number
		{
			const char	*name;
			long long	value;
		};

		struct Text
		{
			const char	*name;
			const char	*value;
		};

		/* One table, under the name a script knows it by. The colours are
		 * marked, because they are no signed numbers: a binding whose
		 * integers are narrower than a colour has to hand them out as
		 * unsigned ones. */
		struct Table
		{
			const char		*name;
			std::vector<Number>	entries;
			bool			colors;
		};

		/* Every table of numbers. They are read when this is called and
		 * not kept: the screen, the corners and the colours of the theme
		 * follow the settings, so a script started later gets what holds
		 * then. */
		static void numbers(std::vector<Table> &out);

		/* DIR: the directories of the box. */
		static void directories(std::vector<Text> &out);
};

#endif /* _SCRIPTCONSTANTS_H */

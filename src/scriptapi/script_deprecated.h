/*
 * script deprecated
 *
 * What a script is told on the console when it uses something of the API that
 * is on its way out. Both bindings said the same things out of the same locale
 * strings, so that a script carried over from one language to the other reads
 * the same advice; here the sentences stand once.
 *
 * A binding hands in what only it knows: the name of its language, the place
 * its script stands at, and how that language writes a true.
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

#ifndef _SCRIPTDEPRECATED_H
#define _SCRIPTDEPRECATED_H

#include <string>

class ScriptDeprecated
{
	public:
		/* a parameter that was a word and is a boolean now */
		static void paramBool(const char *language, const std::string &where,
				const char *val, const char *trueWord);
		/* a parameter that has a new name */
		static void param(const char *language, const std::string &where,
				const char *oldParam, const char *newParam);
		/* a function that has a new name */
		static void function(const char *language, const std::string &where,
				const char *oldFunc, const char *newFunc);
		/* hide() takes no parameter any more */
		static void hideParameter(const char *language, const std::string &where);
};

#endif /* _SCRIPTDEPRECATED_H */

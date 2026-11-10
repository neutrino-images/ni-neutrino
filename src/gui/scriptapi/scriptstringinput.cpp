/*
 * script string input
 *
 * The dialog a script asks a value in, shown here for both bindings:
 * scriptstringinput.h says what of it is shared and what a binding still reads
 * itself.
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

#include "scriptstringinput.h"

#include <config.h>
#include <global.h>

#include <system/debug.h>
#include <gui/widget/stringinput.h>
#include <neutrino.h>

/* The characters a value may hold, as the lua binding had them. The python
 * binding carried the same line without the paragraph sign behind the
 * exclamation mark; it stands here as the two bytes of utf-8 it is, so that
 * this file stays plain ascii. */
#define SCRIPT_STRINGINPUT_CHARS "abcdefghijklmnopqrstuvwxyz0123456789!\"\xc2\xa7$%&/()=?-.@,_: "

/* How long a value is when a script names no size. */
#define SCRIPT_STRINGINPUT_SIZE 30

ScriptStringinput::params::params(void)
{
	size = SCRIPT_STRINGINPUT_SIZE;
	valid_chars = SCRIPT_STRINGINPUT_CHARS;
	icon = std::string(NEUTRINO_ICON_INFO);
	sms = false;
	pin = false;
	observ = NULL;
}

std::string ScriptStringinput::exec(const params_t &pars)
{
	std::string value = pars.value;

	if (pars.sms && pars.pin)
		dprintf(DEBUG_NORMAL, "[ScriptStringinput]: 'sms' AND 'pin' is defined! 'pin' will be prefered.\n");

	/* The pin input is a dialog of its own, no string input. */
	CMenuTarget *i;
	if (pars.pin)
		i = new CPINInput(pars.name, &value, pars.size,
			NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, pars.valid_chars.c_str(), NULL);
	else
		i = new CStringInput(pars.name, &value, pars.size,
			NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, pars.valid_chars.c_str(), pars.observ, pars.icon.c_str());
	i->exec(NULL, "");
	delete i;

	return value;
}

/*
 * script hint box
 *
 * See scripthintbox.h for what this is for.
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

#include "scripthintbox.h"

#include <cstdlib>
#include <cstring>

#include <global.h>
#include <neutrino.h>
#include <system/debug.h>

ScriptHintbox::ScriptHintbox(const params_t &pars)
{
	/* The box keeps the pointer to the title it is given, so the title has
	 * to outlive the call that named it. */
	caption = strdup(pars.name.c_str());
	b = new CHintBox(caption, pars.text.c_str(), pars.width, pars.icon.c_str());
}

ScriptHintbox::~ScriptHintbox()
{
	free(caption);
	delete b;
}

void ScriptHintbox::paint(void)
{
	b->paint();
}

void ScriptHintbox::hide(void)
{
	b->hide();
}

/* The loop is the one gui/widget/hintbox.cpp runs when a menu shows a box of
 * its own: a script's box is shown, waited on and hidden the same way, and the
 * five seconds it stays unless the script named another are the box's. */
void ScriptHintbox::exec(int timeout)
{
	b->paint();

	neutrino_msg_t msg;
	neutrino_msg_data_t data;
	if (timeout == -1)
		timeout = 5; /* default timeout 5 sec */

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(timeout);

	int res = messages_return::none;

	while (!(res & (messages_return::cancel_info | messages_return::cancel_all)))
	{
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

		if ((msg == CRCInput::RC_timeout) || (msg == CRCInput::RC_ok))
			res = messages_return::cancel_info;
		else if (CNeutrinoApp::getInstance()->backKey(msg))
			res = messages_return::cancel_all;
		else if ((msg == CRCInput::RC_up) || (msg == CRCInput::RC_down))
		{
			if (msg == CRCInput::RC_up)
				b->scroll_up();
			else
				b->scroll_down();
		}
		else if (CNeutrinoApp::getInstance()->listModeKey(msg))
		{
			/* do nothing */
		}
		else if (msg == CRCInput::RC_mode)
		{
			break;
		}
		else if ((msg == CRCInput::RC_next) || (msg == CRCInput::RC_prev))
		{
			res = messages_return::cancel_all;
			g_RCInput->postMsg(msg, data);
		}
		else
		{
			res = CNeutrinoApp::getInstance()->handleMsg(msg, data);
			if (res & messages_return::unhandled)
			{
				/* leave here and handle above... */
				g_RCInput->postMsg(msg, data);
				res = messages_return::cancel_all;
			}
		}
	}
	b->hide();
}

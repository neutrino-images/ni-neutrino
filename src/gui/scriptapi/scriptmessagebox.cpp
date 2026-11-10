/*
 * script message box
 *
 * The box a script shows, shown here for both bindings: scriptmessagebox.h says
 * what of it is shared and what a binding still reads itself.
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

#include "scriptmessagebox.h"

#include <config.h>
#include <cstring>

#include <system/helpers.h>
#include <gui/widget/msgbox.h>

#include "scripttext.h"

/* the words a script names its buttons by */
static long buttonMask(const std::vector<std::string> &buttons)
{
	static const struct { const char *name; long code; } mb[] = {
		{ "yes",	CMsgBox::mbYes },
		{ "no",		CMsgBox::mbNo },
		{ "cancel",	CMsgBox::mbCancel },
		{ "all",	CMsgBox::mbAll },
		{ "back",	CMsgBox::mbBack },
		{ "ok",		CMsgBox::mbOk },
		{ NULL,		0 }
	};

	long show_buttons = 0;
	for (size_t i = 0; i < buttons.size(); i++)
		for (int j = 0; mb[j].name; j++)
			if (!strcmp(mb[j].name, buttons[i].c_str()))
			{
				show_buttons |= mb[j].code;
				break;
			}

	/* a script that names no button gets all of them */
	if ((show_buttons & 0xFF) == 0)
		show_buttons |= CMsgBox::mbAll;

	return show_buttons;
}

/* the word for the button a script wants preselected */
static long defaultButton(const std::string &word)
{
	static const struct { const char *name; long code; } mbr[] = {
		{ "yes",	CMsgBox::mbrYes },
		{ "no",		CMsgBox::mbrNo },
		{ "cancel",	CMsgBox::mbrCancel },
		{ "back",	CMsgBox::mbrBack },
		{ "ok",		CMsgBox::mbrOk },
		{ NULL,		0 }
	};

	for (int i = 0; mbr[i].name; i++)
		if (!strcmp(mbr[i].name, word.c_str()))
			return mbr[i].code;

	return 0;
}

/* and the word an answer is turned back into */
static std::string answerWord(int res)
{
	static const struct { const char *name; long code; } mbr[] = {
		{ "yes",	CMsgBox::mbrYes },
		{ "no",		CMsgBox::mbrNo },
		{ "cancel",	CMsgBox::mbrCancel },
		{ "back",	CMsgBox::mbrBack },
		{ "ok",		CMsgBox::mbrOk },
		{ NULL,		0 }
	};

	for (int i = 0; mbr[i].name; i++)
		if (res == mbr[i].code)
			return mbr[i].name;

	/* what both bindings answered for anything else */
	return "cancel";
}

/* the words for the button alignment */
static long alignMask(const std::string &word)
{
	static const struct { const char *name; long code; } mb[] = {
		{ "center1",	CMsgBox::mbBtnAlignCenter1 },
		{ "center2",	CMsgBox::mbBtnAlignCenter2 },
		{ "left",	CMsgBox::mbBtnAlignLeft },
		{ "right",	CMsgBox::mbBtnAlignRight },
		{ NULL,		0 }
	};

	long show_buttons = 0;
	for (int i = 0; mb[i].name; i++)
		if (!strcmp(mb[i].name, word.c_str()))
		{
			show_buttons |= mb[i].code;
			break;
		}

	return show_buttons;
}

std::string ScriptMessagebox::exec(const params_t &pars)
{
	std::string text = pars.text;
	long show_buttons = alignMask(pars.align) | buttonMask(pars.buttons);
	long default_button = defaultButton(pars.default_button);

	/* the words of the text mode are the ones a text of a script uses; a
	 * script that names none gets the mode a box has */
	long text_mode = DEFAULT_MSGBOX_TEXT_MODE;
	if (!pars.text_mode.empty())
		text_mode = ScriptText::readMode(pars.text_mode, text);

	int res = ShowMsg(pars.name, text, (CMsgBox::msg_result_t)default_button,
		(CMsgBox::button_define_t)show_buttons,
		pars.icon.empty() ? NULL : pars.icon.c_str(),
		pars.width, pars.timeout, pars.return_default_on_timeout, (int)text_mode);

	return answerWord(res);
}

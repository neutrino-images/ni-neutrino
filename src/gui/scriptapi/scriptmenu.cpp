/*
 * script menu
 *
 * See scriptmenu.h for what this is for.
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

#include "scriptmenu.h"

#include <cstdlib>

#include <neutrino.h>

ScriptMenu::ScriptMenu(const params_t &pars)
{
	/* The width a script asked for, where it asked for one. */
	m = pars.width_given ? new CMenuWidget(pars.name.c_str(), pars.icon.c_str(), pars.width)
			     : new CMenuWidget(pars.name.c_str(), pars.icon.c_str());

	if (!pars.buttons.empty())
		m->setFooterButtons(pars.buttons);
}

ScriptMenu::~ScriptMenu()
{
	/* The names a row holds and the option lists a chooser was built from
	 * are the script's, and are let go of with the menu. The rows and the
	 * widget know nothing of them. */
	while (!tofree.empty())
	{
		free(tofree.back());
		tofree.pop_back();
	}
	itemmap.clear();
	delete m;
}

/* An item is filled in by the binding before it is built into a row, so it is
 * made here and answered back. */
ScriptMenuItem *ScriptMenu::add(void)
{
	items.push_back(ScriptMenuItem());
	return &items.back();
}

ScriptMenuItem *ScriptMenu::item(int id)
{
	std::map<int, ScriptMenuItem *>::iterator it = itemmap.find(id);
	return (it == itemmap.end()) ? NULL : it->second;
}

void ScriptMenu::keep(int id, ScriptMenuItem *item)
{
	itemmap.insert(std::make_pair(id, item));
}

/* A masked entry shows that something is set, and nothing of what it is. */
std::string ScriptMenu::maskedText(const std::string &value)
{
	return value.empty() ? "" : "********";
}

/* A chooser without options is a script that asked for something the menu
 * cannot show, and is said so where its row would have been. */
void ScriptMenu::optionError(void)
{
	m->addItem(new CMenuSeparator(CMenuSeparator::STRING | CMenuSeparator::LINE,
			"ERROR! (options_count)", NONEXISTANT_LOCALE));
}

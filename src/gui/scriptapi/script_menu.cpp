/*
 * script menu
 *
 * See script_menu.h for what this is for.
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

#include "script_menu.h"

#include <cstdlib>
#include <cstring>

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

char *ScriptMenu::keepName(const std::string &text)
{
	if (text.empty())
		return NULL;

	char *kept = strdup(text.c_str());
	tofree.push_back(kept);
	return kept;
}

/* What the menu adds for these is the same for every interpreter, so what a
 * script named is all they are. */
bool ScriptMenu::addIfPlain(const std::string &type, const std::string &name)
{
	if (type == "back")
		m->addItem(GenericMenuBack);
	else if (type == "next")
		m->addItem(GenericMenuNext);
	else if (type == "cancel")
		m->addItem(GenericMenuCancel);
	else if (type == "separator")
		m->addItem(GenericMenuSeparator);
	else if ((type == "separatorline") || (type == "subhead"))
	{
		if (!name.empty())
		{
			int flag = (type == "separatorline") ? CMenuSeparator::LINE : CMenuSeparator::SUB_HEAD;
			m->addItem(new CMenuSeparator(CMenuSeparator::STRING | flag, name.c_str(), NONEXISTANT_LOCALE));
		}
		else
			m->addItem(GenericMenuSeparatorLine);
	}
	else
		return false;

	return true;
}

/* The list the binding read is the list the widget is built from; what is left
 * to do here is to hand it over in the shape the widget wants and to find the
 * value the script named in it. */
CMenuOptionChooser::keyval_ext *ScriptMenu::chooserKeys(ScriptMenuItem *item,
		const std::string &value, int &count)
{
	count = (int)item->options.size();

	CMenuOptionChooser::keyval_ext *keys =
		(CMenuOptionChooser::keyval_ext *)calloc(count, sizeof(CMenuOptionChooser::keyval_ext));
	tofree.push_back(keys);

	item->int_val = 0;
	for (int j = 0; j < count; j++)
	{
		keys[j].key = item->options[j].first;
		keys[j].value = NONEXISTANT_LOCALE;
		keys[j].valname = keepName(item->options[j].second);
		if (value == item->options[j].second)
			item->int_val = keys[j].key;
	}
	return keys;
}

int ScriptMenu::addRow(ScriptMenuItem *item, CMenuItem *mi, const ScriptMenuItemParams &pars)
{
	if (mi == NULL)
		return 0;

	const char *hint_icon = keepName(pars.hint_icon);
	if (!pars.hint.empty() || hint_icon)
		mi->setHint(hint_icon, pars.hint);

	m->addItem(mi);

	int id = nextId();
	item->mi = mi;
	keep(id, item);
	return id;
}

/* The row an item becomes, out of what the binding read and what its factory
 * makes for it. What every binding writes the same way is written once here;
 * what a row hands on when it is picked is the binding's, and so is the
 * observer a chooser tells. */
CMenuItem *ScriptMenu::makeRow(ScriptMenuItem *item, const ScriptMenuItemParams &pars, factory *maker)
{
	const std::string &type = item->type;

	/* A row keeps the pointers it is given for as long as the menu lives, so
	 * the names a script wrote are held here, before the row is built. */
	char *icon = keepName(pars.icon);
	char *right_icon = keepName(pars.right_icon);

	CMenuItem *mi = NULL;
	if (type == "forwarder")
	{
		item->str_val = pars.value;
		CMenuTarget *target = maker->forwarder(pars);
		target->setValue(&item->str_val);
		mi = new CMenuForwarder(item->name, pars.enabled, item->str_val, target,
				NULL/*ActionKey*/, pars.directkey, icon, right_icon);
	}
	else if (type == "chooser")
	{
		/* A chooser without options is a script that asked for something
		 * the menu cannot show, and is said so where its row would have
		 * been. */
		if (item->options.empty())
		{
			optionError();
			return NULL;
		}

		int keys_count = 0;
		CMenuOptionChooser::keyval_ext *keys = chooserKeys(item, pars.value, keys_count);
		mi = new CMenuOptionChooser(item->name.c_str(), &item->int_val, keys, keys_count,
				pars.enabled, maker->observer(pars), pars.directkey, icon, pars.pulldown);
	}
	else if (type == "numeric")
	{
		int range_from = 0, range_to = 99;
		sscanf(pars.range.c_str(), "%d,%d", &range_from, &range_to);

		item->int_val = range_from;
		sscanf(pars.value.c_str(), "%d", &item->int_val);
		mi = new CMenuOptionNumberChooser(item->name, &item->int_val, pars.enabled,
				range_from, range_to, maker->observer(pars), 0, 0, NONEXISTANT_LOCALE, pars.pulldown);
	}
	else if (type == "string")
	{
		item->str_val = pars.value;
		mi = new CMenuOptionStringChooser(item->name.c_str(), &item->str_val, pars.enabled,
				maker->observer(pars), pars.directkey, icon, pars.pulldown);
	}
	else if (type == "stringinput")
	{
		item->str_val = pars.value;
		mi = new CMenuForwarder(item->name, pars.enabled, item->str_val,
				maker->stringinput(pars, item, icon), NULL/*ActionKey*/,
				pars.directkey, icon, right_icon);
	}
	else if (type == "keyboardinput")
	{
		item->str_val = pars.value;
		item->masked = pars.masked;

		CMenuTarget *target = maker->keyboardinput(pars, item, icon);
		CMenuForwarder *row = new CMenuForwarder(item->name, pars.enabled, item->str_val, target,
				NULL/*ActionKey*/, pars.directkey, icon, right_icon);
		if (item->masked)
		{
			row->setOption(maskedText(item->str_val));
			maker->setMaskedRow(target, row);
		}
		mi = row;
	}
	else if (type == "filebrowser")
	{
		item->str_val = pars.value;
		mi = new CMenuForwarder(item->name, pars.enabled, item->str_val,
				maker->filebrowser(pars, item), NULL/*ActionKey*/,
				pars.directkey, icon, right_icon);
	}

	return mi;
}

/*
 * settingactive.h - which setting items of a menu are changeable right now
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __settingactive_h__
#define __settingactive_h__

#include <functional>
#include <map>
#include <string>
#include <vector>

/* Kept apart from the widgets so the walk over a menu's items can be tested with
   items that are not widgets. Item needs only setActive(bool).

   An item is active when the screen allows it and its row's conditions hold.
   What the screen allows is a function asked on every pass and never a value
   fixed when the item was built: a screen that mirrors another setting would
   otherwise keep the item dead after that setting changed. Every item of the
   menu is judged again after any change, because the row that changed is rarely
   the one whose state moves. An item is told only when its state differs from
   what it was last given, so a menu is not repainted for nothing. */
template <class Item>
class SettingActiveSet
{
	public:
		typedef bool (*Holds)(const std::string &key);

		typedef std::function<bool()> Screen;

		// An empty screen allows everything. applied is what the item was built as.
		void add(const Item *item, const std::string &key, const Screen &screen, bool applied)
		{
			State s;
			s.key = key;
			s.screen = screen;
			s.applied = applied;
			known[item] = s;
		}

		void forget(const Item *item) { known.erase(item); }

		void reevaluate(const std::vector<Item *> &items, Holds holds)
		{
			for (size_t i = 0; i < items.size(); i++)
			{
				typename std::map<const Item *, State>::iterator it = known.find(items[i]);
				// Separators and items a screen built itself are not ours.
				if (it == known.end())
					continue;
				const bool want = (!it->second.screen || it->second.screen()) && holds(it->second.key);
				if (want == it->second.applied)
					continue;
				it->second.applied = want;
				items[i]->setActive(want);
			}
		}

	private:
		struct State
		{
			std::string key;
			Screen screen;
			bool applied;
		};
		std::map<const Item *, State> known;
};

/* What follows a change of one row, for every shape of row alike: the apply
   registry hears of the key once, then the items of the menu are judged again.
   A menu that is not known skips the second step. */
template <class Item, class Apply>
void settleChange(SettingActiveSet<Item> &set, const std::string &key, Apply apply,
		  std::vector<Item *> *items, typename SettingActiveSet<Item>::Holds holds)
{
	apply(key);
	if (items != NULL)
		set.reevaluate(*items, holds);
}

#endif

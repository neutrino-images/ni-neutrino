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
#include <set>
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

		/* What a screen does once the item's row took effect, a question whose
		   answer can undo the change for one: the box has to show the change
		   before anybody can judge it. True asks for a repaint. */
		typedef std::function<bool()> After;

		/* How an item takes its value again from the settings after a write made
		   elsewhere, and draws it when paint says the item's menu is the one on
		   top. An item that edits the member itself needs only the drawing; one
		   that holds a copy needs both, or the next change made on the box would
		   start from the copy and put it back over the write. */
		typedef std::function<void(bool paint)> Reread;

		/* An item's active state set without drawing it, for a menu something
		   else covers: it is drawn as a whole when it is on top again. */
		typedef std::function<void(bool active)> Quietly;

		/* An empty screen allows everything. applied is what the item was built as.
		   owner is the menu the item sits in, handed back by follow(). */
		void add(const Item *item, const std::string &key, const Screen &screen, bool applied, void *owner = NULL)
		{
			State s;
			s.key = key;
			s.screen = screen;
			s.applied = applied;
			s.owner = owner;
			s.deferred = false;
			s.pending = false;
			known[item] = s;
		}

		/* How an item is deferred: hold makes its widget edit a HeldValue, and commit
		   puts that into the setting when the item is settled. */
		typedef std::function<void()> Hold;
		typedef std::function<void()> Commit;
		void deferWith(const Item *item, const Hold &hold, const Commit &commit)
		{
			typename std::map<const Item *, State>::iterator it = known.find(item);
			if (it == known.end())
				return;
			it->second.hold = hold;
			it->second.commit = commit;
		}

		/* An item whose change takes effect once, when its screen settles it, and
		   not on every step of the chooser: a question after each step would ask
		   about every value passed on the way. Until then no step reaches the
		   setting, so nothing that reads it meanwhile, another key of the same
		   group or a save, takes up a value that nobody has confirmed. */
		void deferApply(const Item *item)
		{
			typename std::map<const Item *, State>::iterator it = known.find(item);
			if (it == known.end())
				return;
			it->second.deferred = true;
			if (it->second.hold)
				it->second.hold();
		}

		// True for a deferred item, whose change is then kept for takePending().
		bool holdChange(const Item *item)
		{
			typename std::map<const Item *, State>::iterator it = known.find(item);
			if (it == known.end() || !it->second.deferred)
				return false;
			it->second.pending = true;
			return true;
		}

		/* The key of a deferred item changed since the last call, once, with the
		   change put into the setting. */
		bool takePending(const Item *item, std::string &key)
		{
			typename std::map<const Item *, State>::iterator it = known.find(item);
			if (it == known.end() || !it->second.pending)
				return false;
			it->second.pending = false;
			if (it->second.commit)
				it->second.commit();
			key = it->second.key;
			return true;
		}

		/* Writes to these keys, besides the item's own, make it take its value again: an
		   item the screen builds for a display of another setting. */
		void rereadOnAlso(const Item *item, const std::vector<std::string> &keys)
		{
			typename std::map<const Item *, State>::iterator it = known.find(item);
			if (it != known.end())
				it->second.also = keys;
		}

		// For an item that is not known nothing is kept.
		void rereadWith(const Item *item, const Reread &reread, const Quietly &quietly)
		{
			typename std::map<const Item *, State>::iterator it = known.find(item);
			if (it == known.end())
				return;
			it->second.reread = reread;
			it->second.quietly = quietly;
		}

		/* After settings were written elsewhere: every item whose key is among
		   written takes its value again, drawn only where its menu is onTop, the
		   one waiting for a key; NULL is none. Answers each owner of a known item
		   once, for the caller to judge that menu's items again, since a written
		   value moves the state of rows that were not written. Nothing is applied:
		   the write that changed the values has applied them. */
		std::vector<void *> follow(const std::set<std::string> &written, const void *onTop)
		{
			std::vector<void *> owners;
			for (typename std::map<const Item *, State>::iterator it = known.begin(); it != known.end(); ++it)
			{
				bool moved = written.count(it->second.key) != 0;
				for (size_t k = 0; !moved && k < it->second.also.size(); k++)
					moved = written.count(it->second.also[k]) != 0;
				if (moved && it->second.reread)
					it->second.reread(onTop != NULL && it->second.owner == onTop);
				void *o = it->second.owner;
				bool seen = o == NULL;
				for (size_t i = 0; !seen && i < owners.size(); i++)
					seen = owners[i] == o;
				if (!seen)
					owners.push_back(o);
			}
			return owners;
		}

		void forget(const Item *item) { known.erase(item); }

		// For an item that is not known nothing is kept, as for a separator.
		void afterApply(const Item *item, const After &after)
		{
			typename std::map<const Item *, State>::iterator it = known.find(item);
			if (it != known.end())
				it->second.after = after;
		}

		bool runAfter(const Item *item)
		{
			typename std::map<const Item *, State>::iterator it = known.find(item);
			if (it == known.end() || !it->second.after)
				return false;
			return it->second.after();
		}

		/* quiet sets the states of a menu something else covers without drawing
		   them, through the item's Quietly where it has one. */
		void reevaluate(const std::vector<Item *> &items, Holds holds, bool quiet = false)
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
				if (quiet && it->second.quietly)
					it->second.quietly(want);
				else
					items[i]->setActive(want);
			}
		}

	private:
		struct State
		{
			std::string key;
			std::vector<std::string> also;
			Screen screen;
			bool applied;
			After after;
			Reread reread;
			Quietly quietly;
			void *owner;
			bool deferred;
			bool pending;
			Hold hold;
			Commit commit;
		};
		std::map<const Item *, State> known;
};

/* The int a deferred item's widget edits instead of the setting (deferApply), and
   puts into it once the item is settled. Not copied: the widget points into it. */
class HeldValue
{
	public:
		HeldValue() : setting(NULL), copy(0) {}
		HeldValue(const HeldValue &) = delete;
		HeldValue &operator=(const HeldValue &) = delete;

		// From now on the widget edits what this answers, filled from the setting.
		int *hold(int *edited)
		{
			setting = edited;
			copy = *edited;
			return &copy;
		}
		bool holding() const { return setting != NULL; }
		// A write made elsewhere is shown, over a step that is not settled yet.
		void follow()
		{
			if (setting != NULL)
				copy = *setting;
		}
		void commit()
		{
			if (setting != NULL)
				*setting = copy;
		}

	private:
		int *setting;
		int copy;
};

/* What follows a change of one row, for every shape of row alike: the apply
   registry hears of the key once, then what the screen does after it for the
   changed item, then the items of the menu are judged again, so a value the
   screen put back is the one they are judged on. A menu that is not known skips
   the last step, and a changed item that is NULL or unknown the middle one.
   Answers whether the screen asked for a repaint. */
template <class Item, class Apply>
bool settleChange(SettingActiveSet<Item> &set, const std::string &key, const Item *changed, Apply apply,
		  std::vector<Item *> *items, typename SettingActiveSet<Item>::Holds holds)
{
	apply(key);
	const bool repaint = changed != NULL && set.runAfter(changed);
	if (items != NULL)
		set.reevaluate(*items, holds);
	return repaint;
}

/* Settings written elsewhere have landed and been applied: every item of their
   keys takes the value, and every menu holding a known item is judged again,
   drawn only where it is onTop, the menu waiting for a key. itemsOf answers a
   menu's items from the owner follow() hands back. */
template <class Item, class ItemsOf>
void followWrites(SettingActiveSet<Item> &set, const std::vector<std::string> &keys, const void *onTop,
		  ItemsOf itemsOf, typename SettingActiveSet<Item>::Holds holds)
{
	const std::set<std::string> written(keys.begin(), keys.end());
	const std::vector<void *> owners = set.follow(written, onTop);
	for (size_t i = 0; i < owners.size(); i++)
		set.reevaluate(itemsOf(owners[i]), holds, owners[i] != onTop);
}

/* A change the screen asks to keep once it has taken effect, for a setting that
   can leave the screen unreadable, such as the video mode. Nothing is asked
   while value is the one kept. Yes makes value the one kept; anything else puts
   kept back into value and applies the key again, so the box returns to it.
   Answers whether value was put back. */
template <class Ask, class Apply>
bool keepOrRestore(int &value, int &kept, const std::string &key, Ask ask, Apply apply)
{
	if (value == kept)
		return false;
	if (ask())
	{
		kept = value;
		return false;
	}
	value = kept;
	apply(key);
	return true;
}

/* A setting the box's menu asks about after a change, for replaceAsking(): ask
   is handed the value it had before. */
struct AskedSetting
{
	int *value;
	std::string key;
	std::function<bool(int before)> ask;
};

/* Settings put over wholesale from the box's menu, a loaded file or the defaults:
   replace puts every change in force without asking, then each asked setting that
   moved goes through keepOrRestore() with the value it had before, so a mode or
   a remote control nobody can answer with goes back as it does from its menu. */
template <class Replace, class Apply>
void replaceAsking(const std::vector<AskedSetting> &asked, Replace replace, Apply apply)
{
	std::vector<int> before;
	for (size_t i = 0; i < asked.size(); i++)
		before.push_back(*asked[i].value);
	replace();
	for (size_t i = 0; i < asked.size(); i++)
	{
		const AskedSetting &a = asked[i];
		const int was = before[i];
		int kept = was;
		keepOrRestore(*a.value, kept, a.key, [&a, was]() { return a.ask(was); }, apply);
	}
}

#endif

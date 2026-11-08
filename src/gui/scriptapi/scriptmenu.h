/*
 * script menu
 *
 * The menu a script fills, in the language the scripts are written in: neither
 * this nor anything it calls knows which interpreter asked for it. A binding
 * reads its own item - a table or a dict - fills a ScriptMenu::params and an
 * item, and hands them over; what happens from there on is one implementation,
 * so that two interpreters cannot come to different behaviour.
 *
 * It is the same layer as the window a script draws in (scriptwindow.h) and it
 * sits in the same place: below the bindings and above the components, with the
 * coreapi deliberately not involved.
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

#ifndef _SCRIPTMENU_H
#define _SCRIPTMENU_H

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <gui/components/cc.h>
#include <gui/widget/menue.h>

/* One item a script added to its menu. The type is the word the script used
 * for it and the id is the one it answers to; the rest is what the row it
 * becomes is built from. */
struct ScriptMenuItem
{
	int					int_val;	/* the value a chooser carries */
	std::string				str_val;
	std::string				name;
	std::string				type;
	bool					masked;		/* a keyboardinput never shows it */
	CMenuItem				*mi;
	/* the options of a chooser: the value and the text of each */
	std::vector<std::pair<int, std::string> >	options;
};

/* The menu a script fills: the widget it draws in, the items it has added and
 * what has to be let go of when it goes.
 *
 * What a row hands on when it is picked is deliberately not here: it is the one
 * thing about a menu that cannot be said without knowing the interpreter, so
 * the binding keeps it and this holds everything else. */
class ScriptMenu
{
	public:
		/* What a script asked for when it made the menu. A binding fills
		 * this from its own table, so the field names are the ones the
		 * bindings' parameters are. */
		typedef struct params {
			std::string			name;
			std::string			icon;
			bool				width_given;
			int				width;
			std::vector<button_label_cc>	buttons;
		} params_t;

		explicit ScriptMenu(const params_t &pars);
		virtual ~ScriptMenu();

		/* The item a script adds: the next one to be filled in. */
		ScriptMenuItem	*add(void);
		/* And the one an id names, or NULL where the script named none. */
		ScriptMenuItem	*item(int id);
		/* The id the next item is given, as both bindings number them. */
		int		nextId(void) { return (int)itemmap.size() + 1; }
		/* The item an id is given: kept until the menu goes. */
		void		keep(int id, ScriptMenuItem *item);

		/* What the row of a masked entry shows: whether a value is set,
		 * never the value itself. */
		static std::string	maskedText(const std::string &value);

		/* The line a chooser that named no options leaves behind. */
		void		optionError(void);

		/* A name a script wrote, held until the menu goes: a row keeps the
		 * pointer it is given, so what it points at has to outlive the call
		 * that named it. */
		char	*keepName(const std::string &text);

		/* The item types that are the menu's own and name nothing of the
		 * script: the back, the next, the cancel and the separators.
		 * Answers whether the type was one of them. */
		bool	addIfPlain(const std::string &type, const std::string &name);

		/* The choices of a chooser, as the widget wants them: the list the
		 * binding read out of its own table or dict, and the value the
		 * script named marked in it. Held until the menu goes, and its
		 * length is answered in count. */
		CMenuOptionChooser::keyval_ext *chooserKeys(ScriptMenuItem *item,
				const std::string &value, int &count);

		/* The row an item became: what it hints at, the menu it goes into
		 * and the id a script knows it by. Answers that id, or 0 where the
		 * item could not be made into a row. */
		int	addRow(ScriptMenuItem *item, CMenuItem *mi,
				const std::string &hint, const char *hint_icon);

		/* The names a script wrote and the rows it made, kept until the
		 * menu goes: a row holds the one and the other. */
		std::list<void *>	tofree;

		/* The widget a script fills: what a binding has to call of its own
		 * on it is a call on this. */
		CMenuWidget		*m;

	private:
		std::list<ScriptMenuItem>	items;
		std::map<int, ScriptMenuItem *>	itemmap;
};

#endif /* _SCRIPTMENU_H */

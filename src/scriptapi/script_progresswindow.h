/*
 * script progress window
 *
 * The window a script shows how far something has come in, in the language the
 * scripts are written in: neither this nor anything it calls knows which
 * interpreter asked for it. A binding reads its own table, fills a
 * ScriptProgressWindow::params and hands it over.
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

#ifndef _SCRIPTPROGRESSWINDOW_H
#define _SCRIPTPROGRESSWINDOW_H

#include <string>

#include <gui/widget/progresswindow.h>

/* The window a script shows a progress in, with a title and the status bar of
 * its own or the one of the program. */
class ScriptProgressWindow
{
	public:
		/* What a script asked for when it made the window. A binding fills
		 * this from its own table, so the field names are the ones the
		 * bindings' parameters are. */
		typedef struct params {
			std::string	name;
		} params_t;

		explicit ScriptProgressWindow(const params_t &pars);
		virtual ~ScriptProgressWindow();

		/* the window itself, for a binding that has a call of its own on it */
		CProgressWindow	*w;

		void	paint(bool do_save_bg);
		void	hide(void);
		void	setTitle(const std::string &name);
		/* the status bar of the window itself, or the one of the program */
		void	showStatus(bool local, unsigned int prog, unsigned int max,
				   const std::string &statusText);
};

#endif /* _SCRIPTPROGRESSWINDOW_H */

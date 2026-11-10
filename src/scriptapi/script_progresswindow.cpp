/*
 * script progress window
 *
 * See script_progresswindow.h for what this is for.
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

#include "script_progresswindow.h"

ScriptProgressWindow::ScriptProgressWindow(const params_t &pars)
{
	/* The title is set after the making of the window, as both bindings had
	 * it: a window that is made with a title of its own is a constructor
	 * call here, once the branch that brings it is in the master. */
	w = new CProgressWindow();
	w->setTitle(pars.name);
}

ScriptProgressWindow::~ScriptProgressWindow()
{
	delete w;
}

void ScriptProgressWindow::paint(bool do_save_bg)
{
	w->paint(do_save_bg);
}

void ScriptProgressWindow::hide(void)
{
	w->hide();
}

void ScriptProgressWindow::setTitle(const std::string &name)
{
	w->setTitle(name);
}

/* What a script tells the window how far something has come: the status bar of
 * the window itself, or the one the program shows for it. */
void ScriptProgressWindow::showStatus(bool local, unsigned int prog, unsigned int max,
		const std::string &statusText)
{
	if (local)
		w->showLocalStatus(prog, max, statusText);
	else
		w->showGlobalStatus(prog, max, statusText);
}

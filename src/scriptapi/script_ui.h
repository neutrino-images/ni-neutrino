/*
 * script ui
 *
 * Who is told when a script opens a dialog of its own. Both bindings report
 * here, so that a caller hears of a script in either language.
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

#ifndef _SCRIPTUI_H
#define _SCRIPTUI_H

/* A script may open its own OSD (menu, messagebox, hintbox) while the caller
 * that runs it still shows a hint of its own, such as the "reading data" box
 * of a WebTV resolve. The caller registers a listener on its own thread; the
 * bindings report the outermost open/close transition (nested dialogs are
 * counted), and the callback runs only on the registering thread, so a script
 * started elsewhere in the meantime cannot touch the caller's objects. */
class ScriptUi
{
	public:
		/* Registers the listener of the calling thread; NULL takes it off. */
		static void setListener(void (*cb)(bool open, void *data), void *data);

		/* A dialog of a script opens, and closes again. */
		static void open();
		static void close();
};

#endif /* _SCRIPTUI_H */

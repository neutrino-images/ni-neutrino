/*
 * script ui
 *
 * The listener both bindings kept a copy of each: script_ui.h says what it
 * is for.
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

#include "script_ui.h"

#include <cstddef>
#include <pthread.h>

static void (*script_ui_cb)(bool, void *) = NULL;
static void *script_ui_cb_data = NULL;
static pthread_t script_ui_thread;
static int script_ui_depth = 0;

void ScriptUi::setListener(void (*cb)(bool open, void *data), void *data)
{
	script_ui_cb = cb;
	script_ui_cb_data = data;
	script_ui_thread = pthread_self();
	script_ui_depth = 0;
}

void ScriptUi::open()
{
	if (!script_ui_cb || !pthread_equal(pthread_self(), script_ui_thread))
		return;
	if (script_ui_depth++ == 0)
		script_ui_cb(true, script_ui_cb_data);
}

void ScriptUi::close()
{
	if (!script_ui_cb || !pthread_equal(pthread_self(), script_ui_thread))
		return;
	if (script_ui_depth > 0 && --script_ui_depth == 0)
		script_ui_cb(false, script_ui_cb_data);
}

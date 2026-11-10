/*
 * python instance helper functions
 *
 * The bindings in this directory answer a Python script where the ones in
 * src/gui/lua answer a Lua script, and they are read the same way: what the
 * Lua bindings look up in the table a call was handed, these look up in the
 * keyword arguments of the call, so a key left out and a keyword left out
 * read the same. The notices a script gets for writing something obsolete
 * are the ones the Lua bindings print, out of the same locale strings.
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

#ifndef _PYINSTANCEHELPERS_H
#define _PYINSTANCEHELPERS_H

/* Python.h has to be read before the standard headers: it sets the feature
 * macros those are then read under. */
#include <Python.h>

#include <string>

#define PY_DEBUG printf

/* The name a script sees its own module under, the counterpart of the
 * LUA_CLASSNAME the Lua bindings register under. */
#define PY_CLASSNAME        "neutrino"
#define PY_VIDEO_CLASSNAME  "video"
#define PY_MISC_CLASSNAME   "misc"
#define PY_CURL_CLASSNAME   "curl"
#define PY_HEADER_CLASSNAME "header"
#define PY_FILEHELPER_CLASSNAME "filehelpers"

#define PY_WIKI "https://wiki.tuxbox-neutrino.org/wiki"

/* the magic color that tells us we are using one of the palette colors */
/* The colour a script names as one of the box's own is resolved in the script
 * API, where the constant it carries is: see scriptcolor.h. */

/* The statuses the curl binding answers with, the ones the Lua bindings
 * answer with. */
#define PY_CURL_OK              0
#define PY_CURL_ERR_HANDLE      1
#define PY_CURL_ERR_NO_URL      2
#define PY_CURL_ERR_CREATE_FILE 3
#define PY_CURL_ERR_CURL        4

/* A method of a binding, taking the arguments of the call and its keywords
 * the way the Lua bindings take a table and a list of arguments. */
#define PY_METHODDEF(name, func) \
	{ name, (PyCFunction)(void (*)(void))(func), METH_VARARGS | METH_KEYWORDS, NULL }

/* The **kwargs of a call, as the Lua bindings see the table of a call. A
 * call that was given none is handed as NULL, which every lookup answers
 * with "not there". */
/* A binding object that stands for an OSD container hands its form on
 * through this attribute, so that another binding can take the object as
 * its parent, the way a Lua binding hands on its userdata. */
#define PY_FORM_ATTRIBUTE "_cc_form"
#define PY_FORM_CAPSULE   "neutrino.cc_form"

bool pyHaveKeywords(PyObject *kw);

bool pyLookupString(PyObject *kw, const char *what, std::string &value);
bool pyLookupInt(PyObject *kw, const char *what, long &value);
bool pyLookupUnsigned(PyObject *kw, const char *what, unsigned long &value);

/* Reads one value as a number, the way a positional argument of a call is
 * read, which is what the Lua bindings' luaL_checkint does. */
bool pyArgLong(PyObject *item, long &value);
bool pyArgUnsigned(PyObject *item, unsigned long &value);

/* The same for a text. */
bool pyArgString(PyObject *item, std::string &value);
bool pyLookupBool(PyObject *kw, const char *what, bool &value);
bool pyLookupObject(PyObject *kw, const char *what, PyObject **value);
bool pyLookupList(PyObject *kw, const char *what, PyObject **value);

/* The same, for a key that used to be written as a string ("true", "1",
 * "yes"), which the Lua bindings still take and report. */
bool pyLookupBoolCompat(PyObject *kw, const char *what, bool &value);

void pyParamBoolDeprecated(const char *val);
void pyParamDeprecated(const char *oldParam, const char *newParam);
void pyFunctionDeprecated(const char *oldFunc, const char *newFunc);
void pyObsoleteHideParameter(void);


class CComponentsForm;

/* The OSD form a binding object stands for, handed on so that another
 * binding can take the object as its parent, where a Lua binding hands on
 * its userdata. NULL where the object stands for none. */
CComponentsForm *pyLookupParentForm(PyObject *kw, const char *what);

/* Where the running script stands, "file:line", as the Lua bindings name
 * it for a notice. Answers "" when there is nothing to name. */
std::string pyScriptWhere(void);

#endif // _PYINSTANCEHELPERS_H

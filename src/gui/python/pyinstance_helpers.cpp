/*
 * python instance helper functions
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

#include "pyinstance_helpers.h"

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <global.h>
#include <system/debug.h>
#include <gui/components/cc.h>
#include <neutrino.h>
#include <gui/scriptapi/scriptcolor.h>

bool pyHaveKeywords(PyObject *kw)
{
	return (kw != NULL) && PyDict_Check(kw);
}

/* Borrowed, as the Lua bindings read a field: the item is looked at and
 * used before the call that was handed it returns. */
static PyObject *pyLookupKey(PyObject *kw, const char *what)
{
	if (!pyHaveKeywords(kw))
		return NULL;
	return PyDict_GetItemString(kw, what);
}

static bool pyToLong(PyObject *item, long &value)
{
	PyObject *num = PyNumber_Long(item);
	if (num == NULL) {
		PyErr_Clear();
		return false;
	}
	long v = PyLong_AsLong(num);
	Py_DECREF(num);
	if ((v == -1) && PyErr_Occurred()) {
		PyErr_Clear();
		return false;
	}
	value = v;
	return true;
}


bool pyLookupString(PyObject *kw, const char *what, std::string &value)
{
	PyObject *item = pyLookupKey(kw, what);
	if (item == NULL)
		return false;

	if (PyUnicode_Check(item)) {
		Py_ssize_t len = 0;
		const char *s = PyUnicode_AsUTF8AndSize(item, &len);
		if (s == NULL) {
			PyErr_Clear();
			return false;
		}
		value.assign(s, (size_t)len);
		return true;
	}

	if (PyBytes_Check(item)) {
		char *s = NULL;
		Py_ssize_t len = 0;
		if (PyBytes_AsStringAndSize(item, &s, &len) < 0) {
			PyErr_Clear();
			return false;
		}
		value.assign(s, (size_t)len);
		return true;
	}

	/* A number or a boolean written where a text is read is the text of
	 * itself, the way a Lua script may put one in a field a binding reads
	 * as a string. */
	if (PyLong_Check(item) || PyFloat_Check(item) || PyBool_Check(item)) {
		PyObject *str = PyObject_Str(item);
		if (str == NULL) {
			PyErr_Clear();
			return false;
		}
		const char *s = PyUnicode_AsUTF8(str);
		if (s == NULL) {
			PyErr_Clear();
			Py_DECREF(str);
			return false;
		}
		value = s;
		Py_DECREF(str);
		return true;
	}

	return false;
}

bool pyLookupInt(PyObject *kw, const char *what, long &value)
{
	PyObject *item = pyLookupKey(kw, what);
	if (item == NULL)
		return false;
	return pyToLong(item, value);
}

bool pyArgLong(PyObject *item, long &value)
{
	if (item == NULL)
		return false;
	return pyToLong(item, value);
}

/* A colour, a mask and the like are unsigned where a coordinate is not, so a
 * value above LONG_MAX is one of those rather than an error: the Lua
 * bindings read them with luaL_checkunsigned. */
bool pyArgUnsigned(PyObject *item, unsigned long &value)
{
	if (item == NULL)
		return false;
	unsigned long v = PyLong_AsUnsignedLong(item);
	if (v == (unsigned long)-1 && PyErr_Occurred()) {
		PyErr_Clear();
		return false;
	}
	value = v;
	return true;
}

bool pyArgString(PyObject *item, std::string &value)
{
	if (item == NULL)
		return false;
	if (!PyUnicode_Check(item))
		return false;
	Py_ssize_t len = 0;
	const char *s = PyUnicode_AsUTF8AndSize(item, &len);
	if (s == NULL) {
		PyErr_Clear();
		return false;
	}
	value.assign(s, (size_t)len);
	return true;
}

bool pyLookupUnsigned(PyObject *kw, const char *what, unsigned long &value)
{
	PyObject *item = pyLookupKey(kw, what);
	if (item == NULL)
		return false;
	return pyArgUnsigned(item, value);
}

/* Only a boolean, as the Lua bindings read a field they are told is one.
 * A script that wrote the old strings is answered by pyLookupBoolCompat. */
bool pyLookupBool(PyObject *kw, const char *what, bool &value)
{
	PyObject *item = pyLookupKey(kw, what);
	if (item == NULL)
		return false;
	if (!PyBool_Check(item))
		return false;
	value = (item == Py_True);
	return true;
}

bool pyLookupObject(PyObject *kw, const char *what, PyObject **value)
{
	PyObject *item = pyLookupKey(kw, what);
	if (item == NULL)
		return false;
	if (item == Py_None)
		return false;
	*value = item;
	return true;
}

bool pyLookupList(PyObject *kw, const char *what, PyObject **value)
{
	PyObject *item = pyLookupKey(kw, what);
	if (item == NULL)
		return false;
	if (!PyList_Check(item) && !PyTuple_Check(item))
		return false;
	*value = item;
	return true;
}

bool pyLookupBoolCompat(PyObject *kw, const char *what, bool &value)
{
	if (pyLookupBool(kw, what, value))
		return true;

	std::string tmp;
	if (pyLookupString(kw, what, tmp)) {
		pyParamBoolDeprecated(tmp.c_str());
		value = (tmp == "true" || tmp == "1" || tmp == "yes");
		return true;
	}
	return false;
}

/* Where the running script stands, as the Lua bindings name it in a
 * notice: the file of the frame that is calling, and the line in it. */
std::string pyScriptWhere(void)
{
	PyFrameObject *frame = PyEval_GetFrame();	/* borrowed */
	if (frame == NULL)
		return "";

	int line = PyFrame_GetLineNumber(frame);

	PyCodeObject *code = PyFrame_GetCode(frame);	/* new reference */
	if (code == NULL) {
		PyErr_Clear();
		char buf[32];
		snprintf(buf, sizeof(buf), "?:%d", line);
		return buf;
	}

	std::string file = "?";
	PyObject *name = PyObject_GetAttrString((PyObject *)code, "co_filename");
	if (name != NULL) {
		const char *s = PyUnicode_AsUTF8(name);
		if (s != NULL)
			file = s;
		else
			PyErr_Clear();
		Py_DECREF(name);
	} else
		PyErr_Clear();
	Py_DECREF(code);

	char buf[32];
	snprintf(buf, sizeof(buf), ":%d", line);
	return file + buf;
}


/* A colour a script named as one of the box's own is resolved where the window
 * of a script resolves it, so that a window and a widget of a script cannot
 * come to resolve one differently. */
unsigned int pyCheckMagicMask(unsigned int col)
{
	return (unsigned int)ScriptColorResolve(col);
}

CComponentsForm *pyLookupParentForm(PyObject *kw, const char *what)
{
	PyObject *item = NULL;
	if (!pyLookupObject(kw, what, &item))
		return NULL;

	PyObject *capsule = PyObject_GetAttrString(item, PY_FORM_ATTRIBUTE);
	if (capsule == NULL)
	{
		PyErr_Clear();
		return NULL;
	}

	CComponentsForm *form = NULL;
	if (PyCapsule_CheckExact(capsule))
		form = (CComponentsForm *)PyCapsule_GetPointer(capsule, PY_FORM_CAPSULE);
	if (form == NULL)
		PyErr_Clear();
	Py_DECREF(capsule);
	return form;
}



/* The three notices below say what the Lua bindings say, out of the same
 * locale strings: a script being carried over from one language to the
 * other is told the same things about what to write instead. */
void pyParamBoolDeprecated(const char *val)
{
	std::string where = pyScriptWhere();
	printf("[Python Script] \33[1;31m%s\33[0m %s (\33[31m\"%s\"\33[0m)\n"
	       "                      %s \33[32mTrue\33[0m.\n"
	       "                      (%s)\n",
	       g_Locale->getText(LOCALE_LUA_BOOLPARAM_DEPRECATED1),
	       g_Locale->getText(LOCALE_LUA_BOOLPARAM_DEPRECATED2), val,
	       g_Locale->getText(LOCALE_LUA_BOOLPARAM_DEPRECATED3),
	       where.c_str());
}

void pyParamDeprecated(const char *oldParam, const char *newParam)
{
	std::string where = pyScriptWhere();
	printf("[Python Script] \33[1;31m%s\33[0m %s \33[33m%s\33[0m %s \33[1;33m%s\33[0m.\n"
	       "                      (%s)\n",
	       g_Locale->getText(LOCALE_LUA_FUNCTION_DEPRECATED1),
	       g_Locale->getText(LOCALE_LUA_PARAMETER_DEPRECATED2), oldParam,
	       g_Locale->getText(LOCALE_LUA_FUNCTION_DEPRECATED3), newParam,
	       where.c_str());
}

void pyFunctionDeprecated(const char *oldFunc, const char *newFunc)
{
	std::string where = pyScriptWhere();
	printf("[Python Script] \33[1;31m%s\33[0m %s \33[33m%s\33[0m %s \33[1;33m%s\33[0m.\n"
	       "                      (%s)\n",
	       g_Locale->getText(LOCALE_LUA_FUNCTION_DEPRECATED1),
	       g_Locale->getText(LOCALE_LUA_FUNCTION_DEPRECATED2), oldFunc,
	       g_Locale->getText(LOCALE_LUA_FUNCTION_DEPRECATED3), newFunc,
	       where.c_str());
}

void pyObsoleteHideParameter(void)
{
	std::string where = pyScriptWhere();
	printf("\33[1;31m[Python script warning]\33[0m %s: Obsolete parameter for hide() in use, please remove!\n",
	       where.c_str());
}

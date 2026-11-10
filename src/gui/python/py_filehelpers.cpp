/*
 * python file helpers functions
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

#include "py_filehelpers.h"

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <utime.h>

#include <global.h>
#include <system/debug.h>
#include <system/helpers.h>
#include <neutrino.h>
#include <gui/widget/msgbox.h>

#include "pyinstance.h"

typedef struct
{
	PyObject_HEAD
	CPyFileHelpers *d;
} PyFileHelpersObject;

static PyTypeObject *filehelpers_type = NULL;

CPythonInstFileHelpers *CPythonInstFileHelpers::getInstance()
{
	static CPythonInstFileHelpers *PythonInstFileHelpers = NULL;

	if (!PythonInstFileHelpers)
		PythonInstFileHelpers = new CPythonInstFileHelpers();
	return PythonInstFileHelpers;
}

/* What the Lua bindings print when a call of theirs failed: where the
 * script stands, what went wrong, and where in neutrino it was noticed. */
static void fileHelpersError(const char *msg, const char *file, int line)
{
	std::string where = pyScriptWhere();
	printf(">>> Python script error [%s] %s\n    (error from neutrino: [%s:%d])\n",
	       where.c_str(), msg, file, line);
}

PyObject *CPythonInstFileHelpers::FileHelpersNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	if ((args != NULL && PyTuple_GET_SIZE(args) > 0) || (kwds != NULL && PyDict_Size(kwds) > 0))
	{
		PyErr_SetString(PyExc_TypeError, "filehelpers.new() takes no arguments");
		return NULL;
	}

	PyFileHelpersObject *self = (PyFileHelpersObject *)type->tp_alloc(type, 0);
	if (self == NULL)
	{
		PyErr_Clear();
		return NULL;
	}
	self->d = new CPyFileHelpers();
	return (PyObject *)self;
}

PyObject *CPythonInstFileHelpers::FileHelpersNewModule(PyObject *self, PyObject *args, PyObject *kwds)
{
	if (filehelpers_type == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "filehelpers is not registered");
		return NULL;
	}
	return FileHelpersNew(filehelpers_type, args, kwds);
}

void CPythonInstFileHelpers::FileHelpersDelete(PyObject *self)
{
	PyFileHelpersObject *o = (PyFileHelpersObject *)self;
	delete o->d;
	o->d = NULL;
	Py_TYPE(self)->tp_free(self);
}

/* f.cp(from, to, flags="") */
PyObject *CPythonInstFileHelpers::FileHelpersCp(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "from", "to", "flags", NULL };
	const char *from = NULL;
	const char *to = NULL;
	const char *flags = "";

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ss|s", (char **)kwlist, &from, &to, &flags))
		return NULL;

	CFileHelpers fh;
	fh.setConsoleQuiet(true);
	bool ret = fh.cp(from, to, flags);
	if (!ret)
	{
		helpersDebugInfo di;
		fh.readDebugInfo(&di);
		fileHelpersError(di.msg.c_str(), di.file.c_str(), di.line);
	}

	return PyBool_FromLong(ret ? 1 : 0);
}

/* the layer this file now reads: scriptapi/script_filehelpers.h */
#include "../scriptapi/script_filehelpers.h"

/* f.chmod(file, mode): the mode is the number a script writes, read as the
 * octal digits it looks like; that rule stands in the script API, where the
 * Lua binding reads it the same way. */
PyObject *CPythonInstFileHelpers::FileHelpersChmod(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "file", "mode", NULL };
	const char *file = NULL;
	int mode_i = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "si", (char **)kwlist, &file, &mode_i))
		return NULL;

	std::string error = "";
	bool ret = ScriptFileHelpers::Chmod(file, mode_i, error);
	if (!ret)
		fileHelpersError(error.c_str(), __path_file__, __LINE__);

	return PyBool_FromLong(ret ? 1 : 0);
}

/* f.touch(file, time=now) */
PyObject *CPythonInstFileHelpers::FileHelpersTouch(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "file", "time", NULL };
	const char *file = NULL;
	int newTime = 0;
	bool haveTime = false;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|i", (char **)kwlist, &file, &newTime))
		return NULL;
	if (kwds != NULL && PyDict_GetItemString(kwds, "time") != NULL)
		haveTime = true;
	if (args != NULL && PyTuple_GET_SIZE(args) > 1)
		haveTime = true;

	if (!file_exists(file))
	{
		FILE *f = fopen(file, "w");
		if (f == NULL)
		{
			fileHelpersError(strerror(errno), __path_file__, __LINE__);
			return PyBool_FromLong(0);
		}
		fclose(f);
		if (!haveTime)
			return PyBool_FromLong(1);
	}

	time_t modTime = haveTime ? (time_t)newTime : time(NULL);

	utimbuf utb;
	utb.actime = modTime;
	utb.modtime = modTime;
	bool ret = true;
	if (utime(file, &utb) != 0)
	{
		ret = false;
		fileHelpersError(strerror(errno), __path_file__, __LINE__);
	}

	return PyBool_FromLong(ret ? 1 : 0);
}

/* f.rmdir(dir) */
PyObject *CPythonInstFileHelpers::FileHelpersRmdir(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "dir", NULL };
	const char *dir = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &dir))
		return NULL;

	CFileHelpers *fh = CFileHelpers::getInstance();
	fh->setConsoleQuiet(true);
	bool ret = fh->removeDir(dir);
	if (!ret)
	{
		helpersDebugInfo di;
		fh->readDebugInfo(&di);
		fileHelpersError(di.msg.c_str(), di.file.c_str(), di.line);
	}

	return PyBool_FromLong(ret ? 1 : 0);
}

/* f.mkdir(dir, mode=0755) */
PyObject *CPythonInstFileHelpers::FileHelpersMkdir(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "dir", "mode", NULL };
	const char *dir = NULL;
	int mode_i = 0;
	bool haveMode = false;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|i", (char **)kwlist, &dir, &mode_i))
		return NULL;
	if (kwds != NULL && PyDict_GetItemString(kwds, "mode") != NULL)
		haveMode = true;
	if (args != NULL && PyTuple_GET_SIZE(args) > 1)
		haveMode = true;

	mode_t mode = 0755;
	if (haveMode)
	{
		std::string mode_s = itoa(mode_i, 10);
		mode = (mode_t)(strtol(mode_s.c_str(), (char **)NULL, 8) & 0x0FFF);
	}

	CFileHelpers *fh = CFileHelpers::getInstance();
	fh->setConsoleQuiet(true);
	bool ret = fh->createDir(dir, mode);
	if (!ret)
	{
		helpersDebugInfo di;
		fh->readDebugInfo(&di);
		fileHelpersError(di.msg.c_str(), di.file.c_str(), di.line);
	}

	return PyBool_FromLong(ret ? 1 : 0);
}

/* f.readlink(link): what the link points at, or None where it cannot be
 * read, which is what the Lua binding answers. */
PyObject *CPythonInstFileHelpers::FileHelpersReadlink(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "link", NULL };
	const char *link = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &link))
		return NULL;

	char buf[PATH_MAX];
	memset(buf, '\0', sizeof(buf));
	if (readlink(link, buf, sizeof(buf) - 1) == -1)
	{
		fileHelpersError(strerror(errno), __path_file__, __LINE__);
		Py_RETURN_NONE;
	}

	return PyUnicode_FromString(buf);
}

/* f.ln(src, link, flags="s"): a symlink, and with 'f' over an existing one */
PyObject *CPythonInstFileHelpers::FileHelpersLn(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "src", "link", "flags", NULL };
	const char *src = NULL;
	const char *link = NULL;
	const char *flags = "";

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ss|s", (char **)kwlist, &src, &link, &flags))
		return NULL;

	bool symlnk = (strchr(flags, 's') != NULL);
	bool force = (strchr(flags, 'f') != NULL);

	if (!symlnk)
	{
		fileHelpersError("Currently only supports symlinks.", __path_file__, __LINE__);
		return PyBool_FromLong(0);
	}

	if (symlink(src, link) != 0)
	{
		if (force && (errno == EEXIST))
		{
			if (unlink(link) == 0)
			{
				if (symlink(src, link) == 0)
					return PyBool_FromLong(1);
			}
		}
		fileHelpersError(strerror(errno), __path_file__, __LINE__);
		return PyBool_FromLong(0);
	}

	return PyBool_FromLong(1);
}


/* f.exist(file, flags): 'f' a regular file, 'l' a symlink, 'd' a directory.
 * None where the flags are unknown, as the Lua binding answers. */
PyObject *CPythonInstFileHelpers::FileHelpersExist(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "file", "flags", NULL };
	const char *file = NULL;
	const char *flags = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ss", (char **)kwlist, &file, &flags))
		return NULL;

	bool ret = false;
	bool err = false;
	std::string errMsg = "";

	if (file_exists(file))
	{
		struct stat FileInfo;
		if (lstat(file, &FileInfo) == -1)
		{
			err = true;
			errMsg = (std::string)strerror(errno);
		}
		else if (strchr(flags, 'f') != NULL)
		{
			if (S_ISREG(FileInfo.st_mode))
				ret = true;
		}
		else if (strchr(flags, 'l') != NULL)
		{
			if (S_ISLNK(FileInfo.st_mode))
				ret = true;
		}
		else if (strchr(flags, 'd') != NULL)
		{
			if (S_ISDIR(FileInfo.st_mode))
				ret = true;
		}
		else
		{
			err = true;
			errMsg = (strlen(flags) == 0) ? "no" : "unknown";
			errMsg += " flag given.";
		}
	}

	if (err)
	{
		fileHelpersError(errMsg.c_str(), __path_file__, __LINE__);
		Py_RETURN_NONE;
	}

	return PyBool_FromLong(ret ? 1 : 0);
}

/* f.empty(file): whether a regular file holds nothing, following a symlink.
 * None where the name is not one, as the Lua binding answers. */
PyObject *CPythonInstFileHelpers::FileHelpersIsEmpty(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "file", NULL };
	const char *file = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &file))
		return NULL;

	bool ret = false;
	bool err = false;
	std::string errMsg = "";

	if (file_exists(file))
	{
		struct stat FileInfo;
		if (lstat(file, &FileInfo) == -1)
		{
			err = true;
			errMsg = (std::string)strerror(errno);
		}
		else if (S_ISLNK(FileInfo.st_mode))
		{
			printf("Following symlink: %s\n", file);
			if (stat(file, &FileInfo) == -1)
			{
				err = true;
				errMsg = "Symlink target does not exist: " + std::string(strerror(errno));
			}
			else if (S_ISREG(FileInfo.st_mode))
			{
				ret = (FileInfo.st_size == 0);
			}
			else
			{
				err = true;
				errMsg = "Target of symlink is not a regular file.";
			}
		}
		else if (S_ISREG(FileInfo.st_mode))
		{
			ret = (FileInfo.st_size == 0);
		}
		else
		{
			err = true;
			errMsg = std::string(file) + " is not a regular file.";
		}
	}
	else
	{
		err = true;
		errMsg = std::string(file) + " does not exist.";
	}

	if (err)
	{
		fileHelpersError(errMsg.c_str(), __path_file__, __LINE__);
		DisplayErrorMessage(errMsg.c_str(), "Python Script Error:");
		Py_RETURN_NONE;
	}

	return PyBool_FromLong(ret ? 1 : 0);
}

bool CPythonInstFileHelpers::FileHelpersRegister(PyObject *module)
{
	static PyMethodDef filehelpers_methods[] =
	{
		PY_METHODDEF("cp",       CPythonInstFileHelpers::FileHelpersCp),
		PY_METHODDEF("chmod",    CPythonInstFileHelpers::FileHelpersChmod),
		PY_METHODDEF("touch",    CPythonInstFileHelpers::FileHelpersTouch),
		PY_METHODDEF("rmdir",    CPythonInstFileHelpers::FileHelpersRmdir),
		PY_METHODDEF("mkdir",    CPythonInstFileHelpers::FileHelpersMkdir),
		PY_METHODDEF("readlink", CPythonInstFileHelpers::FileHelpersReadlink),
		PY_METHODDEF("ln",       CPythonInstFileHelpers::FileHelpersLn),
		PY_METHODDEF("exist",    CPythonInstFileHelpers::FileHelpersExist),
		PY_METHODDEF("empty",    CPythonInstFileHelpers::FileHelpersIsEmpty),
		{ NULL, NULL, 0, NULL }
	};

	static PyMethodDef module_methods[] =
	{
		PY_METHODDEF("new", CPythonInstFileHelpers::FileHelpersNewModule),
		{ NULL, NULL, 0, NULL }
	};

	static PyType_Slot filehelpers_slots[] =
	{
		{ Py_tp_new, (void *)CPythonInstFileHelpers::FileHelpersNew },
		{ Py_tp_dealloc, (void *)CPythonInstFileHelpers::FileHelpersDelete },
		{ Py_tp_methods, (void *)filehelpers_methods },
		{ Py_tp_alloc, (void *)PyType_GenericAlloc },
		{ Py_tp_free, (void *)PyObject_Free },
		{ Py_tp_doc, (void *)"file helpers" },
		{ 0, NULL }
	};

	static PyType_Spec filehelpers_spec =
	{
		"neutrino.filehelpers.filehelpers",
		sizeof(PyFileHelpersObject),
		0,
		Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
		filehelpers_slots
	};

	static PyModuleDef module_def =
	{
		PyModuleDef_HEAD_INIT,
		"neutrino.filehelpers",
		"file helpers",
		-1,
		module_methods,
		NULL, NULL, NULL, NULL
	};

	PyObject *sub = PyModule_Create(&module_def);
	if (sub == NULL)
	{
		PyErr_Clear();
		return false;
	}

	filehelpers_type = (PyTypeObject *)PyType_FromSpec(&filehelpers_spec);
	if (filehelpers_type == NULL)
	{
		PyErr_Clear();
		Py_DECREF(sub);
		return false;
	}
	if (PyModule_AddObject(sub, "filehelpers", (PyObject *)filehelpers_type) < 0)
	{
		PyErr_Clear();
		Py_DECREF((PyObject *)filehelpers_type);
		filehelpers_type = NULL;
		Py_DECREF(sub);
		return false;
	}

	return PythonInstAddSubmodule(module, "filehelpers", sub);
}


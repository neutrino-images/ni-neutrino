/*
 * python threads
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

#include "py_threads.h"

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

#include <global.h>
#include <system/debug.h>
#include <system/set_threadname.h>
#include <gui/widget/msgbox.h>
#include <neutrino.h>

#include "pyinstance.h"

typedef struct
{
	PyObject_HEAD
	CPyThread *d;
} PyThreadObject;

static PyTypeObject *thread_type = NULL;

CPythonInstThreads *CPythonInstThreads::getInstance()
{
	static CPythonInstThreads *PythonInstThreads = NULL;

	if (!PythonInstThreads)
		PythonInstThreads = new CPythonInstThreads();
	return PythonInstThreads;
}

CPyThread::CPyThread()
{
	code = NULL;
	source = NULL;
	args = NULL;
	logger = NULL;
	result = NULL;
	started = false;
	detached = false;
	joined = false;
	joinable = false;
	alive = false;
	thread_ident = 0;
}

CPyThread::~CPyThread()
{
	Py_XDECREF(code);
	Py_XDECREF(source);
	Py_XDECREF(args);
	Py_XDECREF(logger);
	Py_XDECREF(result);
}

/* What the thread says, told to the script's logger where it left one, and
 * to the log where it did not. */
void CPyThread::log(const char *hdr, const std::string &msg)
{
	fprintf(stderr, "[%s] %s\n", hdr, msg.c_str());

	if (logger == NULL || logger == Py_None)
		return;

	PyGILState_STATE gstate = PyGILState_Ensure();
	PyObject *res = PyObject_CallFunction(logger, "ss", hdr, msg.c_str());
	if (res == NULL)
		PyErr_Clear();
	else
		Py_DECREF(res);
	PyGILState_Release(gstate);
}

static CPyThread *threadOf(PyObject *self)
{
	PyThreadObject *o = (PyThreadObject *)self;
	return o->d;
}

/* threads.new(code, *args, logger=None) */
PyObject *CPythonInstThreads::ThreadNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	Py_ssize_t numargs = (args != NULL) ? PyTuple_GET_SIZE(args) : 0;
	if (numargs < 1)
	{
		PyErr_SetString(PyExc_TypeError, "threads.new() takes the code the thread runs");
		return NULL;
	}

	std::string source = "";
	if (!pyArgString(PyTuple_GET_ITEM(args, 0), source))
	{
		PyErr_SetString(PyExc_TypeError, "threads.new() takes the code as text");
		return NULL;
	}

	/* the rest of the arguments are the thread's own */
	PyObject *thread_args = PyTuple_GetSlice(args, 1, numargs);
	if (thread_args == NULL)
	{
		PyErr_Clear();
		return NULL;
	}

	PyThreadObject *self = (PyThreadObject *)type->tp_alloc(type, 0);
	if (self == NULL)
	{
		PyErr_Clear();
		Py_DECREF(thread_args);
		return NULL;
	}

	self->d = new CPyThread();
	self->d->source = PyUnicode_FromStringAndSize(source.c_str(), (Py_ssize_t)source.length());
	self->d->args = thread_args;

	PyObject *logger = NULL;
	if (pyLookupObject(kwds, "logger", &logger))
	{
		Py_INCREF(logger);
		self->d->logger = logger;
	}

	return (PyObject *)self;
}

PyObject *CPythonInstThreads::ThreadNewModule(PyObject *self, PyObject *args, PyObject *kwds)
{
	if (thread_type == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "threads is not registered");
		return NULL;
	}
	return ThreadNew(thread_type, args, kwds);
}

void CPythonInstThreads::ThreadDelete(PyObject *self)
{
	PyThreadObject *o = (PyThreadObject *)self;
	if (o->d != NULL && o->d->started && !o->d->detached)
	{
		/* a thread nobody joined is waited for, so that nothing it holds is
		   let go of while it still runs */
		pthread_join(o->d->thread, NULL);
	}
	delete o->d;
	o->d = NULL;
	Py_TYPE(self)->tp_free(self);
}

/* What runs on the thread: the code in a namespace of its own, with the
 * arguments in 'argv' and what it leaves in 'result' kept for a reader. */
void *CPythonInstThreads::runThread(void *arg)
{
	CPyThread *T = (CPyThread *)arg;

	set_threadname("python thread");

	PyGILState_STATE gstate = PyGILState_Ensure();

	T->thread_ident = PyThread_get_thread_ident();
	T->alive = true;

	const char *text = (T->source != NULL) ? PyUnicode_AsUTF8(T->source) : NULL;
	if (text == NULL)
	{
		PyErr_Clear();
		T->alive = false;
		PyGILState_Release(gstate);
		return NULL;
	}

	/* a namespace of its own, as a child Lua state is one of its own */
	PyObject *globals = PyDict_New();
	if (globals != NULL)
	{
		PyObject *name = PyUnicode_FromString("__thread__");
		if (name != NULL)
		{
			if (PyDict_SetItemString(globals, "__name__", name) < 0)
				PyErr_Clear();
			Py_DECREF(name);
		}
		if (T->args != NULL)
		{
			if (PyDict_SetItemString(globals, "argv", T->args) < 0)
				PyErr_Clear();
		}
		if (PyDict_SetItemString(globals, "__builtins__", PyEval_GetBuiltins()) < 0)
			PyErr_Clear();

		PyObject *res = (T->code != NULL)
				? PyEval_EvalCode(T->code, globals, globals)
				: PyRun_StringFlags(text, Py_file_input, globals, globals, NULL);
		if (res == NULL)
		{
			PyObject *type = NULL, *val = NULL, *tb = NULL;
			PyErr_Fetch(&type, &val, &tb);
			PyErr_NormalizeException(&type, &val, &tb);

			std::string what = "NULL";
			if (val != NULL)
			{
				PyObject *str = PyObject_Str(val);
				if (str != NULL)
				{
					const char *s = PyUnicode_AsUTF8(str);
					if (s != NULL)
						what = s;
					else
						PyErr_Clear();
					Py_DECREF(str);
				}
				else
					PyErr_Clear();
			}
			T->log("python thread", what);
			Py_XDECREF(type);
			Py_XDECREF(val);
			Py_XDECREF(tb);
		}
		else
		{
			Py_DECREF(res);
			PyObject *result = PyDict_GetItemString(globals, "result");	/* borrowed */
			if (result != NULL && result != Py_None)
			{
				Py_INCREF(result);
				Py_XDECREF(T->result);
				T->result = result;
			}
		}

		Py_DECREF(globals);
	}
	else
		PyErr_Clear();

	T->alive = false;
	PyGILState_Release(gstate);
	return NULL;
}

/* t.start(detached=False, joinable=True) */
PyObject *CPythonInstThreads::ThreadStart(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyThread *T = threadOf(self);
	if (T == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "thread is gone");
		return NULL;
	}
	if (T->started)
	{
		PyErr_SetString(PyExc_RuntimeError, "thread is already started");
		return NULL;
	}

	bool start_detached = false, joinable = true;
	pyLookupBoolCompat(kwds, "detached", start_detached);
	pyLookupBoolCompat(kwds, "joinable", joinable);

	/* the code is compiled before the thread is made, so that a mistake in
	   it is answered where the script can read it */
	if (T->source != NULL)
	{
		const char *text = PyUnicode_AsUTF8(T->source);
		if (text != NULL)
		{
			T->code = Py_CompileString(text, "<thread>", Py_file_input);
			if (T->code == NULL)
			{
				PyErr_Clear();
				PyErr_SetString(PyExc_SyntaxError, "the code of the thread does not compile");
				return NULL;
			}
		}
		else
			PyErr_Clear();
	}

	T->detached = start_detached;
	T->joinable = joinable;
	T->joined = false;
	T->started = true;

	if (pthread_create(&T->thread, NULL, CPythonInstThreads::runThread, (void *)T) != 0)
	{
		T->started = false;
		PyErr_SetString(PyExc_RuntimeError, "the thread could not be started");
		return NULL;
	}
	if (start_detached)
		pthread_detach(T->thread);

	return PyBool_FromLong(1);
}

/* t.join(timeout=-1): waits for it, and answers nothing, as the Lua binding
 * does; a bounded wait leaves a thread that is still running alone. */
PyObject *CPythonInstThreads::ThreadJoin(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "timeout", NULL };
	CPyThread *T = threadOf(self);
	long timeout = -1;

	if (T == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "thread is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|i", (char **)kwlist, &timeout))
		return NULL;

	if (!T->started || T->detached)
		Py_RETURN_NONE;

	if (timeout < 0)
	{
		if (pthread_join(T->thread, NULL) == 0)
			T->joined = true;
		Py_RETURN_NONE;
	}

	/* the time is looked at in small steps, so that a thread still running
	   when it runs out is answered rather than waited for */
	struct timeval now;
	gettimeofday(&now, NULL);
	long long end_ms = (long long)now.tv_sec * 1000 + now.tv_usec / 1000 + timeout;
	while (T->alive)
	{
		gettimeofday(&now, NULL);
		long long now_ms = (long long)now.tv_sec * 1000 + now.tv_usec / 1000;
		if (now_ms >= end_ms)
			Py_RETURN_NONE;
		usleep(10000);
	}
	if (pthread_join(T->thread, NULL) == 0)
		T->joined = true;
	Py_RETURN_NONE;
}

/* t.cancel(): the thread is asked to stop, the way abortScript asks a script
 * to stop: something is raised in it rather than the thread torn down. */
PyObject *CPythonInstThreads::ThreadCancel(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyThread *T = threadOf(self);
	if (T == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "thread is gone");
		return NULL;
	}
	if (!T->started || T->thread_ident == 0)
		Py_RETURN_NONE;

	PyGILState_STATE gstate = PyGILState_Ensure();
	if (PyThreadState_SetAsyncExc(T->thread_ident, PyExc_SystemExit) == 0)
		printf("[CPythonInstThreads::%s] no thread to cancel\n", __func__);
	PyGILState_Release(gstate);

	Py_RETURN_NONE;
}

PyObject *CPythonInstThreads::ThreadDetach(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyThread *T = threadOf(self);
	if (T == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "thread is gone");
		return NULL;
	}
	if (T->started && !T->detached)
	{
		pthread_detach(T->thread);
		T->detached = true;
	}
	Py_RETURN_NONE;
}

PyObject *CPythonInstThreads::ThreadAlive(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyThread *T = threadOf(self);
	if (T == NULL)
		Py_RETURN_NONE;

	return PyBool_FromLong(T->alive ? 1 : 0);
}

PyObject *CPythonInstThreads::ThreadStarted(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyThread *T = threadOf(self);
	if (T == NULL)
		Py_RETURN_NONE;

	return PyBool_FromLong(T->started ? 1 : 0);
}

PyObject *CPythonInstThreads::ThreadDetached(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyThread *T = threadOf(self);
	if (T == NULL)
		Py_RETURN_NONE;

	return PyBool_FromLong(T->detached ? 1 : 0);
}

PyObject *CPythonInstThreads::ThreadJoinable(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyThread *T = threadOf(self);
	if (T == NULL)
		Py_RETURN_NONE;

	return PyBool_FromLong(T->joinable ? 1 : 0);
}

/* t.setLogger(func) */
PyObject *CPythonInstThreads::ThreadSetLogger(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "logger", NULL };
	PyObject *logger = NULL;
	CPyThread *T = threadOf(self);

	if (T == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "thread is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", (char **)kwlist, &logger))
		return NULL;

	Py_XINCREF(logger);
	Py_XDECREF(T->logger);
	T->logger = logger;
	Py_RETURN_NONE;
}

/* t.result(): what the code left in 'result' */
PyObject *CPythonInstThreads::ThreadResult(PyObject *self, PyObject *args, PyObject *kwds)
{
	CPyThread *T = threadOf(self);
	if (T == NULL || T->result == NULL)
		Py_RETURN_NONE;

	Py_INCREF(T->result);
	return T->result;
}

bool CPythonInstThreads::ThreadsRegister(PyObject *module)
{
	static PyMethodDef thread_methods[] =
	{
		PY_METHODDEF("start",     CPythonInstThreads::ThreadStart),
		PY_METHODDEF("join",      CPythonInstThreads::ThreadJoin),
		PY_METHODDEF("cancel",    CPythonInstThreads::ThreadCancel),
		PY_METHODDEF("detach",    CPythonInstThreads::ThreadDetach),
		PY_METHODDEF("alive",     CPythonInstThreads::ThreadAlive),
		PY_METHODDEF("started",   CPythonInstThreads::ThreadStarted),
		PY_METHODDEF("detached",  CPythonInstThreads::ThreadDetached),
		PY_METHODDEF("joinable",  CPythonInstThreads::ThreadJoinable),
		PY_METHODDEF("setLogger", CPythonInstThreads::ThreadSetLogger),
		/* the spelling llthreads uses, so that a script carried over from it
		   is left as it is */
		PY_METHODDEF("set_logger", CPythonInstThreads::ThreadSetLogger),
		PY_METHODDEF("result",    CPythonInstThreads::ThreadResult),
		{ NULL, NULL, 0, NULL }
	};

	static PyMethodDef module_methods[] =
	{
		PY_METHODDEF("new", CPythonInstThreads::ThreadNewModule),
		{ NULL, NULL, 0, NULL }
	};

	static PyType_Slot thread_slots[] =
	{
		{ Py_tp_new, (void *)CPythonInstThreads::ThreadNew },
		{ Py_tp_dealloc, (void *)CPythonInstThreads::ThreadDelete },
		{ Py_tp_methods, (void *)thread_methods },
		{ Py_tp_alloc, (void *)PyType_GenericAlloc },
		{ Py_tp_free, (void *)PyObject_Free },
		{ Py_tp_doc, (void *)"a thread that runs a piece of code" },
		{ 0, NULL }
	};

	static PyType_Spec thread_spec =
	{
		"neutrino.threads.threads",
		sizeof(PyThreadObject),
		0,
		Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
		thread_slots
	};

	static PyModuleDef module_def =
	{
		PyModuleDef_HEAD_INIT,
		"neutrino.threads",
		"threads",
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

	thread_type = (PyTypeObject *)PyType_FromSpec(&thread_spec);
	if (thread_type == NULL)
	{
		PyErr_Clear();
		Py_DECREF(sub);
		return false;
	}
	if (PyModule_AddObject(sub, "threads", (PyObject *)thread_type) < 0)
	{
		PyErr_Clear();
		Py_DECREF((PyObject *)thread_type);
		thread_type = NULL;
		Py_DECREF(sub);
		return false;
	}

	return PythonInstAddSubmodule(module, "threads", sub);
}



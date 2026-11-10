/*
 * python threads
 *
 * The Lua binding in lua_threads.cpp, which is a port of llthreads: a
 * thread is made with a piece of code, started, joined, and its arguments
 * are handed to it. Read as
 *
 *	from neutrino import threads
 *
 *	t = threads.new("import time\n"
 *			"print('worker', argv[0])\n"
 *			"result = 42\n", "one")
 *	t.start()
 *	t.join()
 *
 * Where llthreads compiles the code into a child Lua state of its own, this
 * compiles it and runs it in a thread state of its own, with the arguments
 * in 'argv' and 'result' left for a reader that asks for it.
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

#ifndef _PYTHREADS_H_
#define _PYTHREADS_H_

#include "pyinstance_helpers.h"

#include <pthread.h>
#include <string>

#define PYTHREADS_NAME "threads"

/* One thread: the code it runs, the arguments it was handed, and the flags
 * llthreads keeps about it. */
class CPyThread
{
	public:
		PyObject *code;		/* the compiled code, or NULL */
		PyObject *source;	/* the text it came from */
		PyObject *args;		/* the arguments, a tuple */
		PyObject *logger;	/* a callable told what the thread says */
		PyObject *result;	/* what the code left in 'result' */
		pthread_t thread;
		bool started;
		bool detached;
		bool joined;
		bool joinable;
		bool alive;
		unsigned long thread_ident;

		CPyThread();
		~CPyThread();

		void log(const char *hdr, const std::string &msg);
};

class CPythonInstThreads
{
	public:
		CPythonInstThreads() {};
		~CPythonInstThreads() {};
		static CPythonInstThreads *getInstance();
		static bool ThreadsRegister(PyObject *module);
		/* Runs the code of a thread, on the thread it was started on. */
		static void *runThread(void *arg);

	private:
		static PyObject *ThreadNew(PyTypeObject *type, PyObject *args, PyObject *kwds);
		static PyObject *ThreadNewModule(PyObject *self, PyObject *args, PyObject *kwds);
		static void ThreadDelete(PyObject *self);

		static PyObject *ThreadStart(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *ThreadJoin(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *ThreadCancel(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *ThreadDetach(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *ThreadAlive(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *ThreadStarted(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *ThreadDetached(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *ThreadJoinable(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *ThreadSetLogger(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *ThreadResult(PyObject *self, PyObject *args, PyObject *kwds);
};

#endif //_PYTHREADS_H_

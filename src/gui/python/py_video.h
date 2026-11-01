/*
 * python video functions
 *
 * The Lua binding in lua_video.cpp, read as
 *
 *	from neutrino import video
 *	v = video.new()
 *	v.zapitStopPlayBack()
 *	key = v.PlayFile("title", "/tmp/movie.ts")
 *	v.channelRezap()
 *
 * setInfoFunc takes a callable where the Lua binding takes the name of one
 * of the script's functions; the player calls it with the four numbers the
 * Lua one is called with.
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
#ifndef _PYVIDEO_H
#define _PYVIDEO_H

#include "pyinstance_helpers.h"

class CPyVideo
{
	public:
		bool singlePlay;
		PyObject *infoFunc;
		CPyVideo() { singlePlay = false; infoFunc = NULL; }
		~CPyVideo() { Py_XDECREF(infoFunc); }
};

class CPythonInstVideo
{
	public:
		CPythonInstVideo() {};
		~CPythonInstVideo() {};
		static CPythonInstVideo *getInstance();
		static bool VideoRegister(PyObject *module);
		/* Called by the movie player while a script's play runs, with the
		 * picture it is showing. */
		static bool execPythonInfoFunc(PyObject *video, int xres, int yres, int aspectRatio, int framerate);

	private:
		static PyObject *VideoNew(PyTypeObject *type, PyObject *args, PyObject *kwds);
		static PyObject *VideoNewModule(PyObject *self, PyObject *args, PyObject *kwds);
		static void VideoDelete(PyObject *self);

		static PyObject *SetBlank(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *ShowPicture(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *StopPicture(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *PlayFile(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *SetInfoFunc(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *ZapitStopPlayBack(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *ChannelRezap(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *CreateChannelIDfromUrl(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *GetNeutrinoMode(PyObject *self, PyObject *args, PyObject *kwds);
		static PyObject *SetSinglePlay(PyObject *self, PyObject *args, PyObject *kwds);
};

#endif //_PYVIDEO_H

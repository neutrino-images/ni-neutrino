/*
 * python video functions
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

#include "py_video.h"

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>
#include <system/debug.h>
#include <system/helpers.h>
#include <gui/movieplayer.h>
#include <gui/widget/msgbox.h>
#include <driver/framebuffer.h>
#include <hardware/video.h>
#include <neutrino.h>
#include <zapit/zapit.h>
#include <zapit/getservices.h>

#include "pyinstance.h"

extern cVideo *videoDecoder;

typedef struct
{
	PyObject_HEAD
	CPyVideo *d;
} PyVideoObject;

static PyTypeObject *video_type = NULL;

CPythonInstVideo *CPythonInstVideo::getInstance()
{
	static CPythonInstVideo *PythonInstVideo = NULL;

	if (!PythonInstVideo)
		PythonInstVideo = new CPythonInstVideo();
	return PythonInstVideo;
}

static CPyVideo *videoOf(PyObject *self)
{
	PyVideoObject *o = (PyVideoObject *)self;
	return o->d;
}

PyObject *CPythonInstVideo::VideoNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	if ((args != NULL && PyTuple_GET_SIZE(args) > 0) || (kwds != NULL && PyDict_Size(kwds) > 0))
	{
		PyErr_SetString(PyExc_TypeError, "video.new() takes no arguments");
		return NULL;
	}

	PyVideoObject *self = (PyVideoObject *)type->tp_alloc(type, 0);
	if (self == NULL)
	{
		PyErr_Clear();
		return NULL;
	}
	self->d = new CPyVideo();
	return (PyObject *)self;
}

PyObject *CPythonInstVideo::VideoNewModule(PyObject *self, PyObject *args, PyObject *kwds)
{
	if (video_type == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "video is not registered");
		return NULL;
	}
	return VideoNew(video_type, args, kwds);
}

void CPythonInstVideo::VideoDelete(PyObject *self)
{
	PyVideoObject *o = (PyVideoObject *)self;
	delete o->d;
	o->d = NULL;
	Py_TYPE(self)->tp_free(self);
}

/* v.setBlank(enable=True) */
PyObject *CPythonInstVideo::SetBlank(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "enable", NULL };
	int enable = 1;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|p", (char **)kwlist, &enable))
		return NULL;

	videoDecoder->setBlank(enable ? true : false);
	Py_RETURN_NONE;
}

/* v.ShowPicture(fname) */
PyObject *CPythonInstVideo::ShowPicture(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "fname", NULL };
	const char *fname = "";

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &fname))
		return NULL;

	CFrameBuffer::getInstance()->showFrame(fname);
	Py_RETURN_NONE;
}

PyObject *CPythonInstVideo::StopPicture(PyObject *self, PyObject *args, PyObject *kwds)
{
	CFrameBuffer::getInstance()->stopFrame();
	Py_RETURN_NONE;
}

/* v.setInfoFunc(func): the callable the player reports the picture to. */
PyObject *CPythonInstVideo::SetInfoFunc(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "func", NULL };
	PyObject *func = NULL;
	CPyVideo *D = videoOf(self);

	if (D == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "video is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", (char **)kwlist, &func))
		return NULL;

	Py_XINCREF(func);
	Py_XDECREF(D->infoFunc);
	D->infoFunc = func;
	Py_RETURN_NONE;
}

/* The player calls this while a script's play runs. */
bool CPythonInstVideo::execPythonInfoFunc(PyObject *video, int xres, int yres, int aspectRatio, int framerate)
{
	if (video == NULL)
		return false;

	PyVideoObject *o = (PyVideoObject *)video;
	if (o->d == NULL || o->d->infoFunc == NULL || o->d->infoFunc == Py_None)
		return false;

	PyGILState_STATE gstate = PyGILState_Ensure();
	PyObject *res = PyObject_CallFunction(o->d->infoFunc, "iiii", xres, yres, aspectRatio, framerate);
	if (res == NULL)
	{
		PyObject *type = NULL, *value = NULL, *traceback = NULL;
		PyErr_Fetch(&type, &value, &traceback);
		PyErr_NormalizeException(&type, &value, &traceback);
		char msg[1024];
		std::string what = "NULL";
		if (value != NULL)
		{
			PyObject *str = PyObject_Str(value);
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
		snprintf(msg, sizeof(msg) - 1, "[CPythonInstVideo::%s] error running the info function: %s", __func__, what.c_str());
		fprintf(stderr, "%s\n", msg);
		DisplayErrorMessage(msg);
		Py_XDECREF(type);
		Py_XDECREF(value);
		Py_XDECREF(traceback);
		PyGILState_Release(gstate);
		return false;
	}
	Py_DECREF(res);
	PyGILState_Release(gstate);
	return true;
}

/* v.PlayFile(title, file, info1="", info2="", file2=""), answering the key
 * the play ended on, as the Lua binding answers. */
PyObject *CPythonInstVideo::PlayFile(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "title", "file", "info1", "info2", "file2", NULL };
	const char *title = "", *file = "", *info1 = "", *info2 = "", *file2 = "";
	CPyVideo *D = videoOf(self);

	if (D == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "video is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ss|sss", (char **)kwlist, &title, &file, &info1, &info2, &file2))
		return NULL;

	if ((D->singlePlay == false) && (CMoviePlayerGui::getInstance().getBlockedFromPlugin() == false))
		CMoviePlayerGui::getInstance().setBlockedFromPlugin(true);

	printf("CPythonInstVideo::%s: title %s file %s\n", __func__, title, file);

	if (D->infoFunc != NULL && D->infoFunc != Py_None)
		CMoviePlayerGui::getInstance().setPythonInfoFunc((PyObject *)self, true);
	CMoviePlayerGui::getInstance().SetFile(title, file, info1, info2, file2);
	CMoviePlayerGui::getInstance().exec(NULL, "http_lua");
printf("[pytrace] py PlayFile: the play returned\n");
	CMoviePlayerGui::getInstance().setPythonInfoFunc(NULL, false);
printf("[pytrace] py PlayFile: info func cleared\n");
	if (D->infoFunc != NULL && D->infoFunc != Py_None)
		Py_CLEAR(D->infoFunc);

	int ret = CMoviePlayerGui::getInstance().getKeyPressed();
printf("[pytrace] py PlayFile: key %d, returning to the script\n", ret);
	return PyLong_FromLong(ret);
}

/* v.zapitStopPlayBack(stop=True) */
PyObject *CPythonInstVideo::ZapitStopPlayBack(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "stop", NULL };
	int stop = 1;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|p", (char **)kwlist, &stop))
		return NULL;

	if (stop)
	{
		CMoviePlayerGui::getInstance().stopPlayBack();
		g_Zapit->stopPlayBack();
	}
	else
		g_Zapit->startPlayBack();

	Py_RETURN_NONE;
}

PyObject *CPythonInstVideo::ChannelRezap(PyObject *self, PyObject *args, PyObject *kwds)
{
	CNeutrinoApp::getInstance()->channelRezap();
	if (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_radio)
		CFrameBuffer::getInstance()->showFrame("radiomode.jpg");
	Py_RETURN_NONE;
}

/* v.createChannelIDfromUrl(url): the channel id of the url, as the text the
 * Lua binding answers. */
PyObject *CPythonInstVideo::CreateChannelIDfromUrl(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "url", NULL };
	const char *url = "";

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &url))
		return NULL;
	if (!*url)
		Py_RETURN_NONE;

	t_channel_id id = CREATE_CHANNEL_ID(0, 0, 0, url);
	char id_str[17];
	snprintf(id_str, sizeof(id_str), "%" PRIx64, id);

	return PyUnicode_FromString(id_str);
}

PyObject *CPythonInstVideo::GetNeutrinoMode(PyObject *self, PyObject *args, PyObject *kwds)
{
	return PyLong_FromLong((long)CNeutrinoApp::getInstance()->getMode());
}

/* v.setSinglePlay(mode=True) */
PyObject *CPythonInstVideo::SetSinglePlay(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "mode", NULL };
	int mode = 1;
	CPyVideo *D = videoOf(self);

	if (D == NULL)
	{
		PyErr_SetString(PyExc_RuntimeError, "video is gone");
		return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|p", (char **)kwlist, &mode))
		return NULL;

	D->singlePlay = (mode != 0);
	Py_RETURN_NONE;
}

bool CPythonInstVideo::VideoRegister(PyObject *module)
{
	static PyMethodDef video_methods[] =
	{
		PY_METHODDEF("setBlank",               CPythonInstVideo::SetBlank),
		PY_METHODDEF("ShowPicture",            CPythonInstVideo::ShowPicture),
		PY_METHODDEF("StopPicture",            CPythonInstVideo::StopPicture),
		PY_METHODDEF("PlayFile",               CPythonInstVideo::PlayFile),
		PY_METHODDEF("setInfoFunc",            CPythonInstVideo::SetInfoFunc),
		PY_METHODDEF("zapitStopPlayBack",      CPythonInstVideo::ZapitStopPlayBack),
		PY_METHODDEF("channelRezap",           CPythonInstVideo::ChannelRezap),
		PY_METHODDEF("createChannelIDfromUrl", CPythonInstVideo::CreateChannelIDfromUrl),
		PY_METHODDEF("getNeutrinoMode",        CPythonInstVideo::GetNeutrinoMode),
		PY_METHODDEF("setSinglePlay",          CPythonInstVideo::SetSinglePlay),
		{ NULL, NULL, 0, NULL }
	};

	static PyMethodDef module_methods[] =
	{
		PY_METHODDEF("new", CPythonInstVideo::VideoNewModule),
		{ NULL, NULL, 0, NULL }
	};

	static PyType_Slot video_slots[] =
	{
		{ Py_tp_new, (void *)CPythonInstVideo::VideoNew },
		{ Py_tp_dealloc, (void *)CPythonInstVideo::VideoDelete },
		{ Py_tp_methods, (void *)video_methods },
		{ Py_tp_alloc, (void *)PyType_GenericAlloc },
		{ Py_tp_free, (void *)PyObject_Free },
		{ Py_tp_doc, (void *)"video functions" },
		{ 0, NULL }
	};

	static PyType_Spec video_spec =
	{
		"neutrino.video.video",
		sizeof(PyVideoObject),
		0,
		Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
		video_slots
	};

	static PyModuleDef module_def =
	{
		PyModuleDef_HEAD_INIT,
		"neutrino.video",
		"video functions",
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

	video_type = (PyTypeObject *)PyType_FromSpec(&video_spec);
	if (video_type == NULL)
	{
		PyErr_Clear();
		Py_DECREF(sub);
		return false;
	}
	if (PyModule_AddObject(sub, "video", (PyObject *)video_type) < 0)
	{
		PyErr_Clear();
		Py_DECREF((PyObject *)video_type);
		video_type = NULL;
		Py_DECREF(sub);
		return false;
	}

	return PythonInstAddSubmodule(module, "video", sub);
}


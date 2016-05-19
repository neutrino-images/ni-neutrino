/*
	infoicons

	(C) 2009-2016 NG-Team
	(C) 2016 NI-Team

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __infoicons__
#define __infoicons__

#include <driver/framebuffer.h>
#include <driver/fontrenderer.h>
#include <system/settings.h>

#include "gui/color.h"

#include <string>

enum INFOICONS
{
	INFOICONS_STATIC	= 0,
	INFOICONS_INFOVIEWER	= 1,
	INFOICONS_POPUP		= 2
};

class CInfoIcons
{
	private:
		CFrameBuffer * 	frameBuffer;

		pthread_t	thrTimer;
		int		x, y, width, height, offset;
		int 		start_i, m_entries, popup;
		bool		status;
		static void	CleanUpProc(void* arg);
		static void*	TimerProc(void *arg);

		void	Init();
		void 	paintBackground();

	public:
		CInfoIcons();
		~CInfoIcons();
		static	CInfoIcons* getInstance();

		void 	paintIcons(bool first);
		void 	hideIcons();
		void 	StartIcons(bool msg = false);
		void 	StopIcons(bool msg = false);
		void	enableInfoIcons(bool enable);
		void	switchInfoIconsOnOff();
		void	saveIconstate();

		bool	getStatus(void) { return status; }
};

#endif

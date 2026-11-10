/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Der gemeinsame Teil der Infobar: der Zustand, den beide Ausfuehrungen
	teilen, und die Wahl zwischen ihnen. Siehe infoviewer.h.

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "infoviewer_base.h"

#include "infoviewer.h"
#include "infoviewer_evo.h"

#include <global.h>

CInfoViewerBase::CInfoViewerBase()
{
	chanready = false;
	is_visible = false;
	aspectRatio = 0;
	sec_timer_id = 0;
	SDT_freq_update = false;

	BoxStartX = BoxStartY = BoxEndX = BoxEndY = ChanInfoX = 0;
	showButtonBar = false;
	isVolscale = false;

	md5_ecmInfo = "0";

	current_channel_id = 0;
	lcdUpdateTimer = 0;
	zap_mode = IV_MODE_DEFAULT;
}

CInfoViewerBase::~CInfoViewerBase()
{
}

/* The infobar is built once, at start, and lives as long as the program does;
   that is why the setting that picks it says a restart applies. */
CInfoViewerBase *CInfoViewerBase::create()
{
	if (g_settings.infobar_evo_infoviewer)
		return new CInfoViewerEvo();

	return new CInfoViewer();
}

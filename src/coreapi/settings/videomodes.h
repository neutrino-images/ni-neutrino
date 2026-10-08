/*
 * videomodes.h - which family's table of video modes a build offers
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __coreapi_videomodes_h__
#define __coreapi_videomodes_h__

// The box model macros the selection below reads.
#include <config.h>

#include <stddef.h>

/* Decided here and not around the rows: the conditions name more box models
   than the scan that compiles every combination of a table's own conditions
   takes, and one model at a time reaches every arm of this chain. */
#define COREAPI_VIDEOMODES_PC		0
#define COREAPI_VIDEOMODES_CST_HD1	1
#define COREAPI_VIDEOMODES_CST_HD2	2
#define COREAPI_VIDEOMODES_4K		3
#define COREAPI_VIDEOMODES_OSMIO4K	4

#if BOXMODEL_CST_HD1
#define COREAPI_VIDEOMODES COREAPI_VIDEOMODES_CST_HD1
#elif BOXMODEL_CST_HD2
#define COREAPI_VIDEOMODES COREAPI_VIDEOMODES_CST_HD2
#elif BOXMODEL_HD51 || BOXMODEL_BRE2ZE4K || BOXMODEL_H7 || BOXMODEL_E4HDULTRA || BOXMODEL_PROTEK4K || BOXMODEL_HD60 || BOXMODEL_HD61 || BOXMODEL_MULTIBOX || BOXMODEL_MULTIBOXSE || BOXMODEL_VUPLUS_ALL
#define COREAPI_VIDEOMODES COREAPI_VIDEOMODES_4K
#elif BOXMODEL_OSMIO4K || BOXMODEL_OSMIO4KPLUS
#define COREAPI_VIDEOMODES COREAPI_VIDEOMODES_OSMIO4K
#else
#define COREAPI_VIDEOMODES COREAPI_VIDEOMODES_PC
#endif

namespace coreapi
{

/* Every video mode the program has a word for, in the order the settings file
   numbers its enabled_video_mode_<n> and enabled_auto_mode_<n> keys:
   VIDEOMENU_VIDEOMODE_OPTION_COUNT of them. The video_Mode row offers the ones
   this box draws, in the same words and the same order. */
const char *const *videoModeNames(size_t &count);

/* Whether this box draws the mode a number stands for, which is whether the
   video_Mode row offers a mode of that name here. */
bool videoModeDrawn(size_t index);

} // namespace coreapi

#endif

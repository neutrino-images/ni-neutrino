/*
 * predicates.h - availability tests for settings and for entries of an option list
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

#ifndef __COREAPI_PREDICATES_H__
#define __COREAPI_PREDICATES_H__

namespace coreapi
{

// All of these answer false when the box cannot say what it can do, so an
// entry or a setting is never offered on a guess.
bool canPanScan149();
bool canAspect149();
bool hasScart();
bool hasHdmi();
bool hasFan();
bool canCec();
// Can change its own clock speed.
bool canCpufreq();
// The front panel takes a brightness.
bool canSetBrightness();
// The decoder can show a second picture.
bool canPip();
// It can, and the way the box was started leaves room for it. What a screen
// has to ask before it offers a second picture, since canPip alone says yes
// where the start refuses.
bool pipUsable();
// Goes to deep standby and so can shut itself down.
bool canShutdown();
bool hasFormatButton();
// The front panel is told how often to scroll a long line, not only whether.
bool countsScrolls();
// The front panel shows a play time, which takes eight characters.
bool displayFitsPlaytime();
bool takesZappingMode();
bool takesHdmiColorimetry();
/* The analog outputs by board revision. Revision 6 offers one list for SCART
   and Cinch together. Above it the two are separate lists, SCART with the HD
   entries on every revision but 10 and Cinch only where the build has one.
   Below it only the two SD SCART entries, and only with a SCART socket. */
bool analogOneItem();
bool analogOutputsSplit();
bool hasAnalogCinch();
bool scartSdOffered();
bool scartHdOffered();
// More than one tuner switched on in the tuner setup.
bool severalTunersEnabled();
// The kernel knows the file system and a mkfs for it is there.
bool formatsExt4();
bool formatsExt3();
bool formatsExt2();
bool formatsF2fs();
bool formatsVfat();
bool formatsExfat();
bool formatsXfs();
// The box draws its own screen at that size.
bool drawsOsd720();
bool drawsOsd1080();

} // namespace coreapi

#endif

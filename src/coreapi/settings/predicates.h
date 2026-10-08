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
// How many pictures the decoders can show at once beside the main one, none
// where the box cannot say.
int pipWindows();
// The display is a graphical one, which is what the graphical LCD is on by
// default for.
bool hasGraphicPanel();
// The display is a numeric one.
bool hasNumericPanel();
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
/* What the board revision decides beyond the analog outputs, by the screens' own
   tests. Revision 1 is also what every box without a Coolstream board reports,
   so hasDbdr is the one that is false there and hasHddPowerFlag the one that is
   true. */
// The DBDR option is offered, which the first board revision and every box
// without a Coolstream board lack.
bool hasDbdr();
// Boards above revision 7, which leaves out the first two families, have power
// LEDs with modes of their own.
bool hasLedMenu();
// The panel has a backlight to switch.
bool hasBacklight();
// The front panel is wired up on this board. Revisions 10 and 11 have none.
bool vfdEnabled();
// The panel is wired up and its driver takes a count of scrolls. The first of the two shapes
// of the scroll row, whose second shape asks vfdEnabled alone.
bool vfdCountsScrolls();
// The panel takes a brightness and is wired up, which is what the brightness
// settings need together.
bool canSetPanelBrightness();
// A flag file keeps the disk powered, which the boards below the eighth need.
bool hasHddPowerFlag();
// The oldest family has a SCART picture fix that a flag file switches on.
bool hasScartOsdFix();
// The input driver can be told which remote to listen to.
bool canSelectRemote();
// The module offers a delay, the rpr setting and a clock above high.
bool ciExtended();
// More than one tuner in the box, whether switched on or not.
bool severalTunersFitted();
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

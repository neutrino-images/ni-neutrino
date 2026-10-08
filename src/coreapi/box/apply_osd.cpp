/*
 * apply_osd.cpp - what makes a changed on screen display setting take effect
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

#include <config.h>

#include "coreapi/box/apply_osd.h"
#include "coreapi/box/sentstate.h"

#include <system/settings.h>

#include <string>
#include <utility>
#include <vector>

extern SNeutrinoSettings g_settings;

namespace coreapi
{

namespace
{

class NoOsdOutput : public OsdOutput
{
	public:
		Status setupFonts(FontSetup) { return Status::NotSupported; }
		Status setPalette() { return Status::NotSupported; }
		Status setScreenGeometry() { return Status::NotSupported; }
		Status resetLcd4lParse() { return Status::NotSupported; }
		Status resetInfoViewer() { return Status::NotSupported; }
		Status clearInfoClock() { return Status::NotSupported; }
		Status refreshVolumeBar() { return Status::NotSupported; }
		Status refreshMuteIcon() { return Status::NotSupported; }
		Status resetRadioText() { return Status::NotSupported; }
		Status clearIconCache() { return Status::NotSupported; }
		Status resetInfoIcons() { return Status::NotSupported; }
		Status resetChannelList() { return Status::NotSupported; }
};

NoOsdOutput g_no_osd_output;
OsdOutput *g_osd_output = 0;

struct OsdState
{
	Sent<std::string> font_file;
	Sent<std::string> font_mono;
	Sent<std::pair<int, int> > scaling;
	Sent<std::vector<int> > clock;
	Sent<int> clock_size;
};

OsdState g_state;
SentFlags g_flags;

void takeStale()
{
	const unsigned m = g_flags.take();
	if ((m & (unsigned) OsdSent::Fonts) != 0)
		g_state.font_file.known = g_state.font_mono.known = g_state.scaling.known = false;
	if ((m & (unsigned) OsdSent::InfoClock) != 0)
		g_state.clock.known = false;
}

/* The widest rebuild any of the three asks for: a new face needs everything, a new
   scaling the renderers, and the monospace face alone only the shell's. */
bool fontsToRebuild(FontSetup &what)
{
	const std::string file = settingsText(g_settings.font_file);
	const std::string mono = settingsText(g_settings.font_file_monospace);
	const std::pair<int, int> scaling(g_settings.font_scaling_x, g_settings.font_scaling_y);

	if (g_state.font_file.differs(file))
		what = FontSetup::All;
	else if (g_state.scaling.differs(scaling))
		what = FontSetup::Scaling;
	else if (g_state.font_mono.differs(mono))
		what = FontSetup::Monospace;
	else
		return false;
	return true;
}

Status runFonts()
{
	takeStale();
	Status first = Status::Ok;

	FontSetup what = FontSetup::All;
	if (!fontsToRebuild(what))
		return first;

	const Status s = osdOutput().setupFonts(what);
	noteFirst(first, s);
	if (s == Status::Ok)
	{
		/* Read again after the call: a face that is not on the box is replaced by
		   the shipped one inside the rebuild, and the setting then says so. */
		g_state.font_file.known = g_state.font_mono.known = g_state.scaling.known = true;
		g_state.font_file.value = settingsText(g_settings.font_file);
		g_state.font_mono.value = settingsText(g_settings.font_file_monospace);
		g_state.scaling.value = std::make_pair(g_settings.font_scaling_x, g_settings.font_scaling_y);
	}
	return first;
}

/* The palette is the colours of the theme and nothing else, so a run sends them all
   and a colour written twice with the same value costs one more fill of the table. */
Status runPalette()
{
	return osdOutput().setPalette();
}

/* Taking the slot again is a copy of four numbers, so a run costs nothing a second run
   would not, and the bars are made again by their own next use. */
Status runScreenGeometry()
{
	return osdOutput().setScreenGeometry();
}

/* The skin is parsed once per change of what it shows, which the display daemon keeps as
   a number of its own, and a run only sets that back to nought. */
Status runEventLogo()
{
	/* The list draws the logo of the event in its header, so it lays that out again too. */
	Status first = Status::Ok;
	noteFirst(first, osdOutput().resetLcd4lParse());
	noteFirst(first, osdOutput().resetChannelList());
	return first;
}

/* The modules are rebuilt from the settings on the next drawing of the list, so a run
   costs nothing a second run would not. */
Status runChannelList()
{
	return osdOutput().resetChannelList();
}

const char *const kChannelListKeys[] =
{
	"channellist_additional",
	"channellist_epgtext_alignment",
	"channellist_show_res_icon",
	"progressbar_design_channellist",
	"channellist_show_infobox",
	"channellist_foot",
	"channellist_show_numbers"
};

const char *const kEventLogoKeys[] =
{
	"channellist_show_eventlogo"
};

Status runInfoViewer()
{
	return osdOutput().resetInfoViewer();
}

/* The channel logo and the provider, the conditional access bar and its frame, the second
   tuner and the progress bar each change what the modules of the infobar are made of. */
const char *const kInfoViewerKeys[] =
{
	"infobar_show_channellogo",
	"infobar_sat_display",
	"infobar_casystem_display",
	"infobar_casystem_frame",
	"infobar_show_tuner",
	"infobar_progressbar"
};

/* The clock is drawn again for any of its four settings, and only when one differs from what
   it was last drawn with: the redraw clears the clock on the screen. A new size also moves
   the volume bar and the mute icon, which are laid out around the clock. */
Status runInfoClock()
{
	takeStale();
	Status first = Status::Ok;

	std::vector<int> now;
	now.push_back(g_settings.mode_clock);
	now.push_back(g_settings.infoClockFontSize);
	now.push_back(g_settings.infoClockSeconds);
	now.push_back(g_settings.infoClockBackground);
	if (!g_state.clock.differs(now))
		return first;

	OsdOutput &out = osdOutput();
	bool done = true;
	if (g_state.clock_size.differs(g_settings.infoClockFontSize))
	{
		const Status v = out.refreshVolumeBar();
		const Status m = out.refreshMuteIcon();
		noteFirst(first, v);
		noteFirst(first, m);
		if (v == Status::Ok && m == Status::Ok)
		{
			g_state.clock_size.known = true;
			g_state.clock_size.value = g_settings.infoClockFontSize;
		}
		else
			done = false;
	}
	const Status s = out.clearInfoClock();
	noteFirst(first, s);
	if (s == Status::Ok && done)
	{
		g_state.clock.known = true;
		g_state.clock.value = now;
	}
	return first;
}

const char *const kInfoClockKeys[] =
{
	"mode_clock",
	"infoClockFontSize",
	"infoClockSeconds",
	"infoClockBackground"
};

/* The digits and the room around them are measured when the bar is laid out, so a change of
   what it shows or of its size is a layout again. */
Status runVolumeBar()
{
	return osdOutput().refreshVolumeBar();
}

const char *const kVolumeBarKeys[] =
{
	"volume_digits",
	"volume_size"
};

/* Starting a decoder that runs and stopping one that does not are what a run comes to, so a
   run with the same setting finds nothing to do. */
Status runRadioText()
{
	return osdOutput().resetRadioText();
}

const char *const kRadioTextKeys[] =
{
	"radiotext_enable"
};

/* Starting the icons that run and stopping those that do not are what a run comes to, so
   the group keeps nothing of what it sent: the on and off switch of the icons flips the
   setting and starts them itself, and a run after it finds them as they should be. */
Status runInfoIcons()
{
	return osdOutput().resetInfoIcons();
}

const char *const kInfoIconsKeys[] =
{
	"mode_icons"
};

/* The icons were drawn for a size, and the cache holds them at that one. */
Status runOsdResolution()
{
	return osdOutput().clearIconCache();
}

const char *const kOsdResolutionKeys[] =
{
	"osd_resolution"
};

const char *const kFontsKeys[] =
{
	"font_file",
	"font_file_monospace",
	"font_scaling_x",
	"font_scaling_y"
};

/* The preset and the corners of each of its two slots at each of the two sizes, a
   the full pixel preset and b the other, 0 the 720 line size and 1 the 1080 line one. */
const char *const kScreenGeometryKeys[] =
{
	"screen_preset",
	"screen_StartX_a_0",
	"screen_StartY_a_0",
	"screen_EndX_a_0",
	"screen_EndY_a_0",
	"screen_StartX_a_1",
	"screen_StartY_a_1",
	"screen_EndX_a_1",
	"screen_EndY_a_1",
	"screen_StartX_b_0",
	"screen_StartY_b_0",
	"screen_EndX_b_0",
	"screen_EndY_b_0",
	"screen_StartX_b_1",
	"screen_StartY_b_1",
	"screen_EndX_b_1",
	"screen_EndY_b_1"
};

const char *const kPaletteKeys[] =
{
	"theme.menu_Head",
	"theme.menu_Head_Text",
	"theme.menu_Content",
	"theme.menu_Content_Text",
	"theme.menu_Content_Selected",
	"theme.menu_Content_Selected_Text",
	"theme.menu_Content_inactive",
	"theme.menu_Content_inactive_Text",
	"theme.menu_Foot",
	"theme.menu_Foot_Text",
	"theme.infobar",
	"theme.infobar_Text",
	"theme.infobar_casystem",
	"theme.channellist_Description_Text",
	"theme.colored_events",
	"theme.progressbar_passive",
	"theme.progressbar_active",
	"theme.shadow",
	"theme.clock_Digit"
};

} // namespace

OsdOutput &osdOutput()
{
	if (!g_osd_output)
		return g_no_osd_output;
	return *g_osd_output;
}

void setOsdOutput(OsdOutput *o) { g_osd_output = o; }

void forgetSentOsd(OsdSent what)
{
	g_flags.mark((unsigned) what);
}

void resetSentOsd()
{
	g_state = OsdState();
	g_flags.reset();
}

/* After the framebuffer and the font renderers' owner exist and before anything
   draws, where startup set the fonts up itself. */
const ApplyGroup kFontsApplyGroup = { "fonts", ApplyPhase::Framebuffer, COREAPI_KEYS(kFontsKeys), &runFonts };

/* Right after the fonts, where startup filled the palette: nothing draws before it. */
const ApplyGroup kPaletteApplyGroup = { "palette", ApplyPhase::Framebuffer, COREAPI_KEYS(kPaletteKeys), &runPalette };

/* The program's own load took the slot already, so the run at startup finds it in place. */
const ApplyGroup kScreenGeometryApplyGroup = { "screenGeometry", ApplyPhase::Framebuffer, COREAPI_KEYS(kScreenGeometryKeys), &runScreenGeometry };

const ApplyGroup kEventLogoApplyGroup = { "eventLogo", ApplyPhase::Framebuffer, COREAPI_KEYS(kEventLogoKeys), &runEventLogo };

/* After the infobar is made, which is right before the network phase: until then there is
   nothing to reset, and the infobar reads the settings when it is made. */
/* The list is made with the rest of the screens before the network phase. */
const ApplyGroup kChannelListApplyGroup = { "channelList", ApplyPhase::Network, COREAPI_KEYS(kChannelListKeys), &runChannelList };

const ApplyGroup kInfoViewerApplyGroup = { "infoViewer", ApplyPhase::Network, COREAPI_KEYS(kInfoViewerKeys), &runInfoViewer };

/* With the infobar and the file time, which are made right before the network phase. */
const ApplyGroup kInfoClockApplyGroup = { "infoClock", ApplyPhase::Network, COREAPI_KEYS(kInfoClockKeys), &runInfoClock };

/* With the icons, which the startup starts right before the network phase when the setting is on. */
const ApplyGroup kInfoIconsApplyGroup = { "infoIcons", ApplyPhase::Network, COREAPI_KEYS(kInfoIconsKeys), &runInfoIcons };

const ApplyGroup kVolumeBarApplyGroup = { "volumeBar", ApplyPhase::Network, COREAPI_KEYS(kVolumeBarKeys), &runVolumeBar };

const ApplyGroup kRadioTextApplyGroup = { "radioText", ApplyPhase::Network, COREAPI_KEYS(kRadioTextKeys), &runRadioText };

const ApplyGroup kOsdResolutionApplyGroup = { "osdResolution", ApplyPhase::Framebuffer, COREAPI_KEYS(kOsdResolutionKeys), &runOsdResolution };

} // namespace coreapi

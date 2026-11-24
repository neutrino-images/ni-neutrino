/*
 * script video
 *
 * The work of the video module both bindings wrote out themselves:
 * script_video.h says what of it is shared and what a binding still reads
 * itself.
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

#include "script_video.h"

#include <config.h>
#include <cinttypes>
#include <cstdio>

#include <global.h>
#include <neutrino.h>
#include <driver/framebuffer.h>
#include <driver/screenshot.h>
#include <gui/movieplayer.h>
#include <zapit/zapit.h>
#include <zapit/getservices.h>

int ScriptVideo::playFile(bool singlePlay, const std::string &title, const std::string &file,
			  const std::string &info1, const std::string &info2,
			  const std::string &file2, PlayHook *hook)
{
	CMoviePlayerGui &player = CMoviePlayerGui::getInstance();

	if ((singlePlay == false) && (player.getBlockedFromPlugin() == false))
		player.setBlockedFromPlugin(true);

	/* the player takes these two by a reference it may write to */
	std::string name(title);
	std::string fname(file);

	if (hook)
		hook->infoFunc(true);
	player.SetFile(name, fname, info1, info2, file2);
	player.exec(NULL, "http_script");
	if (hook)
		hook->infoFunc(false);

	return player.getKeyPressed();
}

bool ScriptVideo::restoreAfterScript()
{
	CMoviePlayerGui &player = CMoviePlayerGui::getInstance();

	if (!player.getBlockedFromPlugin())
		return false;

	player.setBlockedFromPlugin(false);
	player.restoreNeutrino();
	return true;
}

void ScriptVideo::stopPlayBack(bool stop)
{
	if (stop)
	{
		CMoviePlayerGui::getInstance().stopPlayBack();
		g_Zapit->stopPlayBack();
	}
	else
		g_Zapit->startPlayBack();
}

void ScriptVideo::channelRezap()
{
	CNeutrinoApp::getInstance()->channelRezap();
	if (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_radio)
		CFrameBuffer::getInstance()->showFrame("radiomode.jpg");
}

std::string ScriptVideo::channelIdOfUrl(const char *url)
{
	t_channel_id id = CREATE_CHANNEL_ID(0, 0, 0, url);
	char id_str[17];
	snprintf(id_str, sizeof(id_str), "%" PRIx64, id);
	return id_str;
}

bool ScriptVideo::screenshot(const std::string &name, bool osd, bool video)
{
#ifdef SCREENSHOT
	CScreenShot shot("/tmp/" + name + ".png", (CScreenShot::screenshot_format_t)0 /*PNG*/);
	shot.EnableOSD(osd);
	shot.EnableVideo(video);
	return shot.StartSync();
#else
	(void)name;
	(void)osd;
	(void)video;
	return false;
#endif
}

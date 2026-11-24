/*
 * script misc
 *
 * The two calls of the misc module that carry work of their own, written here
 * for both bindings: script_misc.h says what of it is shared and what a binding
 * still reads itself.
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

#include "script_misc.h"

#include <config.h>
#include <cstdio>
#include <sys/time.h>

#include <global.h>
#include <neutrino.h>
#include <driver/volume.h>
#include <driver/rcinput.h>
#include <gui/update.h>
#include <cs_api.h>

extern CVolume *g_volume;

int ScriptMisc::strFind(const std::string &text, const std::string &pattern, int pos, int n)
{
	size_t from = (pos < 0) ? 0 : (size_t)pos;
	size_t ret = (n < 0) ? text.find(pattern, from) : text.find(pattern.c_str(), from, (size_t)n);

	return (ret == std::string::npos) ? -1 : (int)ret;
}

std::string ScriptMisc::strSub(const std::string &text, int pos, int len)
{
	if (pos < 0 || (size_t)pos > text.length())
		return std::string();

	if (len < 0)
		return text.substr((size_t)pos);

	return text.substr((size_t)pos, (size_t)len);
}

void ScriptMisc::setVolume(int vol)
{
	if (g_settings.hdmi_cec_volume)
		return;

	if (vol < 0)
		vol = 0;
	if (vol > 100)
		vol = 100;

	g_settings.current_volume = vol;
	g_volume->setvol(vol);
}

unsigned int ScriptMisc::getRevision(std::string &hw)
{
#if HAVE_CST_HARDWARE
	hw = "Coolstream";
#else
	hw = g_info.hw_caps->boxvendor;
	hw += "_";
	hw += g_info.hw_caps->boxname;
#endif
	return cs_get_revision();
}

bool ScriptMisc::versionOk(int major, int minor, int apiMajor, int apiMinor, std::string &msg)
{
	if ((major > apiMajor) || ((major == apiMajor) && (minor > apiMinor)))
	{
		char buf[1024];
		snprintf(buf, sizeof(buf) - 1, "%s (v%d.%d)\n%s v%d.%d",
			g_Locale->getText(LOCALE_SCRIPTAPI_VERSIONSCHECK1),
			apiMajor, apiMinor,
			g_Locale->getText(LOCALE_SCRIPTAPI_VERSIONSCHECK2),
			major, minor);
		msg = buf;
		return false;
	}
	return true;
}

bool ScriptMisc::postMsg(int msg)
{
	neutrino_msg_t post_msg = 0;

	switch (msg)
	{
		case POSTMSG_STANDBY_ON:
			post_msg = NeutrinoMessages::STANDBY_ON;
			break;
		default:
			return false;
	}

	g_RCInput->postMsg(post_msg, 0);
	return true;
}

double ScriptMisc::getTimeOfDay()
{
	struct timeval t1;
	gettimeofday(&t1, NULL);
	return (double)t1.tv_sec + ((double)t1.tv_usec) / 1000000ULL;
}

void ScriptMisc::flashUpdate(const char *mode)
{
	CFlashUpdate flash;
	flash.exec(NULL, mode);
}

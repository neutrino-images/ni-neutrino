/*
 * script video
 *
 * What a script does with the player and the live picture, written here for
 * both bindings. A binding reads its arguments, calls this, and answers in
 * its own language. The calls that are one call into the box - setBlank,
 * ShowPicture, StopPicture, getNeutrinoMode - stay in the bindings: a
 * function here would only forward them.
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

#ifndef _SCRIPTVIDEO_H
#define _SCRIPTVIDEO_H

#include <string>

class ScriptVideo
{
	public:
		/* The way back into a binding while a play runs: the info function
		 * of a script is a thing of its language, so the binding switches
		 * it on before the play and off after it. */
		class PlayHook
		{
			public:
				virtual ~PlayHook() {}
				virtual void infoFunc(bool on) = 0;
		};

		/* v.PlayFile(): plays the file and answers the key the play ended
		 * on. Unless the script asked for a single play, the player is
		 * marked as blocked from a plugin first, so that the box is not
		 * restored between two plays; restoreAfterScript() takes that back.
		 * The hook may be NULL. */
		static int playFile(bool singlePlay, const std::string &title, const std::string &file,
				    const std::string &info1, const std::string &info2,
				    const std::string &file2, PlayHook *hook);

		/* What both bindings do when a script ends: where a play of it left
		 * the player blocked, the block is taken off and the box restored.
		 * Answers whether that was so. */
		static bool restoreAfterScript();

		/* v.zapitStopPlayBack(stop): stops the player and the live picture,
		 * or starts the live picture again. */
		static void stopPlayBack(bool stop);

		/* v.channelRezap(): back onto the channel, with the radio picture
		 * where the box is in radio mode. */
		static void channelRezap();

		/* v.createChannelIDfromUrl(url): the channel id of a url, as the
		 * hex digits a script gets. */
		static std::string channelIdOfUrl(const char *url);

		/* v.Screenshot(): writes /tmp/<name>.png. Answers false where it
		 * failed, and where the box cannot take one at all. */
		static bool screenshot(const std::string &name, bool osd, bool video);
};

#endif /* _SCRIPTVIDEO_H */

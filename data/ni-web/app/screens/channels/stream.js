/* Where a player fetches a channel, and how a playlist reaches the disc.
 *
 * THE PLAYLIST IS NOT FETCHED, IT IS FOLLOWED. What the browser is asked for
 * is a file, and a file is what an anchor is for: the answer never passes
 * through this page, so nothing here has to hold a stream in memory or invent
 * a name for it. The box names it, in a Content-Disposition of the route's
 * own, and the download attribute beside it is only what a box that has not
 * learned to send that header falls back to. Either way the file on the disc
 * is called after the channel and not after the identifier.
 *
 * THE ANCHOR IS MADE AND CLICKED because the marks in a row are buttons: one
 * list of actions is drawn twice by ui/actions.js, once as marks and once
 * inside a sheet, and a link in one of the two would be a control the other
 * does not have. Where an action stands on its own, as on the playback
 * screen, it is a real link and this is not used.
 */
import { channelId } from '../../fmt.js';

// The route that writes one channel out as a playlist. Named here so that the
// two screens offering it cannot come to spell it differently.
const kPlaylist = '/api/v1/stream/playlist/';

/**
 * @param {{ id: string }} channel
 * @returns {string}
 */
export function playlistHref(channel) {
	return kPlaylist + encodeURIComponent(channelId(channel.id));
}

/**
 * What the file is called where the box did not say.
 *
 * The same alphabet the box keeps to, because a name that reached a disc
 * through this page and one that reached it through the header should be the
 * same name. Everything outside it becomes one hyphen, and a run of them
 * becomes one, so a channel called "3sat HD" and one called "3sat  HD" do not
 * end up as two files that look alike.
 *
 * @param {{ name?: string, id: string }} channel
 * @returns {string}
 */
export function playlistFile(channel) {
	const plain = String(channel.name || '').replace(/[^A-Za-z0-9.\-_]+/g, '-').replace(/^-+|-+$/g, '');
	return (plain === '' ? channelId(channel.id) : plain) + '.m3u';
}

/**
 * Ask the browser for the playlist of one channel as a file.
 *
 * @param {{ name?: string, id: string }} channel
 * @returns {void}
 */
export function fetchPlaylist(channel) {
	const link = document.createElement('a');
	link.href = playlistHref(channel);
	link.download = playlistFile(channel);
	/* In the document for the click and out of it again straight after: an
	   anchor left behind is one the next tab press lands on. */
	document.body.appendChild(link);
	link.click();
	document.body.removeChild(link);
}

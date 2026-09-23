/* Everything this destination works out about a name, and nothing that asks the box.

   Its own file because every one of these is a place to be wrong once rather than in
   three screens: the parent of a name, the address a name is reached under, and
   whether a name is worth offering a player for. Nothing here reaches the network or
   holds state.

   The one rule the rest of the destination is built on: a directory is an absolute
   name with no separator at the end, and the root is the one exception. The box
   answers the same listing for /tmp and for /tmp/, but the store keys an entry by
   the address it asks for, so two spellings of one directory would ask the box twice
   and draw two screens that drift apart. */

/** @param {string} value @returns {string} */
export function tidy(value) {
	let path = String(value === undefined || value === null ? '' : value).trim();
	if (path === '') {
		return '';
	}
	// A name with no separator in front is not something to guess a directory
	// for: this layer has none to read it against, and the box says so too.
	if (path.charAt(0) !== '/') {
		path = '/' + path;
	}
	// Doubled separators are the ordinary result of joining two pieces, and one
	// left in the middle is a second spelling of one directory.
	while (path.indexOf('//') !== -1) {
		path = path.split('//').join('/');
	}
	while (path.length > 1 && path.charAt(path.length - 1) === '/') {
		path = path.slice(0, path.length - 1);
	}
	return path;
}

/** @param {string} path @returns {string} */
export function leafOf(path) {
	const tidied = tidy(path);
	const cut = tidied.lastIndexOf('/');
	return cut <= 0 ? tidied.slice(1) : tidied.slice(cut + 1);
}

// The directory that holds a name, and the empty string for the root, which
// has none. The empty answer is what the screen draws as "there is no level
// above this one" rather than as a link back to the same place.
/** @param {string} path @returns {string} */
export function parentOf(path) {
	const tidied = tidy(path);
	if (tidied === '' || tidied === '/') {
		return '';
	}
	const cut = tidied.lastIndexOf('/');
	return cut === 0 ? '/' : tidied.slice(0, cut);
}

/** @param {string} dir @param {string} name @returns {string} */
export function joinPath(dir, name) {
	const left = tidy(dir);
	const right = String(name === undefined || name === null ? '' : name);
	if (right === '') {
		return left;
	}
	return tidy(left === '/' ? '/' + right : left + '/' + right);
}

/**
 * The pieces of a name, each with the name it is reached under, so that the
 * trail above a listing is read off the path and never kept beside it.
 *
 * @typedef {{ label: string, path: string }} Crumb
 * @param {string} path
 * @returns {Crumb[]}
 */
export function crumbsOf(path) {
	const tidied = tidy(path);
	/** @type {Crumb[]} */
	const out = [];
	if (tidied === '') {
		return out;
	}
	let walked = '';
	for (const part of tidied.split('/')) {
		if (part === '') {
			continue;
		}
		walked = walked + '/' + part;
		out.push({ label: part, path: walked });
	}
	return out;
}

/* What a name says it holds, out of the last dot in it.

   The extension and not the type the box sent, because the box sends a type only
   with the bytes and this decides whether to ask for the bytes at all. It is a guess
   and it is treated as one: what it picks is whether a player is offered beside a
   name.

   Stored compression is left out on purpose. A name ending in .gz leaves the box as
   the bytes it is, packed, and a player handed those has nothing to play. */
const kVideo = ['.ts', '.m2ts', '.mpg', '.mpeg', '.vob', '.mp4', '.m4v', '.mkv', '.webm', '.mov'];
const kAudio = ['.mp3', '.m4a', '.aac', '.flac', '.ogg', '.wav'];

/* THE THREE ROADS A FILE TAKES, WHICH ARE NOT THE THREE A CHANNEL TAKES.

   A recording of this box is a transport stream, and no browser opens one. What
   opens it is the demuxer the page already loads for live television: it takes the
   container apart and hands the browser fragmented MP4, so the pictures inside it
   are the browser's business and the container is not.

   What the demuxer cannot do is decode. A recording from a standard definition
   channel is MPEG-2, the demuxer does not even carry it across, and no browser has
   decoded it for years. That is not a defeat, it is the answer, and it is said in
   words with the address beside it.

   A program stream is refused before anything is fetched. .mpg, .mpeg and .vob are
   not transport streams, the demuxer opens none of them, and the picture in one is
   MPEG-2 in practice. Measured rather than assumed for the transport streams beside
   them: a .ts carrying H.264 plays, one carrying MPEG-2 arrives as sound with no
   picture at all, and that is caught where it shows.

   Everything the browser opens by itself takes neither road and is simply handed
   over, which is what .mp4 and the sound formats do today. */
const kDemuxed = ['.ts', '.m2ts'];
const kProgram = ['.mpg', '.mpeg', '.vob'];

/** @param {string} name @returns {string} */
export function extensionOf(name) {
	const leaf = leafOf(name);
	const dot = leaf.lastIndexOf('.');
	return dot <= 0 ? '' : leaf.slice(dot).toLowerCase();
}

// video, audio, or nothing at all.
/** @param {string} name @returns {string} */
export function mediaKind(name) {
	const suffix = extensionOf(name);
	if (suffix === '') {
		return '';
	}
	for (const known of kVideo) {
		if (known === suffix) {
			return 'video';
		}
	}
	for (const known of kAudio) {
		if (known === suffix) {
			return 'audio';
		}
	}
	return '';
}

/**
 * @typedef {object} Road
 * @property {'here' | 'demuxed' | 'none'} how handed to the browser, taken apart
 *           first, or not offered at all
 * @property {boolean} sound sound with no picture, which is an audio element
 * @property {string} why empty unless the road is none: 'program' for a container
 *           nothing here opens, 'unknown' for a name this page reads nothing out of
 */

/**
 * Which of the three a name takes.
 *
 * @param {string} name
 * @returns {Road}
 */
export function fileRoad(name) {
	const suffix = extensionOf(name);
	const kind = mediaKind(name);
	if (kind === 'audio') {
		return { how: 'here', sound: true, why: '' };
	}
	if (kind === '') {
		return { how: 'none', sound: false, why: 'unknown' };
	}
	for (const known of kProgram) {
		if (known === suffix) {
			return { how: 'none', sound: false, why: 'program' };
		}
	}
	for (const known of kDemuxed) {
		if (known === suffix) {
			return { how: 'demuxed', sound: false, why: '' };
		}
	}
	return { how: 'here', sound: false, why: '' };
}

/* The order a listing is drawn in.

   The box hands the names over in the order the directory gave them, and says so,
   because there is no order a directory promises to give again. That is the right
   answer for a route and the wrong one for a screen: a person looking for a name in
   eight hundred of them reads a sorted list and scans an unsorted one.

   Directories first, because that is what the next click is, and then by name without
   regard to case. localeCompare and not a comparison of code units: the names on
   these disks carry umlauts, and code units put them behind z. */
/** @param {{ id: string, kind: string }} a @param {{ id: string, kind: string }} b @returns {number} */
export function byKindThenName(a, b) {
	const left = a.kind === 'dir' ? 0 : 1;
	const right = b.kind === 'dir' ? 0 : 1;
	if (left !== right) {
		return left - right;
	}
	return String(a.id).localeCompare(String(b.id), undefined, { sensitivity: 'base' });
}

/* Where a listing puts the directory it is showing, and why it is not a segment of
   the path.

   It is the query because an escaped separator is a separator to this server. The
   static layer decodes a target whole and splits it afterwards, on purpose
   (src/httpd/static.cpp, segmentsOf), so /files/list/%2Ftmp reaches it as
   /files/list//tmp, which carries an empty segment in the middle and is refused 400.
   Measured, not guessed. A query is untouched by that rule, and it buys a second
   thing: the rule that tells a mistyped module from an address reads the last segment
   for a dot, so a directory whose name carries one would be a 404 in the path.

   Every value goes through encodeURIComponent, so nothing this builds carries a
   separator, an ampersand or a plus that the reader below would take for punctuation
   of its own. */
/** @param {string} base @param {string} dir @returns {string} */
export function dirHref(base, dir) {
	return dir === '' ? base : base + '?path=' + encodeURIComponent(dir);
}

/** @param {string} url @returns {string} */
export function dirInUrl(url) {
	const query = String(url === undefined || url === null ? '' : url).split('?')[1];
	if (query === undefined || query === '') {
		return '';
	}
	for (const piece of query.split('&')) {
		const cut = piece.indexOf('=');
		const key = cut === -1 ? piece : piece.slice(0, cut);
		if (decodeURIComponent(key) !== 'path') {
			continue;
		}
		return cut === -1 ? '' : tidy(decodeURIComponent(piece.slice(cut + 1)));
	}
	return '';
}

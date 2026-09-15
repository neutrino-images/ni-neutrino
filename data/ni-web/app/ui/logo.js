// A channel's picture, wherever one is drawn, and the switch that turns them
// all off.
//
// The box carries that switch in its web server settings and answers it at
// System (src/httpd/ep/ep_webserver.cpp). A caller granted Read cannot read it,
// which on a home network is most callers, so the answer here has three states
// and not two: on, off, and not knowable. Not knowable draws the picture,
// because the box's own default is on and a page that hid every logo whenever
// it could not ask would turn the switch on for nobody and off for everybody.
//
// Asked through the store, so a list of two hundred rows asks once and a
// second screen asks not at all, and asked by the first picture that is drawn
// rather than when the page starts, so that a screen with no logo on it never
// sends the request.
//
// WHAT THIS CANNOT DO. A caller that may not read the switch draws the
// pictures whatever the switch says. The durable answer to that is the picture
// route honouring it, where every caller meets it at the same place; until
// then this is as far as a page can get on its own.
import { html, useState, useEffect } from '../runtime.js';
import * as store from '../store.js';
import { monogram } from './onair.js';

/**
 * Whether channel pictures are drawn at all, out of what the box said about
 * its web server.
 *
 * The member is read off the answer rather than through its declared shape,
 * because a box older than the switch answers this route without it, and the
 * only reading that is right for both is the one that treats an absent member
 * the way the box treats an absent line, which is as the default.
 *
 * @param {Web.Snapshot<Api.Webserver>} seen
 * @returns {boolean}
 */
export function drawsLogos(seen) {
	if (seen.state !== store.READY || !seen.data)
		return true;
	const said = /** @type {Record<string, unknown>} */ (/** @type {unknown} */ (seen.data));
	return said.channel_logos !== false;
}

/**
 * @param {{ channel: { id: string, name: string }, size?: 'sm' | 'md' }} props
 * @returns {Web.Drawn}
 */
export function Logo(props) {
	const channel = props.channel;
	const [allowed, setAllowed] = useState(true);
	/* A picture the box does not have for this channel is a 404, which a
	   browser draws as a broken image, and the initials take over the moment
	   the answer says there is none.

	   WHICH CHANNEL FAILED AND NOT WHETHER ONE DID. A 404 already in the
	   browser's cache fires its error before the first effect of this
	   component runs, so an effect that cleared a flag on the way in would
	   clear the answer that had just arrived, and no second error ever comes
	   for an address that has not changed. Held against the channel instead,
	   the answer needs no clearing: it stops applying when the channel does. */
	const [failed, setFailed] = useState('');
	const [shown, setShown] = useState('');
	const broken = failed === channel.id;

	useEffect(function () {
		return store.watch('GET', '/api/v1/system/webserver', null, function (seen) {
			setAllowed(drawsLogos(seen));
		});
	}, []);

	const kind = props.size === 'sm' ? 'logo sm' : 'logo';
	if (!allowed || broken)
		return html`<span class=${kind + ' logo-word'} aria-hidden="true">${monogram(channel.name)}</span>`;

	/* The initials first and the picture over them, rather than the picture
	   alone. Nothing the box says about a channel tells whether it has one, so
	   the page finds out by asking, and an element drawn before that answer
	   arrives is an empty frame for as long as the asking takes. On a list of
	   three hundred channels over somebody's home line that is not a flicker.
	   The picture is still fetched lazily, which opacity does not prevent and
	   display would. */
	return html`<span
		class=${kind + ' logo-word'}
		data-pic=${shown === channel.id ? 'one' : null}
		aria-hidden="true">
		<span class="logo-mg">${monogram(channel.name)}</span>
		<img
			class="logo-pic"
			src=${'/api/v1/channels/' + encodeURIComponent(channel.id) + '/logo'}
			alt=""
			loading="lazy"
			onLoad=${function () { setShown(channel.id); }}
			onError=${function () { setFailed(channel.id); }} />
	</span>`;
}

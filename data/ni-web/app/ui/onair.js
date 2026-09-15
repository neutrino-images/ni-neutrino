// What the box is playing, in the corner of every screen.
//
// Read and never guessed: the channel is the one the box says it is playing,
// the programme is the one the guide says is on that channel now, and the red
// dot is there while the box is writing to a disc and at no other time.
//
// THE COLOUR IS THE POINT OF THIS FILE. What is playing is the accent, under
// its own name in the tokens. It had been the red, and then a page with a
// recording running and a page without one were the same red in two places
// that meant two different things. The red here is the dot and nothing else.
//
// It is a link and not a decoration: what is playing is the one thing on the
// page that is always about somewhere, and that somewhere is the overview.
import { html, useState, useEffect } from '../runtime.js';
import { t } from '../i18n.js';
import text from '../shell.text.js';
import { hrefFor } from '../nav.js';
import { Dot } from './dot.js';

/* The share of the programme that has gone, as a whole number. Nought where
   the guide said nothing, so a bar drawn from this is empty rather than absent
   and the element keeps its height either way.

   now is handed in rather than read here, so that the same event and the same
   moment always produce the same number: a function that read the clock could
   not be held to an answer. */
/**
 * @param {{ start?: number, duration?: number } | null | undefined} event
 * @param {number} now seconds since the epoch
 * @returns {number} nought to a hundred
 */
export function elapsedShare(event, now) {
	if (!event)
		return 0;
	const start = Number(event.start);
	const runs = Number(event.duration);
	if (!Number.isFinite(start) || !Number.isFinite(runs) || runs <= 0)
		return 0;
	const share = Math.round(((now - start) / runs) * 100);
	return share < 0 ? 0 : (share > 100 ? 100 : share);
}

/* The two or three letters a channel is drawn with where it has no picture.
   The initials of its words, which is what tells ARD Alpha from ARD One in a
   26 pixel box. */
/**
 * @param {string} name
 * @returns {string}
 */
export function monogram(name) {
	const words = String(name || '').split(/[^A-Za-z0-9À-ÿ]+/).filter(function (one) { return one !== ''; });
	if (words.length === 0)
		return '';
	if (words.length === 1)
		return (words[0] || '').slice(0, 2).toUpperCase();
	return words.slice(0, 3).map(function (one) { return one.slice(0, 1); }).join('').toUpperCase();
}

/**
 * @param {{
 *   channel: Api.Channel | null,
 *   event?: { title?: string, start?: number, duration?: number } | null,
 *   recordings?: number,
 *   now?: number
 * }} props
 * @returns {Web.Drawn}
 */
export function OnAir(props) {
	const channel = props.channel;
	const recordings = props.recordings || 0;
	/* A picture the box does not have for this channel is a 404, which a
	   browser draws as a broken image. Asked for once, and the monogram takes
	   over the moment the answer says there is none. */
	const [broken, setBroken] = useState(false);

	useEffect(function () {
		setBroken(false);
	}, [channel ? channel.id : '']);

	if (!channel) {
		return html`<span class="onair" data-on-air="none">
			<span class="onair-meta"><span class="onair-name">${t(text, 'shell.status.nochannel')}</span></span>
		</span>`;
	}

	const share = elapsedShare(props.event, props.now || Math.floor(Date.now() / 1000));
	const title = props.event && props.event.title ? props.event.title : '';

	return html`<a
		class="onair"
		data-on-air="one"
		href=${hrefFor('now', 'overview')}
		title=${channel.name}>
		${broken
			? html`<span class="onair-logo onair-monogram" aria-hidden="true">${monogram(channel.name)}</span>`
			: html`<img
				class="onair-logo"
				src=${'/api/v1/channels/' + encodeURIComponent(channel.id) + '/logo'}
				alt=""
				onError=${function () { setBroken(true); }} />`}
		<span class="onair-meta">
			<span class="onair-name">${channel.name}</span>
			<span class="onair-event">
				${title
					? t(text, 'shell.onair.event', { title: title, percent: share })
					: t(text, 'shell.onair.noevent')}
			</span>
		</span>
		${recordings
			? html`<${Dot}
				kind="recording"
				word=${recordings > 1
					? t(text, 'shell.status.recordings', { count: recordings })
					: t(text, 'shell.status.recording')} />`
			: null}
	</a>`;
}

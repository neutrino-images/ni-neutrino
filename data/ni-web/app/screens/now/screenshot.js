// What the box is drawing, fetched as a picture.
//
// Two blocks and no screen of its own. Driving a box you cannot see is flying blind, so
// the picture stands beside the keys on one screen. It arrives as two drawings and not
// as one block, because the screen pins the picture over the keys on a telephone and
// puts what decides what is in it somewhere else entirely; see useCapture.
//
// The preview asks for a JPEG and the button under it for a PNG. The two are the same
// capture in two forms and the difference is a factor on the wire and on the processor.
//
// The picture is an address and not bytes this page holds. The document allows an image
// from this origin and from a data address, and nothing else; an address made out of
// fetched bytes is of a third kind and the document would refuse it. Fetching into this
// page would also mean holding a megabyte of it until something let it go.
//
// WHICH MEANS EVERY CAPTURE NEEDS AN ADDRESS OF ITS OWN. A fresh element carrying an
// address the page has already fetched is answered out of the copy the browser holds
// for the life of the document, and no-store does not reach that copy: measured on
// Chromium 152, three presses of the button that fetches a fresh capture put nothing on
// the wire and the element showed a picture minutes old. So the round is part of the
// address; the box does not read it.
//
// WHAT COUNTS AS ASKING. A capture is real work for a box with an ARM11, so nothing
// here fetches on a timer of its own. The screen around this hands down a number, and
// every time that number changes this fetches once; see remote.js.
import { html, useState, useEffect, useRef } from '../../runtime.js';
import { buildUrl } from '../../api.js';
import { t } from '../../i18n.js';
import text from './now.text.js';
import { Button } from '../../ui/button.js';
import { Switch } from '../../ui/switch.js';
import { Card, useOnScreen } from './parts.js';

// How often the preview is fetched again while it is asked to do so by itself
// and while it can be seen. A capture costs the box real work, so this is the
// rate of somebody watching and not the rate of a video.
const AGAIN_EVERY_MS = 5000;

/**
 * One capture, asked for in the form the caller wants.
 *
 * The address is spelt by the one place that spells addresses, out of the document's
 * own path, so a route renamed on the server is caught by the check that reads this
 * file rather than by a broken picture.
 *
 * WHICH CAPTURE IS PART OF THE ADDRESS. A browser answers a second image element
 * carrying an address it has already fetched out of the copy it holds in the page, and
 * neither no-store nor a fresh element changes that: measured on Chromium 152, three
 * presses of the button below put nothing at all on the wire. The box does not read the
 * number; it is there so two captures are two addresses.
 *
 * @param {boolean} osd
 * @param {boolean} video
 * @param {string} format
 * @param {number} round which capture this is, counting from one
 * @returns {string}
 */
export function pictureUrl(osd, video, format, round) {
	return buildUrl('/api/v1/osd/screenshot', null, {
		osd: osd, video: video, format: format, at: round,
	});
}

/**
 * The television picture, as the two pieces a screen places separately: what is drawn,
 * and what decides what is drawn.
 *
 * TWO PIECES AND ONE STATE, which is why this is a hook and not a component. On a
 * telephone the picture is pinned over the keys and its options are not, and they are
 * not even next to it: a button and a fold pinned with the picture are lines of a six
 * hundred pixel reading spent on what nobody touches while walking a menu, and a card
 * of them between the picture and the first key is scrolling done before reaching
 * anything worth pressing. A component cannot draw itself into two places.
 *
 * BOTH LAYERS TO BEGIN WITH, and the layer the box draws is the one that must not be
 * turned off: somebody pressing keys is looking for the menu they are standing in. The
 * picture underneath is the dearer of the two, and it is still on, because with no menu
 * open the drawn layer is empty and a black rectangle tells nobody anything.
 *
 * @param {number} bump a number that, whenever it changes, is one capture;
 *        nought means nothing has been asked for yet
 * @returns {{ seen: Web.Drawn, options: Web.Drawn }}
 */
export function useCapture(bump) {
	const box = useRef(/** @type {HTMLElement | null} */ (null));
	const watching = useOnScreen(box);
	const [osd, setOsd] = useState(true);
	const [video, setVideo] = useState(true);
	const [again, setAgain] = useState(false);
	/* Nought means nothing has been fetched yet, and that is what the frame
	   says instead of showing a picture. */
	const [round, setRound] = useState(0);
	const [failed, setFailed] = useState(false);
	/* The round whose picture arrived, and whether what is on screen is that
	   older round rather than the newest.

	   A CAPTURE CAN BE TURNED DOWN AND NOTHING BE WRONG. The box takes one
	   picture of its screen at a time and tells a second asker it is busy rather
	   than queueing it behind a driver read that has no deadline. The element
	   below cannot read that answer, it only sees an error, so the first error
	   falls back to the last picture that did arrive, which is a moment old and
	   is what somebody driving the box is looking at anyway. Only a round with
	   nothing left to fall back on says so in words. */
	const [shown, setShown] = useState(0);
	const [stale, setStale] = useState(false);
	const nothing = !osd && !video;
	const at = stale ? shown : round;

	// One capture per raise of the number above, which is the whole of what
	// the screen around this asks for.
	useEffect(function () {
		if (bump <= 0 || nothing) {
			return;
		}
		forget();
		setRound(function (n) { return n + 1; });
	}, [bump]);

	useEffect(function () {
		if (!again || !watching || nothing) {
			return undefined;
		}
		const timer = window.setInterval(function () {
			setRound(function (n) { return n + 1; });
		}, AGAIN_EVERY_MS);
		return function () { window.clearInterval(timer); };
	}, [again, watching, nothing]);

	/**
	 * @param {boolean} next
	 * @returns {void}
	 */
	function chooseOsd(next) {
		setOsd(next);
		forget();
		fetchAgain();
	}

	/**
	 * @param {boolean} next
	 * @returns {void}
	 */
	function chooseVideo(next) {
		setVideo(next);
		forget();
		fetchAgain();
	}

	/* Only where one has been fetched already. Turning a switch on a frame
	   that is still saying nothing has been fetched is not the same as asking
	   for one. */
	function fetchAgain() {
		setRound(function (n) { return n === 0 ? 0 : n + 1; });
	}

	/**
	 * What every fresh ask clears, so a round is judged on its own answer and
	 * not on the one before it.
	 *
	 * @returns {void}
	 */
	function forget() {
		setFailed(false);
		setStale(false);
	}

	/**
	 * A picture that did not arrive. The first one falls back to the last that
	 * did; a second, with that fallback gone as well, is the case where there is
	 * nothing left to show.
	 *
	 * @returns {void}
	 */
	function missed() {
		if (shown > 0 && !stale) {
			setStale(true);
			return;
		}
		setFailed(true);
	}

	/* Pressing the picture makes it bigger, pressing it again puts it back.

	   IT COSTS THE BOX NOTHING. The capture arrives at the size the box drew
	   it, which is far more than a telephone shows: the bytes for the larger
	   picture are already here, and growing it never asks for another. A
	   capture is real work for the box, and this is not one.

	   Only where the picture is pinned over the keys. Wide it already has the
	   room it needs, and a control that changes nothing is worse than none. */
	const [large, setLarge] = useState(false);

	return {
		seen: html`<div class="now-shot now-shot-live" data-large=${large ? 'one' : null} ref=${box}>
			${nothing
				? html`<p class="now-empty">${t(text, 'now.shot.empty')}</p>`
				: round === 0
					? html`<p class="now-empty">${t(text, 'now.shot.waiting')}</p>`
					: failed
						? html`<p class="now-empty" role="status">${t(text, 'now.shot.failed')}</p>`
						: html`<button
							type="button"
							class="now-shot-zoom"
							aria-pressed=${large ? 'true' : 'false'}
							title=${t(text, large ? 'now.shot.smaller' : 'now.shot.bigger')}
							onClick=${function () { setLarge(!large); }}>
							<img
								key=${'shot-' + at}
								class="now-shot-image"
								src=${pictureUrl(osd, video, 'jpeg', at)}
								alt=${t(text, 'now.shot.alt')}
								onLoad=${function () { setShown(at); }}
								onError=${missed} />
							<span class="sr">${t(text, large ? 'now.shot.smaller' : 'now.shot.bigger')}</span>
						</button>`}
		</div>`,
		options: html`<div class="now-shot-options">
			<p class="now-buttons">
				<${Button}
					primary=${true}
					disabled=${nothing}
					onClick=${function () {
						forget();
						setRound(function (n) { return n + 1; });
					}}>${t(text, 'now.shot.refresh')}<//>
			</p>
			${/* FOLDED, AND THE ONE BUTTON ABOVE IS NOT. This picture shares a
			     screen with the keys and a telephone is 390 pixels wide: three
			     switches, a second button and a sentence unfolded is a card the
			     length of a screen for what nobody presses while walking a
			     menu. Fetching again is the one of them somebody reaches for
			     while driving the box. */''}
			<details class="now-shot-more">
				<summary class="now-sub">${t(text, 'now.shot.more')}</summary>
				<${Switch}
					label=${t(text, 'now.shot.osd')}
					checked=${osd}
					onChange=${function (/** @type {{ currentTarget: HTMLInputElement }} */ e) { chooseOsd(e.currentTarget.checked); }} />
				<${Switch}
					label=${t(text, 'now.shot.video')}
					checked=${video}
					onChange=${function (/** @type {{ currentTarget: HTMLInputElement }} */ e) { chooseVideo(e.currentTarget.checked); }} />
				<${Switch}
					label=${t(text, 'now.shot.auto')}
					checked=${again}
					disabled=${nothing}
					onChange=${function (/** @type {{ currentTarget: HTMLInputElement }} */ e) { setAgain(e.currentTarget.checked); }} />
				<p class="now-buttons">
					${nothing
						? null
						: html`<a
							class="btn"
							href=${pictureUrl(osd, video, 'png', at)}
							download="neutrino.png">${t(text, 'now.shot.download')}</a>`}
				</p>
				<p class="now-hint">${t(text, 'now.shot.hint')}</p>
			</details>
		</div>`,
	};
}

/**
 * The little display on the front of the box.
 *
 * Nothing is fetched here until the button is pressed: whoever is driving the box from
 * a browser is looking at the television and not at the two lines on its front.
 *
 * @returns {Web.Drawn}
 */
export function Display() {
	const [round, setRound] = useState(0);
	const [failed, setFailed] = useState(false);

	return html`<${Card} title=${t(text, 'now.shot.card.display')}>
		<div class="now-shot now-shot-small">
			${round === 0
				? html`<p class="now-empty">${t(text, 'now.shot.waiting')}</p>`
				: failed
					? html`<p class="now-empty" role="status">${t(text, 'now.shot.failed')}</p>`
					: html`<img
						key=${'display-' + round}
						class="now-shot-image"
						src=${buildUrl('/api/v1/osd/display/screenshot', null, { at: round })}
						alt=${t(text, 'now.shot.displayalt')}
						onError=${function () { setFailed(true); }} />`}
		</div>
		<p class="now-buttons">
			<${Button}
				onClick=${function () {
					setFailed(false);
					setRound(function (n) { return n + 1; });
				}}>${t(text, 'now.shot.refresh')}<//>
		</p>
	<//>`;
}

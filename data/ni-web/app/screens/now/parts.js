// The pieces the three screens of this destination share.
//
// Kept here and not one level up: a building block with one user is a building
// block nobody has seen a second use of yet, and a file ten streams write is
// worse than three files one stream writes. Whatever a second destination
// wants out of this is lifted once, later, by somebody who can see both.
import { html, useState, useEffect, useId } from '../../runtime.js';
import * as store from '../../store.js';
import { State } from '../../ui/state.js';

/* What a question nobody is asking yet looks like. Frozen, because it is
   handed to every caller that is waiting for something it needs before it can
   ask, and one of them writing into it would change what the others see. */
const IDLE = Object.freeze({
	state: 'empty', phase: '', data: null, error: null, at: 0, url: '',
});

/**
 * One address the box answers, watched for as long as the screen is on screen.
 *
 * The address is the dependency and not the object the call site wrote: a screen
 * builds its options afresh on every draw, so an effect keyed on the object would
 * unsubscribe and subscribe again on every one of them.
 *
 * asking says whether the question can be put at all. A route whose query is
 * required cannot be asked before the value for it is known. Written as an
 * argument rather than as a condition at the call site, because a hook inside an
 * if is a hook that runs in a different order on the next draw.
 *
 * @template {string} M
 * @template {string} P
 * @param {M} method
 * @param {P} path
 * @param {Api.Call<`${Uppercase<M>} ${P}`, 'json'> | null} [options]
 * @param {boolean} [asking]
 * @returns {Web.Snapshot<Api.Result<`${Uppercase<M>} ${P}`>>}
 */
export function useResource(method, path, options, asking) {
	const wanted = asking === undefined ? true : asking;
	const now = wanted
		? store.read(method, path, options)
		: /** @type {Web.Snapshot<Api.Result<`${Uppercase<M>} ${P}`>>} */ (IDLE);
	const [held, setHeld] = useState(now);

	useEffect(function () {
		if (!wanted) {
			return undefined;
		}
		return store.watch(method, path, options || null, setHeld);
	}, [now.url, wanted]);

	/* The draw in which the address changed happens before the effect that
	   subscribes to it, so for that one draw what is held is the answer to the
	   question before. Drawn as what the store knows about the new address,
	   which is nothing, rather than as the old channel's programme under the
	   new channel's name. */
	return held.url === now.url ? held : now;
}

/**
 * A refusal, in the three members the shared failure box draws.
 *
 * WITHOUT THE LINK, AND MEASURED RATHER THAN ASSUMED: this server answers every
 * address under /errors/ with 404, its own refusal included, so the place a
 * problem names is a place nothing is served from.
 *
 * It also costs more than a dead link. What a page offers is read off its links
 * by their first path segment, and /errors/no-running-channel adds a segment that
 * is not one of the seven destinations, so a screen drawing it makes the document
 * look like it has an eighth.
 *
 * @param {Web.Failure | null} error
 * @returns {{ title: string, detail: string, href: string } | null}
 */
export function drawableProblem(error) {
	if (!error) {
		return null;
	}
	const problem = error.problem;
	return {
		title: problem.title,
		detail: problem.detail,
		href: '',
	};
}

/**
 * One card of this destination, with its own heading and its own failure.
 *
 * Its own, and that is the point of the shape: the tuner readings are the one
 * thing on this screen that is asked for over and over, and a box that turns that
 * question down must colour the card it belongs to and leave the rest standing.
 *
 * wide is the whole row and double is two columns of it, which are the same thing
 * wherever the grid has two columns and are not where it has four.
 *
 * @param {{ title: string, snapshot?: Web.Snapshot<unknown> | undefined,
 *           empty?: string | undefined, onRetry?: (() => void) | undefined,
 *           wide?: boolean | undefined, double?: boolean | undefined,
 *           lead?: boolean | undefined, children?: unknown }} props
 * @returns {Web.Drawn}
 */
export function Card(props) {
	const id = useId();
	const snapshot = props.snapshot;
	const problem = snapshot ? drawableProblem(snapshot.error) : null;
	let cls = 'now-card';
	if (props.wide) {
		cls += ' now-wide';
	}
	if (props.double) {
		cls += ' now-double';
	}
	if (props.lead) {
		cls += ' now-lead';
	}

	return html`<section
		class=${cls}
		aria-labelledby=${id}>
		<h2 id=${id} class="now-card-title">${props.title}</h2>
		<${State}
			phase=${snapshot ? snapshot.phase : ''}
			problem=${problem}
			empty=${props.empty}
			onRetry=${props.onRetry}>${props.children}<//>
	</section>`;
}

/**
 * A measured value against the scale that came with it.
 *
 * A bar and a number, never one of the two: the bar is what is read across the
 * room and the number is what is read by somebody the bar tells nothing. The
 * element is the one the browser already announces as a progress.
 *
 * @param {{ label: string, value: number, max: number, text: string }} props
 * @returns {Web.Drawn}
 */
export function Meter(props) {
	const id = useId();
	const max = props.max > 0 ? props.max : 1;
	const value = props.value < 0 ? 0 : (props.value > max ? max : props.value);

	return html`<div class="now-meter">
		<label class="now-meter-label" for=${id}>${props.label}</label>
		<span class="now-meter-value">${props.text}</span>
		<progress id=${id} class="now-bar" max=${max} value=${value}></progress>
	</div>`;
}

/**
 * Whether this card is worth asking the box about: on screen, and in a window
 * somebody is looking at.
 *
 * There is no event for a tuner reading, so the only way to draw one is to ask
 * again and again, and a screen left open in a background tab asking four worker
 * threads for it every few seconds is felt on the box. What is watched is the
 * element itself, so a card scrolled past on a narrow screen stops costing
 * anything.
 *
 * @param {{ current: Element | null }} box
 * @returns {boolean}
 */
export function useOnScreen(box) {
	/* True until something says otherwise. A browser that answers nothing
	   about visibility is a browser this keeps asking in, because a reading
	   that is never taken looks exactly like a tuner that answers nothing. */
	const [seen, setSeen] = useState(true);

	useEffect(function () {
		const node = box.current;
		if (!node || typeof IntersectionObserver !== 'function') {
			return undefined;
		}

		let intersecting = true;

		function settle() {
			setSeen(intersecting && document.visibilityState !== 'hidden');
		}

		const watcher = new IntersectionObserver(function (entries) {
			for (const entry of entries) {
				intersecting = entry.isIntersecting;
			}
			settle();
		});
		watcher.observe(node);
		document.addEventListener('visibilitychange', settle);

		return function () {
			watcher.disconnect();
			document.removeEventListener('visibilitychange', settle);
		};
	}, [box]);

	return seen;
}

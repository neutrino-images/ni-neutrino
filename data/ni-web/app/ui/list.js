// The long list, which holds a window and a cursor and knows the two belong together.
//
// Not a luxury: the channel lists this box is shipped with hold 1238 and 2664 entries,
// and the route that answers for them hands out at most 500 at a time with a cursor
// for the rest. A list that drew every row it has would draw several thousand
// elements, and one that forgot the cursor would silently stop at the first page.
//
// Two spacers are what let a thousand rows cost the height of the ones on screen: the
// box scrolls over the full height and only the rows in the window are elements.
import { html, useState, useRef, useEffect } from '../runtime.js';

// How many rows above and below the window are drawn anyway, so that a scroll
// of one row does not arrive at an empty strip.
const OVERSCAN = 4;

// How close to the end of what is in hand the window may come before the next
// page is asked for. Half a screen, so the page is on its way before the
// scrollbar reaches the bottom.
const AHEAD = 10;

// Rows in hand and not rows that exist: with a cursor the second number is
// unknown, and a height computed from a guess jumps when the guess is wrong.
// The one extra row of padding below is what keeps the scrollbar inviting when
// there is more to come.
/**
 * @typedef {object} ListWindow
 * @property {number} first the first row of the window
 * @property {number} count how many of them are drawn
 * @property {number} padTop the height of the spacer above, in pixels
 * @property {number} padBottom and below
 */

/**
 * @param {{
 *   total: number,
 *   rowHeight: number,
 *   viewport?: number,
 *   scrollTop?: number,
 *   overscan?: number,
 *   hasMore?: boolean
 * }} opts
 * @returns {ListWindow}
 */
export function windowFor(opts) {
	const rowHeight = opts.rowHeight > 0 ? opts.rowHeight : 1;
	const total = opts.total > 0 ? opts.total : 0;
	const overscan = opts.overscan === undefined ? OVERSCAN : opts.overscan;
	const visible = Math.ceil((opts.viewport || 0) / rowHeight);

	let first = Math.floor((opts.scrollTop || 0) / rowHeight) - overscan;
	if (first < 0)
		first = 0;

	let count = visible + overscan * 2;
	if (first + count > total)
		count = total - first;
	if (count < 0)
		count = 0;

	return {
		first: first,
		count: count,
		padTop: first * rowHeight,
		padBottom: (total - first - count) * rowHeight + (opts.hasMore ? rowHeight : 0)
	};
}

// Asked from the window and not from the scroll position, so a viewport tall
// enough to hold everything in hand asks for the next page without anybody
// scrolling.
/**
 * @param {ListWindow} view
 * @param {number} total rows in hand, which with a cursor is not rows that exist
 * @param {boolean | undefined} hasMore
 * @returns {boolean}
 */
export function needsMore(view, total, hasMore) {
	return !!hasMore && view.first + view.count + AHEAD >= total;
}

/**
 * @template T
 * @param {{
 *   rows?: readonly T[],
 *   rowHeight?: number,
 *   hasMore?: boolean,
 *   onMore?: () => void,
 *   rowKey?: (row: T) => string | number,
 *   row: (row: T, at: number) => Web.Drawn
 * }} props
 * @returns {Web.Drawn}
 */
export function List(props) {
	const rows = props.rows || [];
	const rowHeight = props.rowHeight || 36;
	/* Said rather than left to the argument: useRef(null) on its own settles
	   on a box that can only ever hold null, and every reach into it is then
	   a fault about a type nobody wrote. */
	const box = useRef(/** @type {HTMLDivElement | null} */ (null));
	const [scrollTop, setScrollTop] = useState(0);
	const [viewport, setViewport] = useState(0);

	// Measured and not guessed, and measured again when the window changes:
	// the height of this box is a layout answer and there is no number in the
	// source that knows it.
	useEffect(function () {
		function measure() {
			if (box.current)
				setViewport(box.current.clientHeight);
		}
		measure();
		window.addEventListener('resize', measure);
		return function () { window.removeEventListener('resize', measure); };
	}, []);

	const view = windowFor({
		total: rows.length,
		rowHeight: rowHeight,
		viewport: viewport,
		scrollTop: scrollTop,
		hasMore: props.hasMore
	});

	// After the draw and not during it: asking for the next page is a write to
	// somebody else's state, and a render that does that is a render that can
	// run twice and ask twice.
	const more = props.onMore;
	const wants = more && needsMore(view, rows.length, props.hasMore);
	useEffect(function () {
		if (wants)
			more();
	}, [wants, rows.length]);

	const shown = rows.slice(view.first, view.first + view.count);

	// The two spacers carry a height and nothing else. It is a measurement
	// taken at run time, so it is set on the element rather than named in the
	// stylesheet, where there is no number to write.
	return html`<div
		class="vlist"
		ref=${box}
		onScroll=${function (/** @type {Event} */ e) {
			setScrollTop(/** @type {HTMLDivElement} */ (e.currentTarget).scrollTop);
		}}>
		<div style=${{ height: view.padTop + 'px' }}></div>
		${shown.map(function (row, index) {
			const at = view.first + index;
			return html`<div
				class="row"
				style=${{ height: rowHeight + 'px' }}
				key=${props.rowKey ? props.rowKey(row) : at}>${props.row(row, at)}</div>`;
		})}
		<div style=${{ height: view.padBottom + 'px' }}></div>
	</div>`;
}

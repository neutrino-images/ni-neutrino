// A table, and what becomes of one on a telephone.
//
// Where there is room it scrolls sideways inside its own box rather than wrapping,
// because a page that scrolls sideways as a whole cannot be held in one hand. Below
// the phone width it is not a table at all: the head goes, each row becomes a card and
// each cell carries the word its column head was carrying. That fold is one rule in
// the frame's stylesheet rather than a decision each screen makes for itself.
//
// A column states where its content sits and this writes that on the head cell and on
// every cell of the column from the one statement. A head left over controls that sit
// right is a head that does not stand over its column, and it happened because two
// places said where the content of one column goes.
//
// Sorting is a button in the header cell and not a click on the cell: a click target
// that is not a control is one a keyboard never reaches.
import { html, Fragment, useEffect, useRef } from '../runtime.js';
import { t } from '../i18n.js';
import text from '../shell.text.js';

/* How far below the last row the next page is asked for. Enough that a reader
   going down the list does not meet the end of it, and near enough that a list
   nobody is reading asks for nothing. */
const AHEAD_PX = 200;

/**
 * @param {Web.Sort | null | undefined} sort
 * @param {string} column
 * @returns {Web.Sort}
 */
export function nextSort(sort, column) {
	if (!sort || sort.column !== column)
		return { column: column, dir: 'asc' };
	return { column: column, dir: sort.dir === 'asc' ? 'desc' : 'asc' };
}

/**
 * @template T
 * @typedef {object} Column
 * @property {string} id
 * @property {string} label
 * @property {(row: T) => unknown} cell
 * @property {boolean} [sortable]
 * @property {boolean} [mono] whether the column is drawn in a fixed width face
 * @property {'end'} [align] where the content of the column sits, left unless
 *   this says otherwise. The head takes it too.
 * @property {boolean} [wide] the one column that takes whatever width is over.
 *   Without it the spare width is spread across every column in proportion, and
 *   a clock four characters long ends up in three hundred pixels while the name
 *   beside it is cut. Only the column that says this needs to say anything.
 *
 *   Which is also where it goes wrong, so measure before saying it. A column
 *   falls back to the narrowest it can be, and for a cell that wraps that is one
 *   word, for a cell that ends in an ellipsis it is nothing at all. Say it on the
 *   last long column of a table whose other columns are short and do not wrap; do
 *   not say it where two columns both want the room.
 */

/**
 * The box the rows scroll inside, which is what a row coming up is coming up
 * inside.
 *
 * Not the window. The page keeps its own scrolling box, and a row below the edge
 * of that box is out of sight while still being inside the window, so an observer
 * left on the window sees nothing until the box is scrolled to the very end. The
 * margin that asks early is a margin on the root, and a clip between the root and
 * the row is not widened by it either.
 *
 * The first ancestor that really scrolls, and not the first that says it might:
 * the box the table sits in scrolls sideways, which makes it a candidate here
 * while being exactly as tall as the table.
 *
 * @param {HTMLElement} from
 * @returns {HTMLElement | null} null for the window, which is what an
 *          observer takes when nothing between here and the top scrolls yet
 */
function scrollRootOf(from) {
	let up = from.parentElement;
	while (up) {
		if (up.scrollHeight > up.clientHeight) {
			const how = getComputedStyle(up).overflowY;
			if (how === 'auto' || how === 'scroll')
				return up;
		}
		up = up.parentElement;
	}
	return null;
}

/**
 * A list that grows as it is read.
 *
 * The last row is watched, and the page after this one is asked for while it is
 * still coming up. Three things this does not do, each of which is a way of
 * getting it wrong:
 *
 * It does not ask again once the box has said there is nothing more. What says
 * that is hasMore, which is the cursor and not the length of a page: a page
 * shorter than the one asked for is not on that account the last.
 *
 * It does not disturb where somebody is reading. Rows arrive after the row being
 * watched, and nothing above it moves.
 *
 * It is not the only way. Whoever is on a keyboard never scrolls a row into view,
 * so a screen that offers this offers the control beside it as well.
 *
 * @param {{ current: HTMLElement | null }} hold
 * @param {number} rows
 * @param {string} lastKey what the last row is, so that a page replaced by
 *   another page of the same length is watched again rather than watched at the
 *   row that has gone
 * @param {boolean} hasMore
 * @param {(() => void) | undefined} onNearEnd
 * @returns {void}
 */
function useNearEnd(hold, rows, lastKey, hasMore, onNearEnd) {
	/* The handler is read out of a box rather than named in the dependencies:
	   a screen writes it fresh on every draw, and an observer rebuilt on every
	   draw fires again from a standing start each time. */
	const latest = useRef(onNearEnd);
	latest.current = onNearEnd;

	useEffect(function () {
		const table = hold.current;
		if (!table || !hasMore || !onNearEnd || rows === 0)
			return undefined;
		if (typeof IntersectionObserver !== 'function')
			return undefined;
		const body = table.querySelector('tbody');
		const last = body ? body.lastElementChild : null;
		if (!last)
			return undefined;
		const watch = new IntersectionObserver(function (seen) {
			for (const one of seen) {
				if (one.isIntersecting && latest.current)
					latest.current();
			}
		}, { root: scrollRootOf(table), rootMargin: '0px 0px ' + AHEAD_PX + 'px 0px' });
		watch.observe(last);
		return function () { watch.disconnect(); };
	}, [rows, lastKey, hasMore, onNearEnd === undefined]);
}

/**
 * @template T
 * @param {{
 *   columns?: ReadonlyArray<Column<T>>,
 *   rows?: readonly T[],
 *   sort?: Web.Sort | null,
 *   rowKey?: (row: T) => string | number,
 *   onAir?: (row: T) => boolean,
 *   onSort?: (sort: Web.Sort) => void,
 *   hasMore?: boolean,
 *   onNearEnd?: () => void,
 *   detail?: (row: T) => unknown
 * }} props onAir says which row the box is playing. The frame's stylesheet
 *   already draws tr[data-on-air] in the accent at both foldings, and this is
 *   what writes the attribute, so no screen reaches for the colour itself.
 *   onNearEnd, with hasMore, is a list that grows as it is read.
 *   detail is what opens under one row, drawn in a row of its own across every
 *   column and left out for a row that answers nothing. It is here and not in
 *   a panel under the table because what it is about is the row: a player drawn
 *   below twenty rows is a player nobody sees press play.
 * @returns {Web.Drawn}
 */
export function Table(props) {
	const columns = props.columns || [];
	const rows = props.rows || [];
	const sort = props.sort;
	const hold = useRef(/** @type {HTMLElement | null} */ (null));

	/* What the last row is, so that a page replaced by another page of the same
	   length is watched again rather than watched at the row that has gone. */
	let lastKey = String(rows.length);
	for (const row of rows.slice(-1)) {
		if (props.rowKey)
			lastKey = String(props.rowKey(row));
	}
	useNearEnd(hold, rows.length, lastKey, props.hasMore === true, props.onNearEnd);

	return html`<div class="scroll-x">
		<table class="grid" ref=${hold}>
			<thead><tr>
				${columns.map(function (column) {
					const active = sort && sort.column === column.id;
					return html`<th
						key=${column.id}
						scope="col"
						data-align=${column.align || null}
						data-fit=${column.wide ? 'wide' : null}
						aria-sort=${active && sort ? (sort.dir === 'asc' ? 'ascending' : 'descending') : null}>
						${column.sortable && props.onSort
							? html`<button
								onClick=${function () { if (props.onSort) props.onSort(nextSort(sort, column.id)); }}
								aria-label=${t(text, 'shell.sort', { column: column.label })}>
								${column.label}${active && sort ? (sort.dir === 'asc' ? ' ↑' : ' ↓') : ''}
							</button>`
							: column.label}
					</th>`;
				})}
			</tr></thead>
			<tbody>
				${rows.map(function (row, index) {
					const key = props.rowKey ? props.rowKey(row) : index;
					const more = props.detail ? props.detail(row) : null;
					/* The key is on the pair and not on the row, so that the two
					   move together and a row that opens does not hand its open
					   part to the row above it. */
					return html`<${Fragment} key=${key}>
						<tr data-on-air=${props.onAir && props.onAir(row) ? 'one' : null}>
							${columns.map(function (column) {
								return html`<td
									key=${column.id}
									class=${column.mono ? 'mono' : null}
									data-align=${column.align || null}
									data-label=${column.label}>${column.cell(row)}</td>`;
							})}
						</tr>
						${more
							? html`<tr class="grid-more"><td colspan=${columns.length}>${more}</td></tr>`
							: null}
					<//>`;
				})}
			</tbody>
		</table>
	</div>`;
}

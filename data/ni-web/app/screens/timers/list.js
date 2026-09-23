/* Every timer the box holds, and the way to the one screen that changes one.
 *
 * The daemon answers fourteen fields for each of eight kinds, and two of those fields are
 * its own numbering rather than anything a person says: repeat and state reach this page as
 * numbers on purpose, and the reading of them belongs to the layer that knows the daemon,
 * which is fmt.js.
 *
 * A kind this build has no word for is drawn and not offered for changing. A timer file
 * written by another image can name one, the read side keeps such a timer rather than
 * dropping it, and a form that offered to edit it would be offering fields nothing here
 * knows it has.
 *
 * Making and changing is this screen with a third part in the address.
 */

import { html, useState, Link, route } from '../../runtime.js';
import * as store from '../../store.js';
import * as session from '../../session.js';
import { t } from '../../i18n.js';
import { clock, dateOf, dayAndClock, duration, timerRepeat, timerState } from '../../fmt.js';
import { hrefFor } from '../../nav.js';
import { Table } from '../../ui/table.js';
import { RowActions } from '../../ui/actions.js';
import { Dialog } from '../../ui/dialog.js';
import { StateChip } from '../../ui/dot.js';
import { State } from '../../ui/state.js';
import { toast } from '../../ui/toast.js';
import text from './list.text.js';
import { isKnownKind, useAnswer } from './list.model.js';
import { TimerForm, kindLabel } from './form.js';

export const css = '/app/screens/timers/list.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'timers.head'); }


// The same page of channels the form offers, so that a timer names a channel
// and not a sixteen digit number. One question for both screens, because the
// store keys an answer by the address it was asked for.
const CHANNEL_PAGE = 500;

/**
 * @param {string} value
 * @returns {boolean}
 */
function isNumber(value) {
	if (value === '')
		return false;
	for (const c of value) {
		if (c < '0' || c > '9')
			return false;
	}
	return true;
}

/**
 * The state of a timer as a mark beside the word for it. The mark is a condition
 * of the box and borrows no meaning from the word beside it: a recording that is
 * running is the red one, anything else running is the green one, and a timer the
 * daemon gave up on carries the warning.
 * @param {Api.Timer} timer
 * @returns {string}
 */
export function markFor(timer) {
	if (timer.state === 2)
		return (timer.kind === 'record' || timer.kind === 'immediate-record') ? 'recording' : 'running';
	if (timer.state === 4)
		return 'warning';
	return '';
}

/**
 * The outline the pill around that mark is drawn with, which is not the same
 * question. A timer that is waiting wears the neutral pill; the red one has no
 * outline of its own on purpose, because the mark inside it is already the red
 * and a second red around the word would be the loudest row on the screen twice
 * over.
 * @param {Api.Timer} timer
 * @returns {string}
 */
export function toneFor(timer) {
	const mark = markFor(timer);
	if (mark === 'running')
		return 'good';
	if (mark === 'warning')
		return 'warn';
	return '';
}

/**
 * How often it still runs, in words, for a timer that repeats at all.
 * @param {Api.Timer} timer
 * @returns {string}
 */
export function repeatWords(timer) {
	const how = timerRepeat(timer.repeat);
	if (timer.repeat === 0)
		return how;
	return how + ', ' + (timer.repeat_count === 0
		? t(text, 'timers.count.endless')
		: t(text, 'timers.count.times', { count: timer.repeat_count }));
}

/**
 * Whether two times fall on one day, where the browser is standing.
 *
 * Local and not the difference in seconds: a timer beginning at half past eleven
 * and ending an hour later is on two days, and one beginning at one in the
 * afternoon and ending twenty three hours later is on two days as well. Only the
 * calendar the reader is in answers that.
 *
 * @param {number} from
 * @param {number} to
 * @returns {boolean}
 */
export function sameDay(from, to) {
	const a = dateOf(from);
	const b = dateOf(to);
	if (a === null || b === null)
		return false;
	return a.getFullYear() === b.getFullYear()
		&& a.getMonth() === b.getMonth()
		&& a.getDate() === b.getDate();
}

/**
 * @param {Api.Timer[]} rows
 * @param {{ column: string, dir: string } | null} sort
 * @returns {Api.Timer[]}
 */
export function sortRows(rows, sort) {
	const column = sort ? sort.column : 'start';
	const sign = sort && sort.dir === 'desc' ? -1 : 1;
	const out = rows.slice();
	out.sort(function (a, b) {
		if (column === 'kind' && a.kind !== b.kind)
			return a.kind < b.kind ? -sign : sign;
		if (a.start !== b.start)
			return a.start < b.start ? -sign : sign;
		// The daemon numbers from one and never hands the same number out twice,
		// so this is what keeps two timers of the same minute in one order
		// between two draws.
		return a.id < b.id ? -1 : 1;
	});
	return out;
}

/**
 * @param {{ items?: Array<{ id: string, name: string }> } | null} page
 * @param {string} id
 * @returns {string}
 */
function channelName(page, id) {
	if (id === '' || id === '0')
		return t(text, 'timers.nochannel');
	for (const one of (page && page.items) || []) {
		if (one.id === id)
			return one.name;
	}
	return id;
}

/**
 * @returns {Web.Drawn}
 */
function TimerList() {
	const held = useAnswer('GET', '/api/v1/timers');
	const channels = useAnswer('GET', '/api/v1/channels', { query: { mode: 'tv', limit: CHANNEL_PAGE } }, 'tv');

	const [sort, setSort] = useState(/** @type {Web.Sort} */ ({ column: 'start', dir: 'asc' }));
	const [asking, setAsking] = useState(/** @type {Api.Timer | null} */ (null));

	/* Asked for when the control is pressed and never before it.

	   This screen used to read what the request was granted and draw itself
	   around the answer: no column of controls at all below Write, and a
	   sentence above the table explaining the absence. That is the page saying no
	   on the box's behalf, to somebody who has not asked for anything yet. So the
	   controls are simply there, and pressing one raises the sheet if it has to;
	   the intention behind the press is not lost while that happens, because what
	   comes back is a promise.

	   The sheet closed without signing in rejects, and that is the person saying
	   no rather than the box. Nothing is said about it: they know. */
	/**
	 * @param {() => void} act
	 * @returns {void}
	 */
	function whenAllowed(act) {
		session.requireWrite().then(act, function () { });
	}

	/**
	 * @param {Api.Timer} timer
	 */
	async function remove(timer) {
		setAsking(null);
		try {
			await store.write('DELETE', '/api/v1/timers/{id}', {
				params: { id: timer.id },
				touches: ['/api/v1/timers'],
			});
			/* What this screen now believes, stated rather than asked for again:
			   the removal is answered with nothing at all, and a read placed
			   straight after it races the daemon. The event the box sends on a
			   change is what corrects this if it was wrong. */
			const rest = [];
			for (const one of (held.data && held.data.items) || []) {
				if (one.id !== timer.id)
					rest.push(one);
			}
			store.put('GET', '/api/v1/timers', null, { items: rest });
			toast(t(text, 'timers.deleted'));
		} catch (caught) {
			const failed = /** @type {{ problem?: { title: string, detail: string } } | null} */ (caught);
			toast(failed && failed.problem ? failed.problem.title : t(text, 'timers.notdeleted'), 'bad');
		}
	}

	const rows = sortRows((held.data && held.data.items) || [], sort);


	/** @type {Array<import('../../ui/table.js').Column<Api.Timer>>} */
	const columns = [
		{
			id: 'state', label: t(text, 'timers.col.state'), sortable: false, mono: false,
			cell: function (row) {
				return html`<${StateChip} kind=${markFor(row)} tone=${toneFor(row)} word=${timerState(row.state)} />`;
			},
		},
		{
			id: 'kind', label: t(text, 'timers.col.kind'), sortable: true, mono: false,
			cell: function (row) { return kindLabel(row.kind); },
		},
		{
			id: 'what', label: t(text, 'timers.col.what'), sortable: false, mono: false, wide: true,
			cell: function (row) {
				return html`<span class="timers-what">
					<span>${channelName(channels.data, row.channel_id)}</span>
					${row.title ? html`<span class="timers-title">${row.title}</span>` : null}
				</span>`;
			},
		},
		{
			id: 'start', label: t(text, 'timers.col.start'), sortable: true, mono: true,
			cell: function (row) { return dayAndClock(row.start); },
		},
		{
			id: 'stop', label: t(text, 'timers.col.stop'), sortable: false, mono: true,
			cell: function (row) {
				if (!(row.stop > row.start))
					return '';
				/* Two lines and not one. The table keeps every row on one line,
				   and this cell carrying both the clock and the length was the
				   widest of the eight: it pushed the table past a window of
				   fourteen hundred, and what went over the edge was the column
				   the marks are in. Two lines were not enough on their own: the
				   date in this cell is the date in the cell beside it, and the
				   table was still sixty seven pixels wider than the window. A
				   timer that ends on the day it begins says the day once. */
				return html`<span class="timers-what">
					<span>${sameDay(row.start, row.stop) ? clock(row.stop) : dayAndClock(row.stop)}</span>
					<span class="timers-title">${duration(row.stop - row.start)}</span>
				</span>`;
			},
		},
		{
			id: 'repeat', label: t(text, 'timers.col.repeat'), sortable: false, mono: false,
			cell: function (row) { return repeatWords(row); },
		},
		{
			id: 'dir', label: t(text, 'timers.col.dir'), sortable: false, mono: true,
			cell: function (row) {
				if (row.kind !== 'record' && row.kind !== 'immediate-record')
					return '';
				return row.recording_dir === '' ? t(text, 'timers.dir.default') : row.recording_dir;
			},
		},
	];

	/* Two marks beside a pointer and one control that opens a sheet beside a
	   finger, which is the shared part rather than a decision taken here: two
	   targets of eleven pixels each at the end of a row is one of them hit and
	   the other missed. Both forms are in the document and the stylesheet shows
	   one, so nothing here reads a width. */
	columns.push({
		id: 'change', label: t(text, 'timers.col.change'), sortable: false, mono: false, align: 'end',
		cell: function (row) {
			if (!isKnownKind(row.kind))
				return html`<span class="hint">${t(text, 'timers.foreign')}</span>`;
			return html`<span class="acts"><${RowActions}
				title=${kindLabel(row.kind) + ' ' + dayAndClock(row.start)}
				actions=${[
					{
						/* Not gated. Reading a form is a read, and the one act
						   behind it asks for itself when it is pressed. */
						id: 'edit', label: t(text, 'timers.edit'), mark: '\u270e',
						onAct: function () { route(hrefFor('timers', 'list', String(row.id))); },
					},
					{
						id: 'delete', label: t(text, 'timers.delete'), mark: '\u2715',
						onAct: function () { whenAllowed(function () { setAsking(row); }); },
					},
				]} /></span>`;
		},
	});

	return html`<div class="timers">
		<div class="timers-head">
			<${Link} class="btn primary" href=${hrefFor('timers', 'list', 'new')}>${t(text, 'timers.new')}<//>
		</div>

		<${State}
			phase=${held.phase}
			problem=${held.state === 'error' && held.error ? held.error.problem : null}
			empty=${held.state === 'ready' && rows.length === 0 ? t(text, 'timers.empty') : false}>
			<${Table}
				columns=${columns}
				rows=${rows}
				sort=${sort}
				onSort=${function (/** @type {Web.Sort} */ next) { setSort(next); }}
				rowKey=${function (/** @type {Api.Timer} */ row) { return row.id; }} />
		<//>

		<${Dialog}
			open=${asking !== null}
			title=${t(text, 'timers.confirm.title')}
			confirmLabel=${t(text, 'timers.delete')}
			onCancel=${function () { setAsking(null); }}
			onConfirm=${function () { if (asking !== null) remove(asking); }}>
			<p>${asking === null ? '' : t(text, 'timers.confirm.body', {
				what: kindLabel(asking.kind),
				when: dayAndClock(asking.start),
			})}</p>
		<//>
	</div>`;
}

/**
 * The destination, and which of its two screens the address asks for.
 * @param {{ param?: string }} props
 * @returns {Web.Drawn}
 */
export default function Timers(props) {
	const param = props.param || '';
	if (param === '')
		return html`<${TimerList} />`;
	if (param === 'new')
		return html`<${TimerForm} id="" />`;
	if (isNumber(param))
		return html`<${TimerForm} id=${param} />`;
	return html`<p class="note timers-notfound">${t(text, 'timers.notfound')}</p>`;
}

/* What the box has mounted and how much room is on each of them.
 *
 * Read and not System, which is the point of having this screen at all. Anybody the box
 * grants a read by their address alone sees this; what is in a directory is a level
 * higher. So this is the screen that answers for a caller who cannot open the listing.
 *
 * Free is not what is unallocated. The box answers how much a recording could still use,
 * which is less, because of the reserve the filesystem keeps back. Nothing here
 * recomputes it from the two other numbers.
 *
 * A counter of nought is not a filesystem of no size. It is the box saying it could not
 * read the counters, which is what every pseudo filesystem in the table answers. Those
 * are hidden by default rather than drawn as full or as empty. */

import { html, useState, useEffect, Link } from '../../runtime.js';
import * as store from '../../store.js';
import { t } from '../../i18n.js';
import { bytes, ratio } from '../../fmt.js';
import { State } from '../../ui/state.js';
import { StateChip } from '../../ui/dot.js';
import { Switch } from '../../ui/switch.js';
import { Table } from '../../ui/table.js';
import { hrefFor } from '../../nav.js';
import { dirHref } from './paths.js';
import text from './files.text.js';
import { problemOf } from './answer.js';

export const css = '/app/screens/files/files.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'files.mounts.here'); }


// Above this share of a filesystem the bar stops being decoration and starts
// being a warning, which is the one thing a person wants off this screen at a
// glance.
const TIGHT = 90;

/**
 * The mount points, watched. Exported because the listing screen offers them as
 * the places to begin, and two screens asking the same question share one request
 * through the store.
 *
 * @param {boolean} [ask]
 * @returns {Web.Snapshot<Api.MountList> | null}
 */
export function useMounts(ask) {
	const wanted = ask === undefined ? true : ask;
	const [shot, setShot] = useState(/** @type {Web.Snapshot<Api.MountList> | null} */ (null));
	useEffect(function () {
		if (!wanted) {
			setShot(null);
			return undefined;
		}
		return store.watch('GET', '/api/v1/storage/mounts', {}, setShot);
	}, [wanted]);
	return shot;
}

/**
 * @param {{ total: number, free: number }} row
 * @returns {number}
 */
export function usedShare(row) {
	// Worked out against the size the same answer states, so a filesystem whose
	// counters were unreadable answers nothing rather than a hundred per cent.
	if (!(row.total > 0)) {
		return -1;
	}
	return ratio(row.total - row.free, row.total);
}

/** @returns {Web.Drawn} */
export default function Mounts() {
	const shot = useMounts(true);
	const [mediaOnly, setMediaOnly] = useState(true);

	const all = (shot && shot.data && shot.data.items) || [];
	const rows = mediaOnly ? all.filter(function (one) { return one.total > 0; }) : all;
	const failed = shot && shot.error ? problemOf(shot.error) : null;

	const columns = [
		{
			id: 'state',
			label: t(text, 'files.mounts.col.state'),
			sortable: false,
			mono: false,
			/* Whether there is still room, which is the one question this screen
			   answers and the one thing somebody wants off it without reading a
			   row. Everything the box lists here is mounted, so mounted is not a
			   state this table has; what it has is three: room, nearly none, and
			   a filesystem whose counters could not be read at all. The third is
			   not nought bytes free, and is not drawn as though it were. */
			/** @param {Api.Mount} row */
			cell: function (row) {
				const share = usedShare(row);
				if (share < 0)
					return html`<${StateChip} kind="" word=${t(text, 'files.mounts.state.uncounted')} />`;
				return share >= TIGHT
					? html`<${StateChip} kind="warning" tone="warn" word=${t(text, 'files.mounts.state.tight')} />`
					: html`<${StateChip} kind="running" tone="good" word=${t(text, 'files.mounts.state.room')} />`;
			},
		},
		{
			id: 'point',
			label: t(text, 'files.mounts.col.point'),
			wide: true,
			sortable: false,
			mono: true,
			/** @param {Api.Mount} row */
			cell: function (row) {
				return html`<${Link} href=${dirHref(hrefFor('files', 'list'), row.id)}>${row.id}<//>`;
			},
		},
		{
			id: 'device',
			label: t(text, 'files.mounts.col.device'),
			sortable: false,
			mono: true,
			/** @param {Api.Mount} row */
			cell: function (row) { return row.device; },
		},
		{
			id: 'fstype',
			label: t(text, 'files.mounts.col.fstype'),
			sortable: false,
			mono: false,
			/** @param {Api.Mount} row */
			cell: function (row) { return row.fstype; },
		},
		{
			id: 'total',
			label: t(text, 'files.mounts.col.total'),
			sortable: false,
			mono: true,
			/** @param {Api.Mount} row */
			cell: function (row) { return row.total > 0 ? bytes(row.total) : t(text, 'files.unknown'); },
		},
		{
			id: 'free',
			label: t(text, 'files.mounts.col.free'),
			sortable: false,
			mono: true,
			/** @param {Api.Mount} row */
			cell: function (row) { return row.total > 0 ? bytes(row.free) : t(text, 'files.unknown'); },
		},
		{
			id: 'used',
			label: t(text, 'files.mounts.col.used'),
			sortable: false,
			mono: false,
			/** @param {Api.Mount} row */
			cell: function (row) {
				const share = usedShare(row);
				if (share < 0) {
					return t(text, 'files.unknown');
				}
				/* The number in the row and the bar beside it, and the bar is
				   hidden from anything that reads the row out: it says nothing
				   the text does not, and a second reading of one fact is noise
				   to whoever is listening rather than looking. The width is a
				   measurement and not a name, so it is set on the element. */
				return html`<span class="files-name">
					<span>${t(text, 'files.mounts.usage', {
						used: bytes(row.total - row.free), total: bytes(row.total),
					})}</span>
					<span class=${share >= TIGHT ? 'files-meter files-tight' : 'files-meter'} aria-hidden="true">
						<span style=${{ width: share + '%' }}></span>
					</span>
				</span>`;
			},
		},
	];

	return html`<div>
		<p class="files-hint">
			${t(text, 'files.mounts.note')}
			${' '}
			<${Link} href=${hrefFor('files', 'netfs')}>${t(text, 'files.mounts.tonetfs')}<//>
		</p>
		<div class="files-bar">
			<${Switch}
				label=${t(text, 'files.mounts.only')}
				hint=${t(text, 'files.mounts.only.hint')}
				checked=${mediaOnly}
				onChange=${function (/** @type {Web.On<HTMLInputElement>} */ event) { setMediaOnly(event.currentTarget.checked); }} />
		</div>
		<${State}
			problem=${failed}
			phase=${shot ? shot.phase : ''}
			empty=${shot && shot.state === store.READY && rows.length === 0 ? t(text, 'files.mounts.empty') : false}>
			<${Table}
				columns=${columns}
				rows=${rows}
				rowKey=${function (/** @type {Api.Mount} */ row) { return row.id; }} />
		<//>
	</div>`;
}

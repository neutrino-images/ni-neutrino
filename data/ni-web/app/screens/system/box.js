/* What the box is, and the four ways to take it off the air.

   Nothing here writes a setting, and that is a decision rather than an omission. Two of
   the settings this heading still carries elsewhere are settings only in appearance: the
   television offers a kernel switch that nothing in the tree reads, and the shipped web
   page writes a key for a recording file name that no consumer reads either, both listed
   with that reason in src/coreapi/settings/settingsundeclared.cpp.

   The four commands. Every one of them answers accepted and nothing more, and two of
   them the box will not answer at all afterwards.

   And why they are held back after a bouquet write. The channel service acknowledges a
   bouquet save once it has written it, which is not the same as the file system having
   it; taking the box down in that window is how a channel list is lost. */

import { html, useState, useEffect } from '../../runtime.js';
import * as store from '../../store.js';
import * as events from '../../events.js';
import { bytes, duration, ratio } from '../../fmt.js';
import { t } from '../../i18n.js';
import { State } from '../../ui/state.js';
import { Table } from '../../ui/table.js';
import { Ask, Act, failureText } from './parts.js';
import text from './system.text.js';

export const css = '/app/screens/system/system.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'sys.box.head'); }


// How long the commands stand off after bouquets were written. Long enough for
// a write of a full channel list to reach the disc on a box whose storage is a
// flash chip, short enough that nobody thinks the page is broken.
const kSettleMs = 15000;

/**
 * How full one of the two places a box runs out of is, as one sentence.
 *
 * A total of nought is a reading that failed and says so: nought free out of nought
 * would otherwise be drawn as a place that is full, which is the one wrong answer a
 * person would act on.
 *
 * @param {number} total
 * @param {number} free
 * @returns {string}
 */
export function fullness(total, free) {
	if (!Number.isFinite(total) || total <= 0)
		return '';
	const used = total - free;
	return t(text, 'sys.box.fullness', {
		used: bytes(used < 0 ? 0 : used),
		all: bytes(total),
		share: String(ratio(used < 0 ? 0 : used, total))
	});
}

/**
 * The rows of the first table, each with its own reader, so every value on screen is a
 * named member of the answer rather than a lookup by a string the compiler cannot
 * follow.
 *
 * The last two are the two places a box runs out of, and the root filesystem is on this
 * table because on the smallest box it is the twenty-eight megabytes an image has to fit
 * in: it is the number that hurts first, and no other screen says it.
 *
 * @type {{ id: string, text: string, read: (box: Api.Box) => string }[]}
 */
const kFields = [
	{ id: 'vendor', text: 'sys.box.vendor', read: function (box) { return box.vendor; } },
	{ id: 'model', text: 'sys.box.model', read: function (box) { return box.model; } },
	{ id: 'chipset', text: 'sys.box.chipset', read: function (box) { return box.chipset; } },
	{ id: 'image', text: 'sys.box.image', read: function (box) { return box.image_version; } },
	{ id: 'kernel', text: 'sys.box.kernel', read: function (box) { return box.kernel; } },
	{ id: 'hostname', text: 'sys.box.hostname', read: function (box) { return box.hostname; } },
	{ id: 'uptime', text: 'sys.box.uptime', read: function (box) { return duration(box.uptime); } },
	{ id: 'api', text: 'sys.box.api', read: function (box) { return box.api.major + '.' + box.api.minor; } },
	{ id: 'memory', text: 'sys.box.memory',
	  read: function (box) { return fullness(box.memory_total, box.memory_free); } },
	{ id: 'rootfs', text: 'sys.box.rootfs',
	  read: function (box) { return fullness(box.root_total, box.root_free); } }
];

/**
 * @param {{ box: Api.Box }} props
 * @returns {Web.Drawn}
 */
function Facts(props) {
	const box = props.box;

	return html`<div class="scroll-x">
		<table class="grid sys-box">
			<thead><tr>
				<th scope="col">${t(text, 'sys.box.field')}</th>
				<th scope="col" data-fit="wide">${t(text, 'sys.box.value')}</th>
			</tr></thead>
			<tbody>
				${kFields.map(function (field) {
					const value = field.read(box);
					return html`<tr key=${field.id}>
						<th scope="row">${t(text, field.text)}</th>
						${/* Empty on purpose, and empty is a value the folding
						     reads: the cell beside this one is the name of the
						     row and is still drawn when the head is gone, so a
						     word in front of the value would be that name
						     twice on one card. */ null}
						<td class="mono" data-field=${field.id} data-label="">
							${value === '' ? t(text, 'sys.box.unsaid') : value}
						</td>
					</tr>`;
				})}
			</tbody>
		</table>
	</div>`;
}

/**
 * How many of the old names have been asked for at all, which is the whole
 * point of the count: the decision to retire one rests on this and not on a
 * guess.
 *
 * @param {Api.LegacyUsageEntry[]} rows
 * @returns {number}
 */
export function usedCount(rows) {
	let used = 0;
	for (const row of rows) {
		if (row.calls > 0)
			used++;
	}
	return used;
}

/**
 * The count, most asked first and then by name, so the names a retirement would hurt
 * stand at the top instead of wherever the table happens to keep them.
 *
 * @param {Api.LegacyUsageEntry[]} rows
 * @param {Web.Sort} sort
 * @returns {Api.LegacyUsageEntry[]}
 */
export function usageOrder(rows, sort) {
	const out = rows.slice();
	const back = sort.dir === 'desc' ? -1 : 1;
	out.sort(function (a, b) {
		if (sort.column === 'calls' && a.calls !== b.calls)
			return (a.calls < b.calls ? -1 : 1) * back;
		if (a.name === b.name)
			return 0;
		return (a.name < b.name ? -1 : 1) * back;
	});
	return out;
}

/**
 * @returns {Web.Drawn}
 */
function Usage() {
	const [shot, setShot] = useState(store.read('GET', '/api/v1/system/legacy-usage'));
	const [sort, setSort] = useState(/** @type {Web.Sort} */ ({ column: 'calls', dir: 'desc' }));

	useEffect(function () {
		return store.watch('GET', '/api/v1/system/legacy-usage', null, setShot);
	}, []);

	const rows = shot.data ? shot.data.items : [];

	return html`<section class="sys-block">
		<h2>${t(text, 'sys.usage.title')}</h2>
		<p class="sys-lead">${t(text, 'sys.usage.why')}</p>
		<${State}
			phase=${shot.phase}
			problem=${shot.error ? shot.error.problem : null}
			empty=${shot.state === 'ready' && rows.length === 0}>
			<p class="sys-lead">${t(text, 'sys.usage.summary', { used: usedCount(rows), all: rows.length })}</p>
			<${Table}
				columns=${[
					{ id: 'name', label: t(text, 'sys.usage.name'), sortable: true, mono: true,
					  cell: function (/** @type {Api.LegacyUsageEntry} */ row) { return row.name; } },
					{ id: 'calls', label: t(text, 'sys.usage.calls'), sortable: true, mono: true,
					  cell: function (/** @type {Api.LegacyUsageEntry} */ row) { return String(row.calls); } }
				]}
				rows=${usageOrder(rows, sort)}
				rowKey=${function (/** @type {Api.LegacyUsageEntry} */ row) { return row.name; }}
				sort=${sort}
				onSort=${setSort} />
		<//>
	</section>`;
}

/**
 * @returns {Web.Drawn}
 */
export default function Box() {
	const [shot, setShot] = useState(store.read('GET', '/api/v1/system/info'));
	const [said, setSaid] = useState('');
	const [heldUntil, setHeldUntil] = useState(0);

	useEffect(function () {
		return store.watch('GET', '/api/v1/system/info', null, setShot);
	}, []);

	useEffect(function () {
		return events.on('bouquets-changed', function () {
			setHeldUntil(Date.now() + kSettleMs);
		});
	}, []);

	// The hold lets go by itself, or a screen that was left alone would keep
	// the commands off until somebody clicked something.
	useEffect(function () {
		const left = heldUntil - Date.now();
		if (left <= 0)
			return undefined;
		const timer = window.setTimeout(function () { setHeldUntil(0); }, left);
		return function () { window.clearTimeout(timer); };
	}, [heldUntil]);

	const held = heldUntil > Date.now();

	/**
	 * @param {() => Promise<unknown>} call
	 * @returns {void}
	 */
	function drive(call) {
		setSaid('');
		call().then(function () {
			setSaid(t(text, 'sys.power.sent'));
		}, function (caught) {
			setSaid(failureText(caught));
		});
	}

	return html`<div class="sys">
		<section class="sys-block">
			<h2>${t(text, 'sys.box.title')}</h2>
			<${State}
				phase=${shot.phase}
				problem=${shot.error ? shot.error.problem : null}>
				${shot.data ? html`<${Facts} box=${shot.data} />` : null}
			<//>
		</section>

		<section class="sys-block">
			<h2>${t(text, 'sys.power.title')}</h2>
			<p class="sys-lead">${t(text, 'sys.power.nostate')}</p>
			${/* The one sentence in front of these that is still drawn before
			     anybody presses anything, and it earns it: it says the controls
			     are off for a moment and why, which is a fact about the box and
			     not about this session. */ null}
			${held ? html`<p class="sys-why" role="status">${t(text, 'sys.power.settling')}</p>` : null}
			<div class="sys-row">
				<${Ask}
					label=${t(text, 'sys.power.standby')}
					question=${t(text, 'sys.power.standby.ask')}
					needs="system"
					busy=${held}
					onConfirm=${function () {
						drive(function () {
							return store.write('POST', '/api/v1/system/standby', { body: { on: true } });
						});
					}} />
				${/* No question in front of this one. It puts the picture back
				     on the screen and takes nothing away, and a question in
				     front of something harmless is what teaches everybody to
				     click through the one that matters. */ null}
				<${Act}
					label=${t(text, 'sys.power.wake')}
					needs="system"
					busy=${held}
					onClick=${function () {
						drive(function () {
							return store.write('POST', '/api/v1/system/standby', { body: { on: false } });
						});
					}} />
				<${Ask}
					label=${t(text, 'sys.power.restart')}
					question=${t(text, 'sys.power.restart.ask')}
					needs="system"
					busy=${held}
					onConfirm=${function () {
						drive(function () { return store.write('POST', '/api/v1/system/restart', {}); });
					}} />
				<${Ask}
					label=${t(text, 'sys.power.reboot')}
					question=${t(text, 'sys.power.reboot.ask')}
					needs="system"
					busy=${held}
					onConfirm=${function () {
						drive(function () { return store.write('POST', '/api/v1/system/reboot', {}); });
					}} />
				<${Ask}
					label=${t(text, 'sys.power.shutdown')}
					question=${t(text, 'sys.power.shutdown.ask')}
					needs="system"
					busy=${held}
					onConfirm=${function () {
						drive(function () { return store.write('POST', '/api/v1/system/shutdown', {}); });
					}} />
			</div>
			${said !== '' ? html`<p class="sys-said" role="status">${said}</p>` : null}
		</section>

		<${Usage} />
	</div>`;
}

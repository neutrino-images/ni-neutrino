/* The services this box drives itself, in the two groups they actually are.

   ONE LIST AND TWO THINGS ON IT. The box hands out nineteen names and says of each
   which sort it is: seven softcams it descrambles with, which it also drives from a
   screen of its own on the television, and twelve services an image was built to carry.
   They are on one list because one layer starts them; they are two groups here because
   they answer two different questions. The surface this replaces put them on one page
   under a heading that said Plugin-Steuerung, which is where the belief that Samba is a
   Neutrino plugin comes from. Neutrino's own plugins are Lua and scripts the box lists
   in its menus, they are next door, and each of the two pages says where the other is.

   WHAT IS NOT ON THIS PAGE, and would be if the list were written here rather than
   asked for: zapit, sectionsd and timerd. They are threads of the program and not
   processes, so there is nothing to start, nothing to stop and nothing for a process
   table to find.

   WHAT THE BOX WAS NOT BUILT WITH is folded away rather than shown as a row with
   buttons that will be refused. The box says which of the nineteen it actually carries,
   by looking where the two scripts that start one look. Folded and not dropped, because
   a name missing with no explanation is what sends somebody looking for a page that has
   gone.

   THREE BUTTONS A ROW AND NO COMMAND LINE. The surface this replaces offered a shell in
   the browser; nineteen named services with start, stop and restart is what anybody
   used that shell for, and whoever needs a shell has one over SSH.

   WHY RESTART IS OFFERED ON EVERY ROW EVEN THOUGH NOT EVERY ROW HAS IT. The list says
   what a service is called and whether a process of its name is in the process table,
   and nothing about which verbs it carries; the verbs live in a table a layer below and
   only a refusal reveals them. A guess written into this page would be a second copy of
   that table, and the copy is what goes out of step.

   AND WHY RUNNING IS NOT WHAT WAS ASKED FOR. The box answers whether a process of that
   name exists, which is not the same as its having been started: a service that dies a
   second later was started and is not running. That is why the list is read again after
   every verb. */

import { html, useState, useEffect } from '../../runtime.js';
import * as store from '../../store.js';
import { t } from '../../i18n.js';
import { hrefFor } from '../../nav.js';
import { Dot } from '../../ui/dot.js';
import { State } from '../../ui/state.js';
import { Act, failureText } from './parts.js';
import text from './system.text.js';

export const css = '/app/screens/system/system.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'sys.daemons.head'); }


/**
 * One daemon, named the way the three routes below name it.
 *
 * The list and the routes are the same nineteen names on the box, and the document says
 * so in one place only: the routes declare their choices and the list answers a plain
 * string for the same name (src/httpd/ep/ep_daemons.cpp). Until it says so in both, the one
 * crossing from the list to a route is the cast in drive below.
 *
 * @typedef {Api.PathValues<'POST /api/v1/daemons/{name}/start'>['name']} DaemonName
 */

/** @typedef {Api.Result<'GET /api/v1/daemons'>['items'][number]} Daemon */

/**
 * The three verbs, spelt as the three addresses spell them, so a row is a loop over this
 * and not three blocks that have to be kept alike.
 *
 * Each one carries its own call rather than a piece of an address: an address put
 * together out of pieces is one no check can hold to the document.
 *
 * @type {{ id: string, text: string, send: (name: DaemonName) => Promise<unknown> }[]}
 */
const kVerbs = [
	{
		id: 'start', text: 'sys.daemons.start',
		send: function (name) {
			return store.write('POST', '/api/v1/daemons/{name}/start',
				{ params: { name: name }, touches: ['/api/v1/daemons'] });
		}
	},
	{
		id: 'stop', text: 'sys.daemons.stop',
		send: function (name) {
			return store.write('POST', '/api/v1/daemons/{name}/stop',
				{ params: { name: name }, touches: ['/api/v1/daemons'] });
		}
	},
	{
		id: 'restart', text: 'sys.daemons.restart',
		send: function (name) {
			return store.write('POST', '/api/v1/daemons/{name}/restart',
				{ params: { name: name }, touches: ['/api/v1/daemons'] });
		}
	}
];

/**
 * The rows of one sort, the ones this box carries first.
 *
 * Sorted rather than filtered, so what the image was not built with is still reachable
 * and still says which sort it belongs to. The order inside each half is the box's own.
 *
 * @param {readonly Daemon[]} rows
 * @param {string} kind
 * @returns {Daemon[]}
 */
export function ofKind(rows, kind) {
	/** @type {Daemon[]} */
	const here = [];
	/** @type {Daemon[]} */
	const missing = [];
	for (const row of rows) {
		if (row.kind !== kind)
			continue;
		(row.installed ? here : missing).push(row);
	}
	return here.concat(missing);
}

/**
 * How many of a sort this box actually carries.
 *
 * @param {readonly Daemon[]} rows
 * @returns {number}
 */
export function carried(rows) {
	let count = 0;
	for (const row of rows) {
		if (row.installed)
			count++;
	}
	return count;
}

/**
 * @param {{ rows: readonly Daemon[], busy: string, needs: 'write'|'system', onDrive: (name: DaemonName, send: (name: DaemonName) => Promise<unknown>) => void }} props
 * @returns {Web.Drawn}
 */
function Rows(props) {
	return html`<div class="scroll-x">
		<table class="grid sys-daemons">
			<thead><tr>
				<th scope="col">${t(text, 'sys.daemons.name')}</th>
				<th scope="col">${t(text, 'sys.daemons.state')}</th>
				<th scope="col">${t(text, 'sys.daemons.actions')}</th>
			</tr></thead>
			<tbody>
				${props.rows.map(function (row) {
					return html`<tr key=${row.id}>
						<th scope="row" class="mono" data-daemon=${row.id}>${row.id}</th>
						${/* The word its column head carries, written on the cell
						     rather than left to the table widget: this table is
						     written out here, and a folded row with no words in
						     front of its cells is three values in a column. */ null}
						<td data-label=${t(text, 'sys.daemons.state')}>
							${row.installed
								? html`<${Dot}
									kind=${row.running ? 'running' : ''}
									word=${t(text, row.running ? 'sys.daemons.running' : 'sys.daemons.stopped')} />`
								: html`<span class="sys-sub">${t(text, 'sys.daemons.absent')}</span>`}
						</td>
						${/* The row of buttons is a box inside the cell and not the
						     cell itself: a cell laid out as anything but a table
						     cell leaves the column it was in, and the three heads
						     above then stand over nothing. */ null}
						<td data-label=${t(text, 'sys.daemons.actions')}>
							${row.installed
								? html`<div class="sys-row">
									${kVerbs.map(function (verb) {
										return html`<${Act}
											key=${verb.id}
											label=${t(text, verb.text)}
											needs=${props.needs}
											busy=${props.busy === row.id}
											onClick=${function () { props.onDrive(row.id, verb.send); }} />`;
									})}
								</div>`
								: null}
						</td>
					</tr>`;
				})}
			</tbody>
		</table>
	</div>`;
}

/**
 * @returns {Web.Drawn}
 */
export default function Daemons() {
	const [shot, setShot] = useState(store.read('GET', '/api/v1/daemons'));
	const [said, setSaid] = useState('');
	const [busy, setBusy] = useState('');

	useEffect(function () {
		return store.watch('GET', '/api/v1/daemons', null, setShot);
	}, []);

	const rows = shot.data ? shot.data.items : [];
	const softcams = ofKind(rows, 'softcam');
	const services = ofKind(rows, 'service');

	/**
	 * @param {DaemonName} name as the box's own list spells it
	 * @param {(name: DaemonName) => Promise<unknown>} send
	 * @returns {void}
	 */
	function drive(name, send) {
		setSaid('');
		setBusy(name);
		send(name).then(function () {
			setSaid(name + ': ' + t(text, 'sys.answer.done'));
		}, function (caught) {
			setSaid(name + ': ' + failureText(caught));
		}).then(function () {
			setBusy('');
			// Asked again rather than believed: what the verb did to the
			// process table is the box's answer and not this page's.
			return store.reload('GET', '/api/v1/daemons');
		}, function () {
			setBusy('');
		});
	}

	return html`<div class="sys">
		<${State}
			phase=${shot.phase}
			problem=${shot.error ? shot.error.problem : null}
			empty=${shot.state === 'ready' && rows.length === 0}>
			<section class="sys-block sys-group" data-group="softcam">
				<h2>${t(text, 'sys.daemons.softcams')}</h2>
				<p class="sys-lead">${t(text, 'sys.daemons.softcams.lead',
					{ count: carried(softcams), all: softcams.length })}</p>
				<p class="sys-lead">${t(text, 'sys.daemons.threads')}</p>
				<${Rows} rows=${softcams} busy=${busy} needs="system" onDrive=${drive} />
			</section>

			<section class="sys-block sys-group" data-group="service">
				<h2>${t(text, 'sys.daemons.services')}</h2>
				<p class="sys-lead">${t(text, 'sys.daemons.services.lead',
					{ count: carried(services), all: services.length })}</p>
				<p class="sys-lead">
					${t(text, 'sys.daemons.notplugins')}
					${' '}
					<a href=${hrefFor('system', 'plugins')}>${t(text, 'sys.daemons.toplugins')}</a>
				</p>
				<${Rows} rows=${services} busy=${busy} needs="system" onDrive=${drive} />
			</section>
		<//>
		${said !== '' ? html`<p class="sys-said" role="status">${said}</p>` : null}
		<p class="sys-lead">${t(text, 'sys.daemons.noverb')}</p>
		<p class="sys-lead">${t(text, 'sys.daemons.noshell')}</p>
	</div>`;
}

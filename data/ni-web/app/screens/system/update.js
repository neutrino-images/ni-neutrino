/* Whether the update server has a newer image, and installing an image update that
   already lies on the box.

   A NOTE ABOUT THE SERVER, AND NOTHING TO PRESS THAT INSTALLS FROM IT. The top of
   the page says whether the server's list holds an image newer than the running
   one, as the menu's online update and the background check ask it. Fetching and
   writing an image is left to the menu; the note says so. The box keeps an answer
   for a quarter of an hour, so opening the page again does not ask the server
   again, and the button beside the note asks now.

   THE LOCAL UPDATE OF THE TELEVISION MENU, FOR A .BIN AND NOTHING ELSE. The box
   hands the file to the same install script the menu's local update runs. A
   complete image is not offered here: writing a whole system is for somebody in
   front of the television, and the menu has it.

   NO UPLOAD ON THIS PAGE. A file reaches /tmp the way any file reaches the box, and
   the files destination already has that. The sentence over the list says where to
   put one, and its link opens that destination in the directory the box named.

   WHY NOTHING HERE SAYS THE UPDATE WORKED. The box answers that it took the request
   and nothing after that, and the television shows nothing either: a box in standby
   stays in standby. An update of the box's own program ends it and restarts the box,
   and this page with it. */

import { html, useState, useEffect } from '../../runtime.js';
import * as store from '../../store.js';
import { t } from '../../i18n.js';
import { bytes, clock, day, dayAndClock } from '../../fmt.js';
import { hrefFor } from '../../nav.js';
import { State } from '../../ui/state.js';
import { Dialog } from '../../ui/dialog.js';
import { Act, withSession, failureText } from './parts.js';
import { dirHref } from '../files/paths.js';
import text from './system.text.js';

export const css = '/app/screens/system/system.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'sys.update.head'); }

/** @typedef {Api.Result<'GET /api/v1/update/local'>['items'][number]} UpdateFile */
/** @typedef {Api.Result<'GET /api/v1/update/online'>['running']} UpdateImage */

/**
 * "2026.11 vom 09.11.2026", or as much of it as the box could read.
 *
 * @param {UpdateImage} image
 * @returns {string}
 */
function imageText(image) {
	const release = image.release !== '' ? image.release : t(text, 'sys.online.unknown');
	return image.built > 0
		? t(text, 'sys.online.image', { release: release, day: day(image.built) })
		: release;
}

/**
 * One image of the comparison: version and build time in the same columns as the
 * other one, so the two lines can be read against each other character for
 * character. Mono for that; a part the box could not read stays out.
 *
 * @param {UpdateImage} image
 * @returns {string}
 */
function stamp(image) {
	const release = image.release !== '' ? image.release : t(text, 'sys.online.unknown');
	return image.built > 0 ? release + '  ' + dayAndClock(image.built) : release;
}

/**
 * The note at the top: a newer image on the server, none, or no server to ask.
 *
 * @returns {Web.Drawn}
 */
function Online() {
	const [shot, setShot] = useState(store.read('GET', '/api/v1/update/online'));
	const [asking, setAsking] = useState(false);

	useEffect(function () {
		return store.watch('GET', '/api/v1/update/online', null, setShot);
	}, []);

	/* Asked with fresh, which is an entry of its own in the store; the plain one
	   the note watches is read again after it and gets the answer just made. */
	function askNow() {
		setAsking(true);
		store.reload('GET', '/api/v1/update/online', { query: { fresh: true } }).catch(function () {
			// The plain read below meets the same fault and shows it.
		}).then(function () {
			return store.reload('GET', '/api/v1/update/online');
		}).catch(function () {
			// The entry holds the fault and the note shows it.
		}).then(function () {
			setAsking(false);
		});
	}

	const on = store.lastAnswer(shot);
	/** @type {Web.Drawn} */
	let note;
	if (on === null && shot.error) {
		note = html`<p class="callout bad">${t(text, 'sys.online.failed', { why: failureText(shot.error) })}</p>`;
	} else if (on === null) {
		note = html`<p class="callout" role="status">${t(text, 'sys.online.asking')}</p>`;
	} else if (!on.reached) {
		note = html`<p class="callout warn">${t(text, 'sys.online.unreached', { running: imageText(on.running) })}</p>`;
	} else if (on.newer && on.found) {
		note = html`<div class="callout sys-online-new">
			<p><strong>${t(text, 'sys.online.newer', { name: on.found.name !== '' ? on.found.name : imageText(on.found) })}</strong></p>
			<dl class="sys-online-versions">
				<dt>${t(text, 'sys.online.on.server')}</dt>
				<dd>${stamp(on.found)}</dd>
				<dt>${t(text, 'sys.online.on.box')}</dt>
				<dd>${stamp(on.running)}</dd>
			</dl>
			<p>${t(text, 'sys.online.newer.how')}</p>
		</div>`;
	} else {
		note = html`<p class="callout sys-online-ok">${t(text, 'sys.online.current', { running: imageText(on.running) })}</p>`;
	}

	return html`<section class="sys-block sys-online" aria-live="polite">
		${note}
		<div class="sys-row sys-online-tools">
			<${Act}
				label=${t(text, 'sys.online.again')}
				busy=${asking || shot.phase === 'first'}
				onClick=${askNow} />
			${on !== null && on.checked > 0
				? html`<span class="sys-online-when">${t(text, 'sys.online.checked', { when: clock(on.checked) })}</span>`
				: null}
		</div>
	</section>`;
}

/**
 * @returns {Web.Drawn}
 */
export default function Update() {
	const [shot, setShot] = useState(store.read('GET', '/api/v1/update/local'));
	const [said, setSaid] = useState('');
	const [sending, setSending] = useState(false);
	const [asking, setAsking] = useState(/** @type {UpdateFile | null} */ (null));

	useEffect(function () {
		return store.watch('GET', '/api/v1/update/local', null, setShot);
	}, []);

	const rows = shot.data ? shot.data.items : [];
	const dir = shot.data ? shot.data.dir : '/tmp';

	/**
	 * @param {UpdateFile} file
	 * @returns {void}
	 */
	function send(file) {
		setSending(true);
		store.write('POST', '/api/v1/update/local', { body: { name: file.name } }).then(function () {
			setSaid(t(text, 'sys.update.sent', { name: file.name }));
		}, function (caught) {
			setSaid(file.name + ': ' + failureText(caught));
		}).then(function () {
			setSending(false);
		});
	}

	return html`<div class="sys">
		<${Online} />
		<section class="sys-block">
			<h2>${t(text, 'sys.update.title')}</h2>
			<p class="sys-lead">
				${t(text, 'sys.update.where', { dir: dir })}
				${' '}
				<a href=${dirHref(hrefFor('files', 'list'), dir)}>${t(text, 'sys.update.tofiles')}</a>
			</p>
			<div class="sys-row sys-update-tools">
				<${Act}
					label=${t(text, 'sys.update.reload')}
					onClick=${function () {
						setSaid('');
						store.reload('GET', '/api/v1/update/local');
					}} />
			</div>
			<${State}
				phase=${shot.phase}
				problem=${shot.error ? shot.error.problem : null}
				empty=${shot.state === 'ready' && rows.length === 0 ? t(text, 'sys.update.none', { dir: dir }) : false}>
				<div class="scroll-x">
					<table class="grid sys-update">
						<thead><tr>
							<th scope="col">${t(text, 'sys.update.file')}</th>
							<th scope="col">${t(text, 'sys.update.size')}</th>
							<th scope="col">${t(text, 'sys.update.modified')}</th>
							<th scope="col">${t(text, 'sys.update.action')}</th>
						</tr></thead>
						<tbody>
							${rows.map(function (row) {
								return html`<tr key=${row.name}>
									<th scope="row" class="mono" data-update=${row.name}>${row.name}</th>
									<td data-label=${t(text, 'sys.update.size')}>${bytes(row.size)}</td>
									<td data-label=${t(text, 'sys.update.modified')}>${dayAndClock(row.modified)}</td>
									<td data-label=${t(text, 'sys.update.action')}>
										<${Act}
											label=${t(text, 'sys.update.install')}
											busy=${sending}
											onClick=${function () { setSaid(''); setAsking(row); }} />
									</td>
								</tr>`;
							})}
						</tbody>
					</table>
				</div>
			<//>
			${said !== '' ? html`<p class="callout sys-said" role="status">${said}</p>` : null}
		</section>
		<p class="sys-lead">${t(text, 'sys.update.menuonly')}</p>

		<${Dialog}
			open=${asking !== null}
			title=${t(text, 'sys.update.install')}
			confirmLabel=${t(text, 'sys.update.install')}
			onCancel=${function () { setAsking(null); }}
			onConfirm=${function () {
				const file = asking;
				setAsking(null);
				// The question first and the sign in second, as everywhere in
				// this destination: two sheets over each other are two traps.
				if (file !== null)
					withSession('system', function () { send(file); });
			}}>
			${asking !== null
				? html`<div>
					<p>${t(text, 'sys.update.ask', { name: asking.name })}</p>
					<p class="callout warn">${t(text, 'sys.update.ask.warn')}</p>
				</div>`
				: null}
		<//>
	</div>`;
}

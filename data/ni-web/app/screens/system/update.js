/* Installing an image update that already lies on the box.

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
import { bytes, dayAndClock } from '../../fmt.js';
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

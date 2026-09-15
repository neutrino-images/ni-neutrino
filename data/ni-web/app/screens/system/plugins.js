/* What the box found in its plugin directories, and the one place a named file out of
   one directory is run.

   TWO THINGS AND NOT ONE, because the box answers for them separately. A plugin is
   something the box found, holds in a list and starts by the name of its files. A
   script is a file in one directory that is run and whose output comes back, and that
   directory holds whatever was put there, which is why it is the only thing on this
   destination that asks for the credential owning the box.

   THREE THINGS AND NOT THESE TWO is what the surface this replaces had. It called a
   second page Plugin-Steuerung and put Samba, miniDLNA and eleven others on it, which
   are services an image was built to carry; they live under Dienste, and the sentence
   below says so, because a name used for two things is how somebody ends up looking for
   Samba here. */

import { html, useState, useEffect } from '../../runtime.js';
import { api } from '../../api.js';
import * as store from '../../store.js';
import { t } from '../../i18n.js';
import { hrefFor } from '../../nav.js';
import { State } from '../../ui/state.js';
import { Field } from '../../ui/field.js';
import { Switch } from '../../ui/switch.js';
import { Act, withSession, failureText } from './parts.js';
import text from './system.text.js';

export const css = '/app/screens/system/system.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'sys.plugins.head'); }


/**
 * The sorts the box names, each with this page's word for it. A sort the box
 * adds without a word here reaches the screen as the box's own spelling rather
 * than as nothing.
 *
 * @param {string} kind
 * @returns {string}
 */
export function kindWord(kind) {
	const key = 'sys.plugins.kind.' + kind;
	const said = t(text, key);
	return said === key ? kind : said;
}

/**
 * @returns {Web.Drawn}
 */
function Found() {
	const [shot, setShot] = useState(store.read('GET', '/api/v1/plugins'));
	const [said, setSaid] = useState('');

	useEffect(function () {
		return store.watch('GET', '/api/v1/plugins', null, setShot);
	}, []);

	const rows = shot.data ? shot.data.items : [];

	/**
	 * @param {() => Promise<unknown>} call
	 * @param {string} done what to say where the box took it
	 * @returns {void}
	 */
	function drive(call, done) {
		setSaid('');
		call().then(function () {
			setSaid(done);
		}, function (caught) {
			setSaid(failureText(caught));
		});
	}

	/**
	 * @param {Api.PathValues<'PUT /api/v1/plugins/{name}/hidden'>['name']} name
	 * @param {boolean} hidden
	 * @returns {void}
	 */
	function setHidden(name, hidden) {
		drive(function () {
			return store.write('PUT', '/api/v1/plugins/{name}/hidden', {
				params: { name: name },
				body: { hidden: hidden },
				touches: ['/api/v1/plugins']
			}).then(function (answer) {
				// The file is written before the answer comes back, so the
				// list is asked again rather than guessed at here.
				store.reload('GET', '/api/v1/plugins');
				return answer;
			});
		}, t(text, 'sys.answer.done'));
	}

	return html`<section class="sys-block">
		<p class="sys-lead">
			${t(text, 'sys.plugins.notservices')}
			${' '}
			<a href=${hrefFor('system', 'daemons')}>${t(text, 'sys.plugins.todaemons')}</a>
		</p>
		<div class="sys-row">
			<${Act}
				label=${t(text, 'sys.plugins.reload')}
				needs="write"
				onClick=${function () {
					drive(function () {
						return store.write('POST', '/api/v1/plugins/reload', { touches: ['/api/v1/plugins'] })
							.then(function (answer) {
								// The list is rebuilt by the thread that draws
								// the screens, so the box cannot say it has
								// happened; asking again is the only way to
								// find out what it now holds.
								store.reload('GET', '/api/v1/plugins');
								return answer;
							});
					}, t(text, 'sys.answer.taken'));
				}} />
		</div>
		<${State}
			phase=${shot.phase}
			problem=${shot.error ? shot.error.problem : null}
			empty=${shot.state === 'ready' && rows.length === 0 ? t(text, 'sys.plugins.none') : false}>
			<div class="scroll-x">
				<table class="grid sys-plugins">
					<thead><tr>
						<th scope="col">${t(text, 'sys.plugins.name')}</th>
						<th scope="col">${t(text, 'sys.plugins.what')}</th>
						<th scope="col">${t(text, 'sys.plugins.inmenu')}</th>
						<th scope="col">${t(text, 'sys.plugins.start')}</th>
					</tr></thead>
					<tbody>
						${rows.map(function (row) {
							return html`<tr key=${row.name}>
								<th scope="row" data-plugin=${row.name}>
									${row.title === '' ? row.name : row.title}
									<span class="mono sys-sub">${row.name}</span>
								</th>
								${/* Each cell carries the word its column head
								     carries, because this table is written out
								     here rather than drawn by the one widget
								     that writes them, and below the phone width
								     the head is gone. The two cells whose
								     control already says the word carry an
								     empty one, which the folding reads as
								     nothing to put in front. */ null}
								<td data-label=${t(text, 'sys.plugins.what')}>
									${kindWord(row.kind)}
									${row.description !== '' ? html`<span class="sys-sub">${row.description}</span>` : null}
								</td>
								<td data-label="">
									<${Switch}
										label=${t(text, 'sys.plugins.inmenu')}
										checked=${!row.hidden}
										onChange=${function (/** @type {Event} */ event) {
											const box = /** @type {HTMLInputElement} */ (event.currentTarget);
											const wanted = !box.checked;
											/* Put back at once where the sign in
											   is declined or the box refuses, so
											   the switch never stands somewhere
											   the box is not. */
											box.checked = !row.hidden;
											withSession('write', function () { setHidden(row.name, wanted); });
										}} />
								</td>
								<td data-label="">
									<${Act}
										label=${t(text, 'sys.plugins.start')}
										needs="write"
										onClick=${function () {
											drive(function () {
												return store.write('POST', '/api/v1/plugins/{name}/start',
													{ params: { name: row.name } });
											}, t(text, 'sys.answer.taken'));
										}} />
								</td>
							</tr>`;
						})}
					</tbody>
				</table>
			</div>
		<//>
		${said !== '' ? html`<p class="sys-said" role="status">${said}</p>` : null}
	</section>`;
}

/**
 * @returns {Web.Drawn}
 */
function Script() {
	const [name, setName] = useState('');
	const [args, setArgs] = useState('');
	const [output, setOutput] = useState(/** @type {string | null} */ (null));
	const [failed, setFailed] = useState('');
	const [running, setRunning] = useState(false);

	/**
	 * @returns {void}
	 */
	function run() {
		setOutput(null);
		setFailed('');
		setRunning(true);
		api('POST', '/api/v1/scripts/{name}', {
			params: { name: name },
			body: { args: args }
		}).then(function (answer) {
			setRunning(false);
			setOutput(answer ? answer.output : '');
		}, function (caught) {
			setRunning(false);
			setFailed(failureText(caught));
		});
	}

	return html`<section class="sys-block">
		<h2>${t(text, 'sys.scripts.title')}</h2>
		<p class="sys-lead">${t(text, 'sys.scripts.lead')}</p>
		<${Field}
			label=${t(text, 'sys.scripts.name')}
			value=${name}
			onInput=${function (/** @type {Web.On<HTMLInputElement, InputEvent>} */ e) { setName(e.currentTarget.value); }} />
		<${Field}
			label=${t(text, 'sys.scripts.args')}
			hint=${t(text, 'sys.scripts.args.hint')}
			value=${args}
			onInput=${function (/** @type {Web.On<HTMLInputElement, InputEvent>} */ e) { setArgs(e.currentTarget.value); }} />
		<div class="sys-row">
			<${Act}
				label=${t(text, 'sys.scripts.run')}
				primary=${true}
				needs="system"
				busy=${running || name === ''}
				onClick=${run} />
		</div>
		${failed !== '' ? html`<p class="sys-said" role="status">${failed}</p>` : null}
		${output !== null
			? html`<div class="sys-doc">
				<h3>${t(text, 'sys.scripts.output')}</h3>
				${output === ''
					? html`<p class="sys-lead">${t(text, 'sys.scripts.silent')}</p>`
					: html`<pre class="sys-pre mono">${output}</pre>`}
			</div>`
			: null}
	</section>`;
}

/**
 * @returns {Web.Drawn}
 */
export default function Plugins() {
	return html`<div class="sys">
		<${Found} />
		<${Script} />
	</div>`;
}

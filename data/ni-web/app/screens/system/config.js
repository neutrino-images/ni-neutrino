/* The three documents the box keeps its own configuration in, and the two ways of
   telling it to read its configuration again.

   Shown and downloaded, never edited. There is no route that takes one of these back,
   so a text area with a save button under it would be a promise the box does not keep.

   The download is built from the document's own template with the name beside it,
   because an address glued together out of pieces is one nothing can hold to the
   document the box writes about itself.

   They are fetched only when asked for. On a box with a full channel list the first of
   the three runs to megabytes. */

import { html, useState } from '../../runtime.js';
import { api, buildUrl } from '../../api.js';
import * as store from '../../store.js';
import { t } from '../../i18n.js';
import { Switch } from '../../ui/switch.js';
import { Ask, failureText } from './parts.js';
import text from './system.text.js';

export const css = '/app/screens/system/system.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'sys.config.lead'); }


/**
 * The three names the box declares, with this page's word for each. A closed
 * set here because it is a closed set there: the route takes one of these
 * three and turns away everything else before a handler is entered.
 *
 * @type {{ id: 'services'|'bouquets'|'ubouquets', text: string }[]}
 */
const kDocuments = [
	{ id: 'services', text: 'sys.config.services' },
	{ id: 'bouquets', text: 'sys.config.bouquets' },
	{ id: 'ubouquets', text: 'sys.config.ubouquets' }
];

/**
 * @param {{ id: 'services'|'bouquets'|'ubouquets', text: string }} props
 * @returns {Web.Drawn}
 */
function Document(props) {
	const [body, setBody] = useState(/** @type {string | null} */ (null));
	const [failed, setFailed] = useState('');
	const [loading, setLoading] = useState(false);

	const href = buildUrl('/api/v1/config/{document}', { document: props.id });

	/**
	 * @returns {void}
	 */
	function toggle() {
		if (body !== null || failed !== '') {
			setBody(null);
			setFailed('');
			return;
		}
		setLoading(true);
		api('GET', '/api/v1/config/{document}', {
			params: { document: props.id },
			accept: 'text'
		}).then(function (answer) {
			setLoading(false);
			setBody(answer);
		}, function (caught) {
			setLoading(false);
			setFailed(failureText(caught));
		});
	}

	const open = body !== null || failed !== '';

	return html`<div class="sys-doc" data-document=${props.id}>
		<h3>${t(text, props.text)} <span class="mono sys-sub">${props.id}.xml</span></h3>
		<div class="sys-row">
			<button
				type="button"
				class="btn"
				disabled=${loading}
				aria-expanded=${open ? 'true' : 'false'}
				onClick=${toggle}>${t(text, open ? 'sys.config.hide' : 'sys.config.show')}</button>
			<a class="btn" href=${href} download=${props.id + '.xml'}>${t(text, 'sys.config.download')}</a>
		</div>
		${loading ? html`<p class="sys-lead" role="status">${t(text, 'sys.config.loading')}</p>` : null}
		${failed !== '' ? html`<p class="sys-said" role="status">${failed}</p>` : null}
		${body !== null ? html`<pre class="sys-pre mono">${body}</pre>` : null}
	</div>`;
}

/**
 * @returns {Web.Drawn}
 */
export default function Config() {
	const [hard, setHard] = useState(false);
	const [said, setSaid] = useState('');

	/**
	 * @param {() => Promise<unknown>} call
	 * @returns {void}
	 */
	function drive(call) {
		setSaid('');
		call().then(function () {
			setSaid(t(text, 'sys.answer.taken'));
		}, function (caught) {
			setSaid(failureText(caught));
		});
	}

	return html`<div class="sys">
		<section class="sys-block">
			${kDocuments.map(function (one) {
				return html`<${Document} key=${one.id} id=${one.id} text=${one.text} />`;
			})}
		</section>

		<section class="sys-block">
			<h2>${t(text, 'sys.reload.title')}</h2>
			<${Switch}
				label=${t(text, 'sys.reload.hard')}
				hint=${t(text, 'sys.reload.hard.hint')}
				checked=${hard}
				onChange=${function (/** @type {Web.On<HTMLInputElement>} */ e) { setHard(e.currentTarget.checked); }} />
			<div class="sys-row">
				<${Ask}
					label=${t(text, 'sys.reload.channels')}
					question=${t(text, 'sys.reload.channels.ask')}
					needs="write"
					onConfirm=${function () {
						drive(function () {
							return store.write('POST', '/api/v1/channels/reload', {
								body: { hard: hard },
								touches: ['/api/v1/channels', '/api/v1/bouquets']
							});
						});
					}} />
				<${Ask}
					label=${t(text, 'sys.reload.setup')}
					question=${t(text, 'sys.reload.setup.ask')}
					needs="write"
					onConfirm=${function () {
						drive(function () {
							return store.write('POST', '/api/v1/system/reload-setup', {
								touches: ['/api/v1/settings']
							});
						});
					}} />
			</div>
			${said !== '' ? html`<p class="sys-said" role="status">${said}</p>` : null}
		</section>
	</div>`;
}

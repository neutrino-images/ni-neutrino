/* Every route this server answers, out of the document the server writes about
   itself.

   THE LEVEL IS WHY THIS PAGE IS BESIDE THE READER AND NOT INSTEAD OF IT. The
   box ships a Swagger page under /swagger/ which reads exactly this document
   and shows the bodies, the answers and the shapes; nothing here tries to be a
   second one of those. What that reader is bad at is the question somebody
   arrives with before they hand a password out: what does a caller reach with
   nothing but read. The document answers it one word per route, and here that
   word is a filter, so the answer is one press.

   THE COUNTS ARE THE CONTROL. They were two tables on a screen of their own
   that counted and did nothing else. How many routes sit at each level is worth
   knowing, and a count that is also the button showing them is worth more than
   one that is read here and acted on somewhere else.

   A BUILD WITHOUT THE PROSE STILL HAS THIS PAGE. The switch that leaves the
   descriptions out of the document takes the reader beside it away as well, and
   it leaves every path, every group and every level exactly where they are. So
   what goes missing here is the last column and the link to the reader, and the
   page says which build it is looking at rather than offering a way into a
   reader nobody installed. What says it is the document: no route anywhere in
   it says what it does, which is the one thing that switch takes out.

   A cast where it is fetched, and one. The route that hands the document out
   declares no shape for it, because an OpenAPI document is not something the
   field tables can describe, so the generated client knows only that the
   address answers. The shape is stated once in doc.js and read there. */

import { html, useState, useEffect, useMemo } from '../../runtime.js';
import * as store from '../../store.js';
import { t } from '../../i18n.js';
import { State } from '../../ui/state.js';
import { Select } from '../../ui/select.js';
import { Table } from '../../ui/table.js';
import { byLevel, groupsOf, levelOf, matching, operationsOf } from './doc.js';
import text from './dev.text.js';

/** @typedef {import('./doc.js').Operation} Operation */

export const css = '/app/screens/dev/dev.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'dev.api.lead'); }


const kPath = '/api/openapi.json';

/**
 * What this page calls a level, and the document's own spelling for one this
 * page has never heard of.
 *
 * @param {string} id
 * @returns {string}
 */
export function levelWord(id) {
	const key = 'dev.api.level.' + id;
	const said = t(text, key);
	return said === key ? id : said;
}

/**
 * A word with how many rows carry it beside it, which is the form the group
 * and the level are both offered in.
 *
 * @param {string} word
 * @param {number} count
 * @returns {string}
 */
function counted(word, count) {
	return word + ' (' + String(count) + ')';
}

/**
 * @returns {Web.Drawn}
 */
export default function DevApi() {
	const [shot, setShot] = useState(store.read('GET', kPath));
	const [query, setQuery] = useState('');
	const [group, setGroup] = useState('');
	const [level, setLevel] = useState('');

	useEffect(function () {
		return store.watch('GET', kPath, null, setShot);
	}, []);

	/* The one line where the address meets the shape. Everything below reads
	   what doc.js made of it and nothing reads the answer itself. */
	const document = /** @type {unknown} */ (shot.data);
	const groups = useMemo(function () { return groupsOf(document); }, [document]);
	const all = useMemo(function () { return operationsOf(document); }, [document]);

	/* Over the whole document and not over what is filtered. What a caller has
	   to carry is a fact about the surface: one route at system among ninety is
	   not interesting and ninety at read would be alarming, and a count that
	   moved with the filter would answer a different question every press. It
	   also keeps every level on screen, so the one that is chosen cannot be the
	   one that just vanished for having nothing under it. */
	const levels = useMemo(function () { return byLevel(all); }, [all]);

	/* Not a build flag this page could read: the box says whether it carries
	   the documentation, and the document in front of this screen says the same
	   thing more directly. A document with routes in it and not one word about
	   any of them is the build that left the prose out. */
	const bare = all.length > 0 && !all.some(function (one) { return one.summary !== ''; });

	const found = matching(all, query, group, level);
	const chosen = groups.filter(function (one) { return one.id === group; })[0];

	/** @type {Array<{ value: string, label: string }>} */
	const options = [{ value: '', label: counted(t(text, 'dev.api.group.all'), all.length) }];
	for (const one of groups)
		options.push({ value: one.id, label: counted(one.id, one.operations.length) });

	/** @type {Array<{ id: string, word: string, count: number }>} */
	const buttons = [{ id: '', word: t(text, 'dev.api.level.all'), count: all.length }];
	for (const one of levels)
		buttons.push({ id: one.id, word: levelWord(one.id), count: one.count });

	/* The last column is what the routes say they do, and a build without the
	   prose has no such thing to say. An empty column the width of the table is
	   how a page reports a switch as a fault, so it is not drawn at all, and
	   whichever column ends up last takes the width the table has over. */
	/** @type {Array<import('../../ui/table.js').Column<Operation>>} */
	const columns = [
		{ id: 'method', label: t(text, 'dev.api.method'),
		  cell: function (row) {
			return html`<span class="dev-method mono" data-method=${row.method}>${row.method}</span>`;
		  } },
		{ id: 'path', label: t(text, 'dev.api.path'), mono: true,
		  cell: function (row) { return row.path; } },
		{ id: 'level', label: t(text, 'dev.api.level'), wide: bare,
		  cell: function (row) { return levelWord(levelOf(row)); } }
	];
	if (!bare) {
		columns.push({ id: 'about', label: t(text, 'dev.api.about'), wide: true,
			cell: function (row) { return row.summary; } });
	}

	return html`<div class="dev">
		<section class="dev-block">
			<div class="dev-row">
				${/* NEITHER OF THESE IS A SCREEN OF THIS APPLICATION, and
				     both open in a tab of their own. The reader is a page of
				     its own on the box, it loads its own bundle, and drawing
				     it inside here would be this page carrying a second
				     application; the document is something to read beside
				     this page rather than instead of it. A tab of its own is
				     what keeps this screen, its filters and its scroll, where
				     somebody left them. */ null}
				${bare ? null : html`<a class="btn" href="/swagger/" target="_blank" rel="noopener">${t(text, 'dev.api.reader')}</a>`}
				<a class="btn" href=${kPath} target="_blank" rel="noopener">${t(text, 'dev.api.document')}</a>
			</div>
			<p class="dev-lead">${t(text, bare ? 'dev.api.bare' : 'dev.api.reader.hint')}</p>

			<p class="dev-lead">${t(text, 'dev.api.summary',
				{ count: all.length, groups: groups.length })} ${t(text, 'dev.api.weight')}</p>

			<div class="dev-filters">
				<label class="field">
					<span class="label">${t(text, 'dev.api.find')}</span>
					<input
						id="dev-find"
						type="search"
						value=${query}
						aria-describedby="dev-hits"
						onInput=${function (/** @type {Web.On<HTMLInputElement, InputEvent>} */ event) {
							setQuery(event.currentTarget.value);
						}} />
				</label>
				<${Select}
					id="dev-group"
					label=${t(text, 'dev.api.group')}
					value=${group}
					options=${options}
					onChange=${function (/** @type {Web.On<HTMLSelectElement>} */ event) {
						setGroup(event.currentTarget.value);
					}} />
			</div>

			<div class="dev-levels" role="group" aria-labelledby="dev-levels-label">
				<span class="label" id="dev-levels-label">${t(text, 'dev.api.levels')}</span>
				<div class="dev-level-row">
					${buttons.map(function (one) {
						const here = level === one.id;
						return html`<button
							key=${'level-' + one.id}
							type="button"
							class=${here ? 'btn on' : 'btn'}
							aria-pressed=${here ? 'true' : 'false'}
							onClick=${function () { setLevel(one.id); }}>${counted(one.word, one.count)}</button>`;
					})}
				</div>
			</div>

			${chosen && chosen.about !== '' ? html`<p class="dev-lead">${chosen.about}</p>` : null}
			<p class="dev-lead" id="dev-hits" role="status">${t(text, 'dev.api.hits',
				{ count: found.length, total: all.length })}</p>

			<${State}
				phase=${shot.phase}
				problem=${shot.error ? shot.error.problem : null}
				empty=${shot.state === 'ready' && all.length === 0}>
				${found.length === 0
					? html`<p class="note">${t(text, 'dev.api.none')}</p>`
					: html`<${Table}
						columns=${columns}
						rows=${found}
						rowKey=${function (/** @type {Operation} */ row) { return row.method + ' ' + row.path; }} />`}
			<//>
		</section>
	</div>`;
}

/* The settings, as one screen generated from what the box declares.
 *
 * The old interface carried twenty two hand written forms, one per page of the box's own
 * menu. This is one form: it reads the declaration the box writes about itself, draws a
 * control per row from the kind the row states, hides what the row's own comparisons say
 * to hide, and sends back the rows somebody changed. The screen is the same module for
 * every section, because a section is a name and not a layout.
 *
 * What is sent is only what was changed. A write carrying the whole section writes back
 * every value it read, and the value it read may be one somebody at the box changed while
 * this page sat open.
 *
 * A refused write puts the old value back.
 */
import { html, useState, useEffect, useMemo } from '../../runtime.js';
import * as store from '../../store.js';
import * as session from '../../session.js';
import { t } from '../../i18n.js';
import { hrefFor, labelOf } from '../../nav.js';
import { Button } from '../../ui/button.js';
import { State } from '../../ui/state.js';
import { Switch } from '../../ui/switch.js';
import { toast } from '../../ui/toast.js';
import { Row } from './rows.js';
import {
	numberFault,
	rowsOf,
	rowsOfSection,
	valuesOf,
	shownRows,
	shownValue,
	effective,
	changed,
	countOf,
	driftsFromDefault,
	search
} from './model.js';
import text from './settings.text.js';

export const css = '/app/screens/settings/settings.css';

/**
 * A section, as the document spells the one piece of the address this screen ever fills in.
 *
 * The document enumerates the sections this build declares, and the entry table beside this
 * file asks the running box which ones it carries. They are the same list on the same box,
 * and there is no way to show that to a checker, so the one place where a name the box gave
 * becomes a value for the template is here and nowhere else.
 *
 * @typedef {Api.PathValues<'GET /api/v1/settings/{section}'>['section']} SectionName
 */

/* Everything under the settings goes stale when one of them is written: the
   values of the section that was written, and the declaration itself, because
   a row's choices can be the box's own answer and a write can change what the
   box would answer. Named as the piece of the address it is about, which is
   how the store is told what to forget. */
const kTouched = ['/api/v1/settings/'];

/* Whether the form lists only what differs from the value the box ships.
 *
 * Beside the screen and not inside it. It is one way of reading the settings and not a
 * property of the section being read: somebody who turns it on is looking for what they
 * changed, and what they changed is spread over sixteen sections. Held in the screen it
 * was thrown away at every one of them, because this is the same module for all sixteen
 * and the change of name is what empties what was typed.
 *
 * Module state and not the browser's: a reload is the right amount of forgetting for a
 * way of looking at something.
 */
let onlyDrifting = false;

/**
 * What the words of the frame call a section, and the box's own name for it where this page
 * has never heard of it. A key with a dot in it is never put on screen: that is the defect
 * the resolving of these names removed.
 *
 * @param {string} id
 * @returns {string}
 */
export function sectionLabel(id) {
	return labelOf({ id: id, text: 'nav.settings.' + id, label: id });
}

/**
 * @param {{ param: string, ctx: unknown, entry: { id: string } }} props
 * @returns {Web.Drawn}
 */
export default function SettingsForm(props) {
	const section = /** @type {SectionName} */ (props.entry.id);

	/* The two answers are held as what they are on the wire and read by the
	   model beside this file, which is the one place that knows the shape of a
	   declaration. Everything this screen draws comes out of that reading, so a
	   member the box stops sending is a row that stops being drawn and never a
	   value read off undefined. */
	const [schema, setSchema] = useState(/** @type {Web.Snapshot<{ items?: unknown }> | null} */ (null));
	const [values, setValues] = useState(/** @type {Web.Snapshot<{ items?: unknown }> | null} */ (null));
	const [edits, setEdits] = useState(/** @type {Record<string, string>} */ ({}));
	const [query, setQuery] = useState('');
	const [driftOnly, holdDriftOnly] = useState(onlyDrifting);
	const [saving, setSaving] = useState(false);
	const [problem, setProblem] = useState(/** @type {Web.Failure | null} */ (null));
	/* Only a way to ask for another draw when what this request is granted
	   changes. What it is granted is kept in the session, where the whole page
	   reads it, so there is no second copy of it here. */
	const redraw = useState(0)[1];

	useEffect(function () {
		return store.watch('GET', '/api/v1/settings/schema', {}, function (snapshot) {
			setSchema(snapshot);
		});
	}, []);

	useEffect(function () {
		return store.watch('GET', '/api/v1/settings/{section}', { section: section }, function (snapshot) {
			setValues(snapshot);
		});
	}, [section]);

	// What was typed belongs to the section it was typed in. Carrying it to the
	// next one would write a value into whichever setting happens to share its
	// key there. The filter above is not in this list: it says how to read a
	// section and not what one holds.
	useEffect(function () {
		setEdits({});
		setProblem(null);
	}, [section]);

	useEffect(function () {
		return session.subscribe(function () {
			redraw(function (n) { return n + 1; });
		});
	}, []);

	const allRows = useMemo(function () {
		return rowsOf(schema === null ? null : schema.data);
	}, [schema === null ? null : schema.data]);

	const sectionRows = useMemo(function () {
		return rowsOfSection(allRows, section);
	}, [allRows, section]);

	const held = useMemo(function () {
		return valuesOf(values === null ? null : values.data, allRows);
	}, [values === null ? null : values.data, allRows]);

	const now = effective(held, edits);
	const visible = shownRows(sectionRows, now);
	const body = changed(sectionRows, held, edits);
	const pending = countOf(body);

	/* Nothing is said in advance about what this session may do. The frame
	   dropped its own sentence about that and this is the rest of it: the two
	   controls that want more than reading ask for it when they are pressed,
	   the sign in sheet comes up, and the write goes through behind it. A
	   sentence over a form nobody has touched explains a session to somebody
	   who came to read a setting. */

	/* A number that does not fit the bounds the row declares, held here and
	   only for what somebody typed. Not for what the box already holds: a
	   stored value outside its own bounds is the box's business and would
	   otherwise lock this screen for good. And not a second rulebook either,
	   because these are the same two numbers the row states and the field is
	   already drawn with. */
	const badValues = visible.filter(function (row) {
		const typed = edits[row.id];
		return typed !== undefined && numberFault(row, typed) !== null;
	});

	/**
	 * @param {string} id
	 * @param {string} value
	 */
	function edit(id, value) {
		setEdits(function (was) {
			/** @type {Record<string, string>} */
			const next = {};
			for (const key of Object.keys(was))
				next[key] = /** @type {string} */ (was[key]);
			next[id] = value;
			return next;
		});
	}

	/** @param {string[]} ids */
	function forget(ids) {
		setEdits(function (was) {
			/** @type {Record<string, string>} */
			const next = {};
			for (const key of Object.keys(was)) {
				if (ids.indexOf(key) === -1)
					next[key] = /** @type {string} */ (was[key]);
			}
			return next;
		});
	}

	function save() {
		const sent = Object.keys(body);
		if (sent.length === 0 || saving)
			return;

		setSaving(true);
		setProblem(null);
		store.write('PATCH', '/api/v1/settings/{section}', {
			params: { section: section },
			/* The document does not describe this body yet. The writer builds one only out
			   of parameters a table declares and this handler reads its own
			   (src/httpd/doc/openapi.cpp, src/httpd/ep/ep_settings.cpp), so what is generated for
			   this route is "takes no body at all", where what it takes is one flat object
			   of the settings to write. */
			body: /** @type {Api.BodyValue<'PATCH /api/v1/settings/{section}'>} */ (/** @type {unknown} */ (body)),
			touches: kTouched
		}).then(function (answer) {
			setSaving(false);
			const said = /** @type {{ items?: unknown, results?: unknown } | null} */ (answer);
			const results = said && said.results !== null && typeof said.results === 'object'
				? /** @type {Record<string, { status?: unknown, detail?: unknown }>} */ (said.results)
				: null;

			if (results === null) {
				// Every one of them landed, and the answer is the section as it
				// reads now, which is what a caller asking again would get.
				forget(sent);
				/* The same gap on the answer's side: the table declares no
				   shape for this route, so the document says it answers
				   nothing, while what it answers when every key landed is the
				   section as it reads now. Kept rather than asked for again,
				   because a read placed after a write is a second answer to a
				   question that has just been answered. */
				store.put('GET', '/api/v1/settings/{section}', { section: section },
					/** @type {Api.Result<'GET /api/v1/settings/{section}'>} */ (/** @type {unknown} */ (answer)));
				toast(t(text, 'settings.saved'));
				return;
			}

			/* Some landed and some did not. What landed is forgotten so the
			   form stops offering to send it again; what did not stays where it
			   is, with what the box said about it beside it, because a value
			   silently dropped is one nobody knows was refused. */
			const landed = [];
			const refused = [];
			for (const key of Object.keys(results)) {
				const one = results[key];
				const code = one === undefined ? 0 : Number(one.status);
				if (code >= 200 && code < 300) {
					landed.push(key);
					continue;
				}
				refused.push(key);
				const detail = one === undefined || typeof one.detail !== 'string' ? '' : one.detail;
				toast(t(text, 'settings.rejected', { id: key, detail: detail }), 'bad');
			}
			forget(landed);
			toast(t(text, 'settings.partly', { count: sent.length, failed: refused.length }), 'bad');
			store.reload('GET', '/api/v1/settings/{section}', { params: { section: section } });
		}, function (failed) {
			setSaving(false);
			const error = /** @type {Web.Failure | null} */ (failed);
			if (error !== null && error.aborted === true)
				return;
			// The box did not take it, so the form stops showing it as though it
			// had. What the box said is drawn as the box wrote it.
			forget(sent);
			setProblem(error);
			toast(t(text, 'settings.reverted'), 'bad');
		});
	}

	/* Back to what the box shipped, as an edit and not a write: it lands with
	   everything else the next time Save is pressed, so one row put back and
	   three rows changed are one request and one answer. */
	/** @param {import('./model.js').Row} row */
	function revert(row) {
		edit(row.id, row.fallback);
	}

	/** @param {import('./model.js').Row} row */
	function clearSecret(row) {
		store.write('POST', '/api/v1/settings/secret/clear', {
			body: { key: row.id },
			touches: kTouched
		}).then(function () {
			forget([row.id]);
			toast(t(text, 'settings.secret.cleared', { label: row.label }));
			store.reload('GET', '/api/v1/settings/{section}', { params: { section: section } });
		}, function (failed) {
			const error = /** @type {Web.Failure | null} */ (failed);
			if (error !== null && error.aborted === true)
				return;
			setProblem(error);
		});
	}

	if (schema !== null && schema.state === store.FAILED)
		return html`<${State} problem=${schema.error === null ? null : schema.error.problem} />`;
	if (values !== null && values.state === store.FAILED)
		return html`<${State} problem=${values.error === null ? null : values.error.problem} />`;
	if (schema === null || values === null || schema.data === null || values.data === null)
		return html`<${State} phase="first" />`;

	/* Counted over the whole section and not over what is listed, because the
	   number is what the switch below is for: hiding the rows that agree must
	   not change how many are said to disagree. */
	let driftCount = 0;
	for (const row of visible) {
		if (driftsFromDefault(row, now) === true)
			driftCount++;
	}

	const found = search(allRows, query);
	const searching = query.trim() !== '';
	const listed = searching
		? found
		: visible.filter(function (row) {
			return !driftOnly || driftsFromDefault(row, held) !== false;
		});

	/* Where a control is drawn and where only the way to one is. A search reaches the whole
	   declaration, and what it finds is mostly not on this page: this screen holds the values of
	   one section, so a control over a setting it has not read would show a blank as though that
	   were the setting. A hit whose comparisons do not hold is the same case for a different
	   reason. Both are drawn as a line and a way to the page they live on. */
		/** @type {Record<string, boolean>} */
	const editable = {};
	for (const row of visible)
		editable[row.id] = true;

	const phase = saving ? 'writing' : values.phase;

	/* A div and not a form, deliberately. The document forbids a form action altogether (the
	   policy in data/ni-web/index.html), so a form here is one that may never be submitted;
	   pressing return in the search field would submit it anyway and write every changed row;
	   and the browser's own validation of a number field refuses the submission with nothing on
	   screen to say why, which is the one failure a person cannot act on. */
	return html`<div class="settings">
		<header class="set-head">
			<p class="scr" id="set-count">${searching
				? t(text, 'settings.search.hits', { count: found.length, total: allRows.length })
				: t(text, 'settings.count', { count: visible.length, drifting: driftCount })}</p>
			<label class="field set-find">
				<span class="label">${t(text, 'settings.search.label')}</span>
				<input
					id="set-search"
					type="search"
					value=${query}
					aria-describedby="set-count"
					onInput=${function (/** @type {Event} */ event) {
						const field = /** @type {HTMLInputElement} */ (event.currentTarget);
						setQuery(field.value);
					}} />
			</label>
			${searching ? null : html`<${Switch}
				label=${t(text, 'settings.drift.label')}
				checked=${driftOnly}
				onChange=${function (/** @type {Event} */ event) {
					const box = /** @type {HTMLInputElement} */ (event.currentTarget);
					onlyDrifting = box.checked;
					holdDriftOnly(box.checked);
				}} />`}
		</header>

		${problem === null ? null : html`<${State} problem=${problem.problem} />`}

		<${State} phase=${phase}>
			${listed.length === 0
				? html`<p class="note">${searching
					? t(text, 'settings.search.none')
					: (driftOnly ? t(text, 'settings.drift.none') : t(text, 'settings.section.empty'))}</p>`
				: html`<div class="set-rows">${listed.map(function (row) {
					if (editable[row.id] !== true) {
						return html`<p class="set-elsewhere" key=${row.id} data-setting=${row.id}>
							<a href=${hrefFor('settings', row.section)}>${row.label}</a>
							<span class="hint mono">${row.id}</span>
							<span class="hint">${t(text, 'settings.section.of', { section: sectionLabel(row.section) })}</span>
						</p>`;
					}
					return html`<${Row}
						key=${row.id}
						row=${row}
						value=${shownValue(row, held, edits)}
						${/* Held against what is on screen and not against what
						     the box last said, so that a row put back stops
						     being marked the moment it is put back. The list
						     above is filtered on what the box said, which is
						     what keeps a row from vanishing while somebody is
						     typing in it. */ null}
						drifts=${driftsFromDefault(row, now) === true}
						place=${searching ? t(text, 'settings.section.of', { section: sectionLabel(row.section) }) : ''}
						onChange=${edit}
						onRevert=${revert}
						onClear=${function (/** @type {import('./model.js').Row} */ one) {
							session.requireSystem().then(function () { clearSecret(one); }, function () {});
						}} />`;
				})}</div>`}
		<//>

		<footer class="set-bar">
			<span class="hint" id="set-pending">${t(text, 'settings.pending', { count: pending })}</span>
			<${Button} onClick=${function () { forget(Object.keys(edits)); }} disabled=${pending === 0}>
				${t(text, 'settings.revert')}
			<//>
			<${Button}
				primary=${true}
				reason=${badValues.length > 0 ? t(text, 'settings.badvalues') : ''}
				disabled=${pending === 0 || saving}
				onClick=${function () {
					session.requireWrite().then(save, function () {
						// The sheet was closed. What was typed is still on the
						// screen and Save is still there to be pressed again.
					});
				}}>
				${t(text, 'settings.save')}
			<//>
		</footer>
	</div>`;
}

/* The mounts the box is to make for itself, as against the ones it has made.
 *
 * TWO FILES AND EIGHT SLOTS IN EACH, WHICH IS THE SHAPE AND NOT A LIMIT THIS SCREEN
 * CHOSE. The box keeps these mounts in /var/etc/fstab and /var/etc/auto.net, both of
 * them eight entries long, because the screen at the box writes exactly eight and
 * rewrites the whole file every time it saves. A ninth entry written from here would
 * be dropped by that screen without a word. So what a write addresses is a slot, and
 * an empty slot is a thing to fill in rather than an absence (src/coreapi/netfs.h).
 *
 * A CARD PER ENTRY AND NOT A ROW, which is the one place in this interface where a
 * listing is not a table. An entry has eight fields, and eight columns is a row
 * nobody reads across even where there is room for it.
 *
 * A PASSWORD IS NEVER ON THIS SCREEN. The files hold one in clear, the kernel taking
 * it no other way, so the read answers whether there is one and never which one, and
 * it is the floor under the routes that withholds it rather than the routes. From
 * that follow the two rules the form states: a password field left alone means the
 * stored one stays, and taking one off a mount is done by emptying the whole slot
 * (src/httpd/ep/ep_netfs.cpp).
 *
 * ALL OF IT IS SYSTEM, THE READING TOO. What a read answers is the address of
 * somebody's storage on their own network, the shares on it and the name they log in
 * under. So arriving here is what raises the sign in sheet, and nothing is asked of
 * the box until the box has said this request carries the level.
 */

import { html, useState, useEffect, Link } from '../../runtime.js';
import * as store from '../../store.js';
import * as session from '../../session.js';
import { t } from '../../i18n.js';
import { hrefFor } from '../../nav.js';
import { Button } from '../../ui/button.js';
import { Dialog } from '../../ui/dialog.js';
import { Field } from '../../ui/field.js';
import { RowActions } from '../../ui/actions.js';
import { Select } from '../../ui/select.js';
import { Sheet } from '../../ui/sheet.js';
import { State } from '../../ui/state.js';
import { StateChip } from '../../ui/dot.js';
import { Switch } from '../../ui/switch.js';
import { toast } from '../../ui/toast.js';
import { problemOf } from './answer.js';
import text from './netfs.text.js';

export const css = '/app/screens/files/netfs.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'netfs.what'); }


/**
 * A slot as the form holds it while somebody is typing in it.
 *
 * The password is a member of its own and starts empty whatever the entry carries,
 * because the read never hands one over. It is sent only where something was typed.
 *
 * @typedef {object} Draft
 * @property {'fstab' | 'automount'} table
 * @property {number} slot
 * @property {boolean} active
 * @property {'nfs' | 'cifs'} type
 * @property {string} host
 * @property {string} remote_dir
 * @property {string} local_dir
 * @property {string} user
 * @property {string} password
 * @property {boolean} had_password
 * @property {string} options
 */

/**
 * @param {'fstab' | 'automount'} table
 * @param {Api.NetfsEntry} entry
 * @returns {Draft}
 */
export function draftOf(table, entry) {
	return {
		table: table,
		slot: entry.slot,
		active: entry.active,
		type: entry.type,
		host: entry.host,
		remote_dir: entry.remote_dir,
		local_dir: entry.local_dir,
		user: entry.user,
		password: '',
		had_password: entry.has_password,
		options: entry.options,
	};
}

/* A slot nobody has filled in. The box writes a placeholder into every column of
   such a line, a line with a column missing being one whose later columns are read
   as the wrong things, and the reader hands those placeholders back as the nothing
   they stand for. */
/**
 * @param {Api.NetfsEntry} entry
 * @returns {boolean}
 */
export function isFree(entry) {
	return entry.host === '' && entry.remote_dir === '' && entry.local_dir === '';
}

/**
 * What is wrong with a draft, as the keys of the sentences for it.
 *
 * Keys and not the sentences themselves, so what the form marks is decided by which
 * field is wrong and not by two sentences happening to read alike.
 *
 * Three fields and no more. Everything else that would not survive the file is
 * refused by the layer that owns the file, in one place.
 *
 * @param {Draft} draft
 * @returns {string[]}
 */
export function draftProblems(draft) {
	/** @type {string[]} */
	const wrong = [];
	if (draft.host.trim() === '')
		wrong.push('netfs.bad.host');
	if (draft.remote_dir.trim() === '')
		wrong.push('netfs.bad.remote');
	if (draft.local_dir.trim() === '')
		wrong.push('netfs.bad.local');
	return wrong;
}

/**
 * The body of a write, with the password in it only where one was typed.
 *
 * @typedef {{
 *   active: boolean, type: 'nfs' | 'cifs', host: string, remote_dir: string,
 *   local_dir: string, user: string, options: string, password?: string
 * }} WriteBody
 */

/**
 * @param {Draft} draft
 * @returns {WriteBody}
 */
export function bodyOf(draft) {
	/** @type {WriteBody} */
	const body = {
		active: draft.active,
		type: draft.type,
		host: draft.host.trim(),
		remote_dir: draft.remote_dir.trim(),
		local_dir: draft.local_dir.trim(),
		user: draft.user.trim(),
		options: draft.options.trim(),
	};
	if (draft.password !== '')
		body.password = draft.password;
	return body;
}

/* The server and what it offers, written the way the kind writes it.

   The two halves reach this page apart, because that is how a write takes them.
   Putting them back together is the one place the two spellings are stated here,
   and they are the two the box's own reader takes: a colon between them for the
   one kind, and two separators in front for the other (src/coreapi/netfs.cpp,
   serverText). */
/**
 * @param {Api.NetfsEntry} entry
 * @returns {string}
 */
export function remoteText(entry) {
	if (entry.type !== 'cifs')
		return entry.host + ':' + entry.remote_dir;
	const share = entry.remote_dir.indexOf('/') === 0 ? entry.remote_dir : '/' + entry.remote_dir;
	return '//' + entry.host + share;
}

/**
 * What one entry is called where a sheet or a question has to name it.
 *
 * @param {Api.NetfsEntry} entry
 * @returns {string}
 */
export function nameOf(entry) {
	if (isFree(entry))
		return t(text, 'netfs.slot', { number: entry.slot + 1 });
	return remoteText(entry);
}

/**
 * The word for a file, which is what a sheet and a heading name it by.
 *
 * @param {'fstab' | 'automount'} table
 * @returns {string}
 */
function fileName(table) {
	return t(text, table === 'automount' ? 'netfs.automount.name' : 'netfs.fstab.name');
}

/**
 * What the box said about a refusal, and never a sentence invented here.
 *
 * @param {unknown} caught
 * @returns {string}
 */
function refusedWith(caught) {
	const problem = problemOf(caught);
	return problem.detail || problem.title;
}

/**
 * One file of the two, watched, and only once the box has said this request
 * carries System. Asking before that would draw the box's refusal where the
 * entries belong, and that refusal would still be standing after a sign in.
 *
 * @param {'fstab' | 'automount'} table
 * @param {boolean} ask
 * @returns {Web.Snapshot<Api.NetfsTable> | null}
 */
function useTable(table, ask) {
	const [shot, setShot] = useState(/** @type {Web.Snapshot<Api.NetfsTable> | null} */ (null));
	useEffect(function () {
		if (!ask) {
			setShot(null);
			return undefined;
		}
		return store.watch('GET', '/api/v1/storage/netfs/{table}',
			{ params: { table: table } }, setShot);
	}, [table, ask]);
	return shot;
}

/* One entry, drawn.

   Written out here and not inside the screen, for a reason that costs an evening
   when it is got wrong: a component declared inside another one is a different
   function on every draw, so what it drew is thrown away and made again each time,
   and the sheet somebody had open at the end of a row closes under their hand. */
/**
 * @param {{
 *   table: 'fstab' | 'automount',
 *   entry: Api.NetfsEntry,
 *   onEdit: (table: 'fstab' | 'automount', entry: Api.NetfsEntry) => void,
 *   onClear: (table: 'fstab' | 'automount', entry: Api.NetfsEntry) => void
 * }} props
 * @returns {Web.Drawn}
 */
function Slot(props) {
	const entry = props.entry;
	const number = entry.slot + 1;

	if (isFree(entry)) {
		return html`<li class="netfs-slot netfs-free">
			<span>${t(text, 'netfs.slot.free', { number: number })}</span>
			<${Button} onClick=${function () { props.onEdit(props.table, entry); }}>
				${t(text, 'netfs.fill')}
			<//>
		</li>`;
	}

	return html`<li class="netfs-slot">
		<div class="netfs-slot-head">
			<${StateChip}
				kind=${entry.active ? 'running' : ''}
				tone=${entry.active ? 'good' : ''}
				word=${t(text, entry.active ? 'netfs.on' : 'netfs.off')} />
			<span class="chip">${t(text, 'netfs.type.' + entry.type)}</span>
			<span class="netfs-number">${t(text, 'netfs.slot', { number: number })}</span>
			<span class="acts"><${RowActions}
				title=${nameOf(entry)}
				actions=${[
					{
						id: 'edit', label: t(text, 'netfs.edit'), mark: '✎',
						onAct: function () { props.onEdit(props.table, entry); },
					},
					{
						id: 'clear', label: t(text, 'netfs.clear'), mark: '✕',
						onAct: function () { props.onClear(props.table, entry); },
					},
				]} /></span>
		</div>
		<dl class="netfs-facts">
			<div>
				<dt>${t(text, 'netfs.remote')}</dt>
				<dd class="mono">${remoteText(entry)}</dd>
			</div>
			<div>
				<dt>${t(text, props.table === 'automount' ? 'netfs.local.auto' : 'netfs.local')}</dt>
				<dd class="mono">${entry.local_dir}</dd>
			</div>
			<div>
				<dt>${t(text, 'netfs.user')}</dt>
				<dd>${entry.user || t(text, 'netfs.user.none')}</dd>
			</div>
			<div>
				<dt>${t(text, 'netfs.password')}</dt>
				<dd>${t(text, entry.has_password ? 'netfs.password.set' : 'netfs.password.none')}</dd>
			</div>
			<div>
				<dt>${t(text, 'netfs.options')}</dt>
				<dd class="mono">${entry.options || t(text, 'netfs.options.none')}</dd>
			</div>
		</dl>
	</li>`;
}

/**
 * One of the two files, with its acts above it and its eight slots under it.
 *
 * @param {{
 *   table: 'fstab' | 'automount',
 *   ask: boolean,
 *   ran: string | undefined,
 *   acts: unknown,
 *   onEdit: (table: 'fstab' | 'automount', entry: Api.NetfsEntry) => void,
 *   onClear: (table: 'fstab' | 'automount', entry: Api.NetfsEntry) => void
 * }} props
 * @returns {Web.Drawn}
 */
function FileSection(props) {
	const auto = props.table === 'automount';
	const shot = useTable(props.table, props.ask);
	const held = shot && shot.data;
	const failed = shot && shot.error ? problemOf(shot.error) : null;
	const broken = held ? held.unreadable_lines : 0;

	return html`<section class="netfs-file">
		<div class="netfs-file-head">
			<h3>${fileName(props.table)}</h3>
			<span class="chip mono">${held ? held.path : ''}</span>
			<span class="netfs-file-acts">${props.acts}</span>
		</div>
		<p class="files-hint">${t(text, auto ? 'netfs.automount.what' : 'netfs.fstab.what')}</p>

		${broken > 0
			? html`<p class="netfs-broken" role="status">
				${t(text, broken === 1 ? 'netfs.unreadable' : 'netfs.unreadable.many', { count: broken })}
			</p>`
			: null}

		${props.ran === undefined
			? null
			: html`<div class="netfs-ran">
				<h4>${t(text, 'netfs.ran')}</h4>
				<pre>${props.ran}</pre>
			</div>`}

		<${State} problem=${failed} phase=${shot ? shot.phase : ''}>
			${held
				? html`<ul class="netfs-slots">
					${held.items.map(function (/** @type {Api.NetfsEntry} */ entry) {
						return html`<${Slot}
							key=${entry.slot}
							table=${props.table}
							entry=${entry}
							onEdit=${props.onEdit}
							onClear=${props.onClear} />`;
					})}
				</ul>`
				: null}
		<//>
	</section>`;
}

/**
 * The eight fields of one slot, in the one place they are asked for.
 *
 * @param {{
 *   draft: Draft,
 *   wrong: readonly string[],
 *   refusal: string,
 *   sending: boolean,
 *   onChange: (patch: Partial<Draft>) => void,
 *   onSave: () => void,
 *   onClose: () => void
 * }} props
 * @returns {Web.Drawn}
 */
function SlotForm(props) {
	const draft = props.draft;
	const auto = draft.table === 'automount';
	const title = t(text, 'netfs.edit.title', {
		number: draft.slot + 1,
		file: fileName(draft.table),
	});

	/**
	 * @param {string} key
	 * @returns {string}
	 */
	function errorOf(key) {
		return props.wrong.indexOf(key) === -1 ? '' : t(text, key);
	}

	return html`<${Sheet} open=${true} label=${title} onClose=${props.onClose}>
		<h2>${title}</h2>

		${props.refusal ? html`<p class="netfs-refusal" role="alert">${props.refusal}</p>` : null}

		<${Switch}
			label=${t(text, 'netfs.active')}
			checked=${draft.active}
			onChange=${function (/** @type {Web.On<HTMLInputElement>} */ e) { props.onChange({ active: e.currentTarget.checked }); }} />

		<${Select}
			label=${t(text, 'netfs.type')}
			value=${draft.type}
			options=${[
				{ value: 'nfs', label: t(text, 'netfs.type.nfs') },
				{ value: 'cifs', label: t(text, 'netfs.type.cifs') },
			]}
			onChange=${function (/** @type {Web.On<HTMLSelectElement>} */ e) {
				props.onChange({ type: e.currentTarget.value === 'cifs' ? 'cifs' : 'nfs' });
			}} />

		<${Field}
			label=${t(text, 'netfs.host')}
			value=${draft.host}
			hint=${t(text, 'netfs.host.hint')}
			error=${errorOf('netfs.bad.host')}
			onInput=${function (/** @type {Web.On<HTMLInputElement, InputEvent>} */ e) { props.onChange({ host: e.currentTarget.value }); }} />

		<${Field}
			label=${t(text, 'netfs.remotedir')}
			value=${draft.remote_dir}
			hint=${t(text, 'netfs.remotedir.hint')}
			error=${errorOf('netfs.bad.remote')}
			onInput=${function (/** @type {Web.On<HTMLInputElement, InputEvent>} */ e) { props.onChange({ remote_dir: e.currentTarget.value }); }} />

		<${Field}
			label=${t(text, auto ? 'netfs.local.auto' : 'netfs.local')}
			value=${draft.local_dir}
			hint=${t(text, auto ? 'netfs.localdir.auto.hint' : 'netfs.localdir.hint')}
			error=${errorOf('netfs.bad.local')}
			onInput=${function (/** @type {Web.On<HTMLInputElement, InputEvent>} */ e) { props.onChange({ local_dir: e.currentTarget.value }); }} />

		<${Field}
			label=${t(text, 'netfs.user')}
			value=${draft.user}
			autocomplete="off"
			onInput=${function (/** @type {Web.On<HTMLInputElement, InputEvent>} */ e) { props.onChange({ user: e.currentTarget.value }); }} />

		<${Field}
			label=${t(text, 'netfs.password.field')}
			type="password"
			value=${draft.password}
			autocomplete="new-password"
			hint=${t(text, draft.had_password ? 'netfs.password.keep' : 'netfs.password.new')}
			onInput=${function (/** @type {Web.On<HTMLInputElement, InputEvent>} */ e) { props.onChange({ password: e.currentTarget.value }); }} />

		<${Field}
			label=${t(text, 'netfs.options')}
			value=${draft.options}
			hint=${t(text, 'netfs.options.hint')}
			onInput=${function (/** @type {Web.On<HTMLInputElement, InputEvent>} */ e) { props.onChange({ options: e.currentTarget.value }); }} />

		<p class="netfs-form-acts">
			<${Button} onClick=${props.onClose}>${t(text, 'netfs.cancel')}<//>
			${' '}
			<${Button} primary=${true} disabled=${props.sending} onClick=${props.onSave}>
				${t(text, 'netfs.save')}
			<//>
		</p>
	<//>`;
}

/** @returns {Web.Drawn} */
export default function NetFs() {
	/* Kept only so that the two files are asked for the moment a sign in makes
	   it possible. Which level admits which is asked of the session layer, in
	   one place, and never worked out from this. */
	const [, setGranted] = useState(session.state());
	useEffect(function () {
		return session.subscribe(setGranted);
	}, []);
	const mayRead = session.atLeast('system');

	/* Arriving is the act, this whole screen being System, so the sheet is
	   raised on the way in rather than a sentence being drawn in its place.
	   Somebody who closes it is left with the way back in below the heading. */
	useEffect(function () {
		if (mayRead)
			return;
		session.requireSystem().catch(function () { });
	}, [mayRead]);

	const [editing, setEditing] = useState(/** @type {Draft | null} */ (null));
	const [wrong, setWrong] = useState(/** @type {string[]} */ ([]));
	const [refusal, setRefusal] = useState('');
	const [sending, setSending] = useState(false);
	const [doomed, setDoomed] = useState(/** @type {{ table: 'fstab' | 'automount', entry: Api.NetfsEntry } | null} */ (null));
	/* What the box's own script last wrote, kept per file. It is the whole of
	   what an act here has to say about itself: what came of the mounting is
	   read off what is mounted, which is the screen beside this one. */
	const [said, setSaid] = useState(/** @type {Record<string, string>} */ ({}));

	/**
	 * @param {() => void} act
	 * @returns {void}
	 */
	function whenAllowed(act) {
		session.requireSystem().then(act, function () { });
	}

	/**
	 * @param {'fstab' | 'automount'} table
	 * @param {Api.NetfsEntry} entry
	 * @returns {void}
	 */
	function edit(table, entry) {
		whenAllowed(function () {
			setWrong([]);
			setRefusal('');
			setEditing(draftOf(table, entry));
		});
	}

	/**
	 * @param {'fstab' | 'automount'} table
	 * @param {Api.NetfsEntry} entry
	 * @returns {void}
	 */
	function ask(table, entry) {
		whenAllowed(function () { setDoomed({ table: table, entry: entry }); });
	}

	/**
	 * @param {Partial<Draft>} patch
	 * @returns {void}
	 */
	function change(patch) {
		setEditing(function (was) {
			return was === null ? was : Object.assign({}, was, patch);
		});
	}

	/**
	 * What the box now holds, put back from the answer rather than asked for
	 * again: both writes rewrite the whole file and answer the whole file.
	 *
	 * @param {'fstab' | 'automount'} table
	 * @param {Api.NetfsTable} now
	 * @returns {void}
	 */
	function take(table, now) {
		store.put('GET', '/api/v1/storage/netfs/{table}', { params: { table: table } }, now);
	}

	/** @returns {Promise<void>} */
	async function save() {
		const draft = editing;
		if (draft === null || sending)
			return;
		const problems = draftProblems(draft);
		setWrong(problems);
		if (problems.length > 0)
			return;

		setSending(true);
		setRefusal('');
		try {
			const now = await store.write('PUT', '/api/v1/storage/netfs/{table}/{slot}', {
				params: { table: draft.table, slot: draft.slot },
				body: bodyOf(draft),
				touches: ['/api/v1/storage/netfs'],
			});
			take(draft.table, now);
			setEditing(null);
			toast(t(text, 'netfs.saved', { number: draft.slot + 1 }));
		} catch (caught) {
			setRefusal(refusedWith(caught));
		}
		setSending(false);
	}

	/**
	 * @param {'fstab' | 'automount'} table
	 * @param {Api.NetfsEntry} entry
	 * @returns {Promise<void>}
	 */
	async function empty(table, entry) {
		setDoomed(null);
		try {
			const now = await store.write('DELETE', '/api/v1/storage/netfs/{table}/{slot}', {
				params: { table: table, slot: entry.slot },
				touches: ['/api/v1/storage/netfs'],
			});
			take(table, now);
			toast(t(text, 'netfs.cleared', { number: entry.slot + 1 }));
		} catch (caught) {
			toast(refusedWith(caught), 'bad');
		}
	}

	/**
	 * One of the three acts, each of which drives the box's own start up script.
	 * What the script wrote is shown as it wrote it, and what the box has mounted
	 * is asked for again, that being where the outcome shows.
	 *
	 * @param {string} which
	 * @param {Promise<Api.NetfsRun>} running
	 * @returns {void}
	 */
	function afterRun(which, running) {
		running.then(function (answer) {
			setSaid(function (was) {
				const next = Object.assign({}, was);
				next[which] = answer.output === '' ? t(text, 'netfs.ran.silent') : answer.output;
				return next;
			});
			/* Thrown away rather than asked for: what a mount changes is what the
			   screen next door draws, and that screen asks again for itself if
			   somebody is on it. Asking from here would cost the box an answer
			   nobody is waiting for. */
			store.invalidate('/api/v1/storage/mounts');
		}, function (caught) {
			toast(refusedWith(caught), 'bad');
		});
	}

	/** @returns {void} */
	function mountAll() {
		whenAllowed(function () {
			afterRun('fstab', store.write('POST', '/api/v1/storage/netfs/fstab/mount', {
				touches: ['/api/v1/storage/mounts'],
			}));
		});
	}

	/** @returns {void} */
	function unmountAll() {
		whenAllowed(function () {
			afterRun('fstab', store.write('POST', '/api/v1/storage/netfs/fstab/unmount', {
				touches: ['/api/v1/storage/mounts'],
			}));
		});
	}

	/** @returns {void} */
	function reloadAutomounter() {
		whenAllowed(function () {
			afterRun('automount', store.write('POST', '/api/v1/storage/netfs/automount/reload', {
				touches: ['/api/v1/storage/mounts'],
			}));
		});
	}

	return html`<section class="netfs">
		<p class="files-hint">
			${t(text, 'netfs.rewrite')}
			${' '}
			<${Link} href=${hrefFor('files', 'mounts')}>${t(text, 'netfs.tomounts')}<//>
		</p>

		${/* The way back in for somebody who closed the sheet. It asks for the
		     level and nothing else: what happens when it is granted is that the
		     session says so, this draws again, and the two files are asked for. */
			mayRead
				? null
				: html`<p class="netfs-way-in">
					<${Button} primary=${true} onClick=${function () { session.requireSystem().catch(function () { }); }}>
						${t(text, 'netfs.signin')}
					<//>
				</p>`}

		<${FileSection}
			table="fstab"
			ask=${mayRead}
			ran=${said['fstab']}
			onEdit=${edit}
			onClear=${ask}
			acts=${html`<span class="acts-row">
				<${Button} onClick=${mountAll}>${t(text, 'netfs.mountall')}<//>
				${' '}
				<${Button} onClick=${unmountAll}>${t(text, 'netfs.unmountall')}<//>
			</span>`} />

		<${FileSection}
			table="automount"
			ask=${mayRead}
			ran=${said['automount']}
			onEdit=${edit}
			onClear=${ask}
			acts=${html`<${Button} onClick=${reloadAutomounter}>${t(text, 'netfs.reload')}<//>`} />

		${editing === null
			? null
			: html`<${SlotForm}
				draft=${editing}
				wrong=${wrong}
				refusal=${refusal}
				sending=${sending}
				onChange=${change}
				onSave=${save}
				onClose=${function () { setEditing(null); }} />`}

		<${Dialog}
			open=${doomed !== null}
			title=${t(text, 'netfs.clear.title')}
			confirmLabel=${t(text, 'netfs.clear')}
			onCancel=${function () { setDoomed(null); }}
			onConfirm=${function () { if (doomed !== null) empty(doomed.table, doomed.entry); }}>
			<p>${doomed === null ? '' : t(text, 'netfs.clear.body', { what: nameOf(doomed.entry) })}</p>
		<//>
	</section>`;
}

/* What one directory on the box holds.
 *
 * The level is the first thing this screen draws, and it is why the destination has two
 * screens instead of one. The mount points are Read, so anybody on the home network sees
 * how much room the box has; a listing is System, because it hands over the names, the
 * modes, the owners and the sizes of what is on the disks, and the directory the server
 * keeps its own configuration in is among them.
 *
 * A directory is an address, and it sits in the query rather than in the path. An address
 * so that a listing can be typed, sent to somebody and walked by a check; the query
 * because an escaped separator inside a path is a separator to this server and a target
 * carrying one is refused before anything reads it. The measurement and the two lines of
 * the server it comes from are written out beside dirHref in paths.js.
 *
 * What is read back after a write, and why here of all places. Nothing on the stream says
 * a directory changed, so the rule the store is built on, that a page believes what it did
 * and lets the event correct it, has nothing to correct it with here. What these three
 * routes have instead is that they answer after the filesystem has. */

import { html, useState, useEffect, Link, route, getCurrentUrl } from '../../runtime.js';
import { buildUrl } from '../../api.js';
import * as store from '../../store.js';
import * as session from '../../session.js';
import { t } from '../../i18n.js';
import { bytes, dayAndClock } from '../../fmt.js';
import { Button } from '../../ui/button.js';
import { RowActions } from '../../ui/actions.js';
import { Dialog } from '../../ui/dialog.js';
import { Field } from '../../ui/field.js';
import { State } from '../../ui/state.js';
import { Table } from '../../ui/table.js';
import { toast } from '../../ui/toast.js';
import { hrefFor } from '../../nav.js';
import text from './files.text.js';
import { problemOf, isOutOfReach } from './answer.js';
import { Upload } from './upload.js';
import { FilePlay } from './play.js';
import { useMounts } from './mounts.js';
import {
	tidy, parentOf, joinPath, crumbsOf, mediaKind, byKindThenName, dirHref, dirInUrl,
} from './paths.js';

export const css = '/app/screens/files/files.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'files.head'); }


/**
 * @param {string} dir
 * @param {boolean} ask
 * @returns {Web.Snapshot<Api.FileList> | null}
 */
function useListing(dir, ask) {
	const [shot, setShot] = useState(/** @type {Web.Snapshot<Api.FileList> | null} */ (null));
	useEffect(function () {
		if (!ask || dir === '') {
			setShot(null);
			return undefined;
		}
		return store.watch('GET', '/api/v1/storage/files', { query: { path: dir } }, setShot);
	}, [dir, ask]);
	return shot;
}

/**
 * @param {Api.File[]} rows
 * @param {{ column: string, dir: string } | null} sort
 * @returns {Api.File[]}
 */
function sortRows(rows, sort) {
	const out = rows.slice();
	const column = sort ? sort.column : 'name';
	const backwards = !!sort && sort.dir === 'desc';
	out.sort(function (a, b) {
		// Directories first in every ordering: that is the shape of the thing
		// and not one way of looking at it.
		const kinds = (a.kind === 'dir' ? 0 : 1) - (b.kind === 'dir' ? 0 : 1);
		if (kinds !== 0) {
			return kinds;
		}
		let by = 0;
		if (column === 'size') {
			by = numberOf(a.size) - numberOf(b.size);
		} else if (column === 'changed') {
			by = numberOf(a.mtime) - numberOf(b.mtime);
		} else {
			by = byKindThenName(a, b);
		}
		return backwards ? -by : by;
	});
	return out;
}

// A member the box leaves out for a name the filesystem would not answer for,
// as a number to sort by. Nought and not the value it would have had: the
// listing says through attributes_read that nobody measured this one.
/** @param {unknown} value @returns {number} */
function numberOf(value) {
	return typeof value === 'number' && Number.isFinite(value) ? value : 0;
}

/** @param {string} path @returns {string} */
function downloadHref(path) {
	/* Built by the one file that builds addresses, and then handed to the browser as
	   a link rather than fetched.

	   A link, because the answer carries the name in a header and the browser saves it
	   under that name; and because a recording runs to gigabytes, which is a number of
	   bytes this page must never hold in order to hand them on. The same link is what
	   lets a browser ask for one stretch of a file and carry on after a break.

	   NO CREDENTIAL IN IT. Every address on this list is followed by this browser, which
	   attaches the session cookie by itself, so a token written in here would be a secret
	   in a history list bought for nothing. The one address that needs one is the one
	   meant to leave this browser, and that one is drawn below. */
	return buildUrl('/api/v1/storage/file', null, { path: path });
}

/**
 * The same file as an address another program can be given, which is a different thing
 * from the address above and is why it is fetched rather than assembled.
 *
 * A player sets no header and carries no cookie, so the address has to carry the
 * credential itself; the session layer draws one that is worth the media of this box
 * and nothing else, and it draws one for the page rather than one per row. Asked for when
 * a row is opened and not before: a row nobody opened is a token nobody needed.
 *
 * Empty until it arrives, and empty again if the box will not draw one, which is what the
 * control below reads to decide whether to offer an address at all. Showing the address
 * without the credential would be showing the address that answers 403, which is the
 * defect this is here to close.
 *
 * @param {string} path
 * @returns {string}
 */
function useOutsideHref(path) {
	const [href, setHref] = useState('');
	useEffect(function () {
		if (path === '') {
			setHref('');
			return undefined;
		}
		let wanted = true;
		setHref('');
		session.outsideAddress(path).then(function (address) {
			if (wanted) {
				setHref(address);
			}
		}).catch(function () {
			// The control says what it can do and offers nothing it cannot.
		});
		return function () { wanted = false; };
	}, [path]);
	return href;
}

// The one spelling of this screen's own address, so the trail, the rows, the
// mount points and the form cannot disagree about where a directory lives.
/** @param {string} dir @returns {string} */
function listHref(dir) {
	return dirHref(hrefFor('files', 'list'), dir);
}

/** @returns {Web.Drawn} */
export default function Files() {
	/* Read off the address rather than handed in: the frame hands a screen the
	   third segment of the path and this screen's directory is in the query,
	   which nothing above it reads. The router redraws the whole frame on every
	   change of address, so this is read again whenever it can have changed,
	   the browser's own back button included. */
	const dir = dirInUrl(getCurrentUrl());
	/* What this request was granted, kept only so that the listing is asked for
	   the moment a sign in makes it possible and not one address later.
	   Which level admits which is asked of the session layer and never worked
	   out from this. */
	const [, setGranted] = useState(session.state());
	useEffect(function () {
		return session.subscribe(setGranted);
	}, []);
	const mayList = session.atLeast('system');
	const shot = useListing(dir, mayList);
	/* The starting points, which need only what the home network is granted.
	   Asked for whenever this screen is showing them, which is at the root and
	   also at a directory nobody may list yet. */
	const mounts = useMounts(dir === '' || !mayList);

	/* A directory in the address and no System yet: the sheet, now, because
	   arriving at such an address is the act. This screen used to draw a panel
	   in its place saying a sign in was needed, which is the page refusing on
	   the box's behalf; what it does instead is offer the sign in and then draw
	   the directory. Somebody who closes the sheet is left on the screen with
	   the starting points, which is the honest thing to be left on. */
	useEffect(function () {
		if (dir === '' || mayList)
			return;
		session.requireSystem().catch(function () { });
	}, [dir, mayList]);

	const [typed, setTyped] = useState(dir);
	const [sort, setSort] = useState(/** @type {{ column: string, dir: string } | null} */ (null));
	const [making, setMaking] = useState(false);
	const [madeName, setMadeName] = useState('');
	const [madeError, setMadeError] = useState('');
	const [doomed, setDoomed] = useState(/** @type {string} */ (''));
	const [playing, setPlaying] = useState(/** @type {string} */ (''));
	/* Drawn for the row that is open and for no other, so the box draws a credential
	   when somebody asks to watch something and never because a directory was listed. */
	const outside = useOutsideHref(playing === '' ? '' : joinPath(dir, playing));
	// A name the box refused to reach, kept for as long as this screen is open
	// so that the row goes on saying so instead of offering the same refusal
	// again. Learned from the box and never written down in front.
	const [closed, setClosed] = useState(/** @type {Record<string, boolean>} */ ({}));

	// The field follows the address, so walking into a directory and then
	// typing carries on from where the person is.
	useEffect(function () {
		setTyped(dir);
		setPlaying('');
		setClosed({});
	}, [dir]);

	function again() {
		store.reload('GET', '/api/v1/storage/files', { query: { path: dir } });
	}

	/** @param {unknown} caught @param {string} name */
	function refused(caught, name) {
		const said = problemOf(caught);
		if (isOutOfReach(said)) {
			setClosed(function (was) {
				const next = Object.assign({}, was);
				next[name] = true;
				return next;
			});
			return;
		}
		toast(said.detail || said.title, 'bad');
	}

	async function make() {
		const name = madeName.trim();
		if (name === '') {
			setMadeError(t(text, 'files.mkdir.empty'));
			return;
		}
		if (name.indexOf('/') !== -1) {
			setMadeError(t(text, 'files.mkdir.slash'));
			return;
		}
		try {
			await store.write('POST', '/api/v1/storage/directory', {
				body: { path: joinPath(dir, name) },
				touches: ['/api/v1/storage/files'],
			});
			setMaking(false);
			setMadeName('');
			setMadeError('');
			toast(t(text, 'files.mkdir.done', { name: name }));
			again();
		} catch (caught) {
			const said = problemOf(caught);
			setMadeError(said.detail || said.title);
		}
	}

	/** @param {string} name */
	async function remove(name) {
		setDoomed('');
		try {
			/* The name in the query and not in a body, which is where the
			   route puts it and for a measured reason: something between a
			   caller and this box rewrites the body of a removal often enough
			   that a route needing one would work from some networks and not
			   from others (ep_storage.cpp). */
			await store.write('DELETE', '/api/v1/storage/path', {
				query: { path: joinPath(dir, name) },
				touches: ['/api/v1/storage/files'],
			});
			toast(t(text, 'files.delete.done', { name: name }));
			again();
		} catch (caught) {
			refused(caught, name);
		}
	}

	if (dir === '' || !mayList) {
		return html`<div>
			<div class="files-panel">
				<h2>${t(text, 'files.where.title')}</h2>
				<p class="files-hint">${t(text, 'files.where.body')}</p>
				<${PathForm} value=${typed} onInput=${setTyped} />
				<ul class="files-starts">
					${((mounts && mounts.data && mounts.data.items) || []).map(function (one) {
						return html`<li key=${one.id}>
							<${Link} href=${listHref(one.id)}>
								<span class="mono">${one.id}</span>
								<span>${one.total > 0 ? bytes(one.free) : t(text, 'files.unknown')}</span>
							<//>
						</li>`;
					})}
				</ul>
			</div>
		</div>`;
	}

	const items = (shot && shot.data && shot.data.items) || [];
	const rows = sortRows(items, sort);
	const up = parentOf(dir);
	const failed = shot && shot.error ? problemOf(shot.error) : null;

	const columns = [
		{
			id: 'name',
			label: t(text, 'files.col.name'),
			wide: true,
			sortable: true,
			mono: true,
			/** @param {Api.File} row */
			cell: function (row) {
				if (row.kind === 'dir') {
					return html`<${Link} href=${listHref(joinPath(dir, row.id))}>${row.id}<//>`;
				}
				if (closed[row.id]) {
					return html`<span class="files-name">${row.id}<span class="files-closed">${t(text, 'files.closed.row')}</span></span>`;
				}
				return html`<a href=${downloadHref(joinPath(dir, row.id))}>${row.id}</a>`;
			},
		},
		{
			id: 'kind',
			label: t(text, 'files.col.kind'),
			sortable: false,
			mono: false,
			/** @param {Api.File} row */
			cell: function (row) { return t(text, 'files.kind.' + row.kind); },
		},
		{
			id: 'size',
			label: t(text, 'files.col.size'),
			sortable: true,
			mono: true,
			/** @param {Api.File} row */
			cell: function (row) {
				// attributes_read is what a reader tests before it looks for
				// any of these, and the box leaves them out rather than
				// writing nought, so that nothing here reads as measured.
				return row.attributes_read ? bytes(numberOf(row.size)) : t(text, 'files.unknown');
			},
		},
		{
			id: 'changed',
			label: t(text, 'files.col.changed'),
			sortable: true,
			mono: false,
			/** @param {Api.File} row */
			cell: function (row) {
				return row.attributes_read ? dayAndClock(numberOf(row.mtime)) : t(text, 'files.unknown');
			},
		},
		{
			id: 'actions',
			label: t(text, 'files.col.actions'),
			sortable: false,
			mono: false,
			align: 'end',
			/* Through the shared part, which draws the marks side by side beside
			   a pointer and one control opening a sheet beside a finger. Two
			   words at the end of a row are two targets a finger cannot tell
			   apart, and the file this row is about is what the sheet is named
			   by, so a sheet opened from the fourth row says which row. */
			/** @param {Api.File} row */
			cell: function (row) {
				const kind = row.kind === 'regular' ? mediaKind(row.id) : '';
				const showing = playing === row.id;
				/** @type {Array<{ id: string, label: string, mark: string, onAct: () => void }>} */
				const acts = [];
				if (kind) {
					acts.push({
						id: 'preview',
						label: showing ? t(text, 'files.preview.close') : t(text, 'files.preview'),
						mark: showing ? '\u25a0' : '\u25b6',
						onAct: function () { setPlaying(showing ? '' : row.id); },
					});
				}
				acts.push({
					id: 'remove',
					label: t(text, 'files.delete'),
					mark: '\u2715',
					onAct: function () { setDoomed(row.id); },
				});
				return html`<span class="acts"><${RowActions} title=${row.id} actions=${acts} /></span>`;
			},
		},
	];

	/* The last piece of the trail is not a link and carries no aria-current.
	   The bar above already marks which destination the page is on, one mark
	   per document is what a reader looks for, and a second one would turn the
	   one question the behaviour net asks about that bar into an answer about
	   this trail instead. */
	return html`<div>
		<ol class="files-crumbs">
			<li><${Link} href=${listHref('')}>${t(text, 'files.root')}<//></li>
			${crumbsOf(dir).map(function (crumb) {
				return html`<li key=${crumb.path}>
					<span aria-hidden="true">/</span>
					${crumb.path === dir
						? html`<span class="files-here">${crumb.label}</span>`
						: html`<${Link} href=${listHref(crumb.path)}>${crumb.label}<//>`}
				</li>`;
			})}
		</ol>

		<div class="files-bar">
			<${PathForm} value=${typed} onInput=${setTyped} />
			${up === '' ? null : html`<${Link} class="btn" href=${listHref(up)}>${t(text, 'files.up')}<//>`}
			<${Button} class="files-reload" onClick=${again}>${t(text, 'files.reload')}<//>
			<${Button} class="files-mkdir" onClick=${function () { setMaking(true); setMadeError(''); }}>${t(text, 'files.mkdir')}<//>
		</div>

		<p class="files-hint">
			<strong>${t(text, 'files.closed.title')}</strong>
			${t(text, 'files.closed.body')}
		</p>

		<${Upload} dir=${dir} onDone=${again} />

		<p class="files-hint">${t(text, 'files.download.hint')}</p>

		<${State}
			problem=${failed}
			phase=${shot ? shot.phase : ''}
			empty=${shot && shot.state === store.READY && rows.length === 0 ? t(text, 'files.empty') : false}
			onRetry=${again}>
			<${Table}
				columns=${columns}
				rows=${rows}
				sort=${sort}
				onSort=${setSort}
				rowKey=${function (/** @type {Api.File} */ row) { return row.id; }}
				detail=${function (/** @type {Api.File} */ row) {
					/* Under the row and not at the foot of the screen. The
					   button that opens this stands in the row, and twenty
					   rows further down is out of sight: pressing it looked
					   like pressing nothing. */
					if (playing !== row.id) {
						return null;
					}
					const path = joinPath(dir, row.id);
					return html`<${FilePlay} key=${path} name=${row.id} href=${downloadHref(path)}
						outside=${outside} />`;
				}} />
		<//>

		<${Dialog}
			open=${doomed !== ''}
			title=${t(text, 'files.delete.title')}
			onCancel=${function () { setDoomed(''); }}
			onConfirm=${function () { remove(doomed); }}>
			<p>${t(text, 'files.delete.body', { name: doomed })}</p>
		<//>

		<${Dialog}
			open=${making}
			title=${t(text, 'files.mkdir')}
			onCancel=${function () { setMaking(false); setMadeError(''); }}
			onConfirm=${make}>
			<${Field}
				label=${t(text, 'files.mkdir.label')}
				value=${madeName}
				hint=${t(text, 'files.mkdir.hint', { dir: dir })}
				error=${madeError}
				onInput=${function (/** @type {Web.On<HTMLInputElement, InputEvent>} */ event) { setMadeName(event.currentTarget.value); }} />
		<//>
	</div>`;
}

/* The address bar of this screen.

   A form and not a field that acts on every key press: a listing is a request to
   the box, and asking for one on the way to typing a name would ask for every
   prefix of it. Submitting goes through the router, so the address a person typed
   is the address in the bar afterwards. */
/**
 * @param {{ value: string, onInput: (value: string) => void }} props
 * @returns {Web.Drawn}
 */
function PathForm(props) {
	return html`<form
		class="files-form"
		onSubmit=${function (/** @type {Web.On<HTMLFormElement, SubmitEvent>} */ event) {
			event.preventDefault();
			const where = tidy(props.value);
			if (where !== '') {
				route(listHref(where));
			}
		}}>
		<${Field}
			label=${t(text, 'files.path.label')}
			value=${props.value}
			onInput=${function (/** @type {Web.On<HTMLInputElement, InputEvent>} */ event) { props.onInput(event.currentTarget.value); }} />
		<${Button} type="submit" primary=${true}>${t(text, 'files.path.open')}<//>
	</form>`;
}

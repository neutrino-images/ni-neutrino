import { html, useState, useEffect, useRef } from '../../runtime.js';
import * as store from '../../store.js';
import * as session from '../../session.js';
import { t, language } from '../../i18n.js';
import text from './recordings.text.js';
import { bytes, dayAndClock } from '../../fmt.js';
import { State } from '../../ui/state.js';
import { Button } from '../../ui/button.js';
import { Table } from '../../ui/table.js';
import { Dialog } from '../../ui/dialog.js';
import { Field } from '../../ui/field.js';
import { Select } from '../../ui/select.js';
import { RowActions } from '../../ui/actions.js';
import { toast } from '../../ui/toast.js';
import * as playing from '../../ui/playing.js';
import { DetailSheet, FactList, genreWord } from '../../ui/event.js';
import { waking } from '../../ui/wake.js';
import { refusalKey, readArchive, archiveQuery, lengthWords, archiveFileHref, archivePlaylistHref, kArchiveSortKeys,
	nextArchiveSort, onlyKeys, readArchiveDetails, archiveCoverHref, paragraphsOf, kAlwaysLocked, kTvRefusals } from './archive.model.js';

export const css = '/app/screens/recordings/recordings.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'rec.archive.lead'); }

const kFilterPauseMs = 300;

let keptSort = /** @type {Web.Sort} */ ({ column: 'start', dir: 'desc' });

/**
 * @param {string} key
 * @returns {string}
 */
function keyLabel(key) {
	return t(text, 'rec.archive.col.' + key);
}

/** @returns {Web.Drawn} */
export default function Archive() {
	const [title, setTitle] = useState('');
	const [filter, setFilter] = useState('');
	const [sort, setSort] = useState(keptSort);
	const [pages, setPages] = useState(/** @type {number[]} */ ([0]));

	// One list read per pause in typing, not per key.
	useEffect(function () {
		if (title === filter)
			return undefined;
		const timer = window.setTimeout(function () {
			setFilter(title);
			setPages([0]);
		}, kFilterPauseMs);
		return function () { window.clearTimeout(timer); };
	}, [title, filter]);

	/** @param {Web.Sort} next */
	function sortBy(next) {
		keptSort = next;
		setSort(next);
		setPages([0]);
	}

	const choices = [];
	for (const key of kArchiveSortKeys) {
		for (const dir of ['asc', 'desc']) {
			choices.push({ value: key + ' ' + dir, label: t(text, 'rec.archive.sort.choice',
				{ key: keyLabel(key), order: t(text, 'rec.archive.order.' + dir) }) });
		}
	}

	return html`<section class="rec-archive">
		<${Field} id="rec-archive-filter" label=${t(text, 'rec.archive.filter')} value=${title} autocomplete="off"
			onInput=${function (/** @type {Event} */ e) {
				setTitle(/** @type {HTMLInputElement} */ (e.currentTarget).value);
			}} />
		<div class="rec-archive-sort">
			<${Select} id="rec-archive-sort" label=${t(text, 'rec.archive.sort')} value=${sort.column + ' ' + sort.dir}
				options=${choices}
				onChange=${function (/** @type {Event} */ e) {
					const picked = /** @type {HTMLSelectElement} */ (e.currentTarget).value.split(' ');
					sortBy({ column: picked[0] || 'start', dir: picked[1] === 'asc' ? 'asc' : 'desc' });
				}} />
		</div>
		<${ArchivePages} title=${filter} sort=${sort} offsets=${pages}
			onSort=${function (/** @type {string} */ key) { sortBy(nextArchiveSort(sort, key)); }}
			onMore=${function (/** @type {number} */ next) {
				// Near-end watch and button may both ask for one offset.
				setPages(function (was) { return was.indexOf(next) === -1 ? was.concat([next]) : was; });
			}} />
	</section>`;
}

/**
 * One watch per offset, keyed by the whole query, so a late answer for an old
 * filter never reaches the rows.
 *
 * @param {{
 *   title: string,
 *   sort: Web.Sort,
 *   offsets: number[],
 *   onSort: (key: string) => void,
 *   onMore: (next: number) => void
 * }} props
 * @returns {Web.Drawn}
 */
function ArchivePages(props) {
	const title = props.title;
	const sort = props.sort;
	const offsets = props.offsets;
	const offsetsKey = offsets.join(',');
	const [snaps, setSnaps] = useState(
		/** @type {Record<string, Web.Snapshot<unknown>>} */ ({}));

	/**
	 * @param {number} offset
	 * @returns {Record<string, string>}
	 */
	function queryAt(offset) {
		return archiveQuery(title, offset, sort.column, sort.dir);
	}

	useEffect(function () {
		const keys = offsets.map(function (offset) { return JSON.stringify(queryAt(offset)); });
		setSnaps(function (was) { return onlyKeys(was, keys); });
		const stops = offsets.map(function (offset, at) {
			const query = queryAt(offset);
			const key = keys[at] || '';
			return store.watch('GET', '/api/v1/recordings/archive', { query: query },
				function (/** @type {Web.Snapshot<unknown>} */ snap) {
					setSnaps(function (was) {
						const next = Object.assign({}, was);
						next[key] = snap;
						return next;
					});
				});
		});
		return function () { stops.forEach(function (stop) { stop(); }); };
	}, [title, sort.column, sort.dir, offsetsKey]);

	/** @type {import('./archive.model.js').ArchiveItem[]} */
	const rows = [];
	let next = -1;
	let total = 0;
	let loaded = false;
	let phase = /** @type {Web.Phase} */ ('first');
	let problem = /** @type {Web.Shown | null} */ (null);
	for (const offset of offsets) {
		const snap = snaps[JSON.stringify(queryAt(offset))];
		if (!snap) {
			continue;
		}
		phase = snap.phase;
		if (snap.error && !problem) {
			problem = snap.error.problem;
		}
		if (snap.data) {
			const page = readArchive(snap.data);
			rows.push.apply(rows, page.items);
			next = page.next;
			total = page.total;
			loaded = true;
		}
	}

	const [opened, setOpened] = useState(/** @type {Record<string, ArchiveOpen>} */ ({}));
	const [refusals, setRefusals] = useState(/** @type {Record<string, { why: string, codec: string }>} */ ({}));
	const [asked, setAsked] = useState(/** @type {import('./archive.model.js').ArchiveItem | null} */ (null));
	const [shown, setShown] = useState(/** @type {string | null} */ (null));

	const [now, setNow] = useState(playing.current());

	// The player stops on a refusal, so the reason is kept here.
	useEffect(function () {
		/** @param {import('../../ui/playing.js').Playing | null} next */
		function seen(next) {
			setNow(next);
			if (!next || next.source.of !== 'file' || next.failure === '') {
				return;
			}
			const href = next.source.id;
			const why = { why: next.failure, codec: next.said };
			setRefusals(function (was) { return Object.assign({}, was, { [href]: why }); });
		}
		seen(playing.current());
		return playing.subscribe(seen);
	}, []);

	/**
	 * @param {string} id
	 * @param {Partial<ArchiveOpen>} change
	 * @returns {void}
	 */
	function mark(id, change) {
		setOpened(function (was) {
			const one = Object.assign({ play: false, vlc: false, said: '' }, was[id], change);
			return Object.assign({}, was, { [id]: one });
		});
	}

	/** @param {import('./archive.model.js').ArchiveItem} row */
	function playHere(row) {
		const href = archiveFileHref(row.id);
		setRefusals(function (was) {
			const next = Object.assign({}, was);
			delete next[href];
			return next;
		});
		setOpened(function (was) {
			/** @type {Record<string, ArchiveOpen>} */
			const next = {};
			for (const id of Object.keys(was)) {
				const one = was[id];
				if (one) {
					next[id] = Object.assign({}, one, { play: false });
				}
			}
			next[row.id] = Object.assign({ play: false, vlc: false, said: '' }, was[row.id], { play: true, said: '' });
			return next;
		});
		playHereNow(row.id, nameOf(row));
	}

	/** @param {import('./archive.model.js').ArchiveItem} row */
	function stopHere(row) {
		playing.stop();
		forget(row.id);
	}

	/**
	 * @param {string} id
	 * @returns {void}
	 */
	function forget(id) {
		setOpened(function (was) {
			const next = Object.assign({}, was);
			delete next[id];
			return next;
		});
	}

	/**
	 * @param {import('./archive.model.js').ArchiveItem} row
	 * @param {unknown} caught
	 * @param {Record<string, string>} words
	 * @returns {void}
	 */
	function failed(row, caught, words) {
		if (codeOf(caught) === 'no-such-recording') {
			forget(row.id);
			toast(t(text, 'rec.archive.gone'), 'bad');
			store.invalidate('/api/v1/recordings/archive');
			return;
		}
		mark(row.id, { said: refusalOf(caught, words) });
	}

	/**
	 * @param {import('./archive.model.js').ArchiveItem} row
	 */
	function playTv(row) {
		session.requireLevel('write').then(function () {
			waking(function (/** @type {boolean} */ wake, /** @type {boolean} */ stop) {
				return store.write('POST', '/api/v1/recordings/archive/{id}/play', {
					params: { id: row.id },
					touches: ['/api/v1/recordings/archive'],
					body: stop ? { wake: wake, stop_playback: true } : { wake: wake },
				});
			}, 'play').then(function (/** @type {boolean} */ sent) {
				if (sent) {
					mark(row.id, { said: t(text, 'rec.archive.tv.sent', { title: nameOf(row) }) });
				}
			}, function (/** @type {unknown} */ caught) {
				failed(row, caught, kTvRefusals);
			});
		}, function () { });
	}

	/** @param {import('./archive.model.js').ArchiveItem} row */
	function askDelete(row) {
		session.requireLevel('write').then(function () { setAsked(row); }, function () { });
	}

	/** @param {import('./archive.model.js').ArchiveItem} row */
	function remove(row) {
		const href = archiveFileHref(row.id);
		const now = playing.current();
		if (now && now.source.id === href) {
			playing.stop();
		}
		store.write('DELETE', '/api/v1/recordings/archive/{id}', {
			params: { id: row.id },
			touches: ['/api/v1/recordings/archive'],
		}).then(function () {
			toast(t(text, 'rec.archive.deleted', { title: nameOf(row) }));
			forget(row.id);
		}, function (/** @type {unknown} */ caught) {
			failed(row, caught, {
				'recording-running': 'rec.archive.running',
				'recording-playing': 'rec.archive.playing.now',
			});
		});
	}

	/**
	 * @param {import('./archive.model.js').ArchiveItem} row
	 * @returns {Web.Drawn}
	 */
	function detail(row) {
		const open = opened[row.id];
		if (!open) {
			return null;
		}
		const bad = open.play ? refusals[archiveFileHref(row.id)] : undefined;
		return html`<div class="rec-archive-detail">
			${open.said ? html`<p role="status" data-part="archive-said">${open.said}</p>` : null}
			${bad
				? html`<p role="status" data-part="archive-said">${t(text, 'rec.archive.no.' + knownFailure(bad.why), { codec: bad.codec })}</p>`
				: null}
			${open.play && !bad ? html`<${ArchivePlay} href=${archiveFileHref(row.id)} />` : null}
			${open.vlc || bad ? html`<${ArchiveVlc} id=${row.id} />` : null}
		</div>`;
	}

	/**
	 * @param {import('./archive.model.js').ArchiveItem} row
	 * @returns {import('../../ui/actions.js').RowAction[]}
	 */
	function actionsOf(row) {
		return archiveActions(row, isHere(now, row.id), {
			onInfo: function (/** @type {import('./archive.model.js').ArchiveItem} */ one) { setShown(one.id); },
			onPlayHere: playHere,
			onStop: stopHere,
			onPlayTv: playTv,
			onDelete: askDelete,
			onVlc: function (/** @type {import('./archive.model.js').ArchiveItem} */ one) {
				const was = opened[one.id];
				mark(one.id, { vlc: !(was && was.vlc) });
			},
		});
	}

	/** @type {import('../../ui/table.js').Column<import('./archive.model.js').ArchiveItem>[]} */
	const columns = [
		{ id: 'title', label: keyLabel('title'), sortable: true, wide: true, cell: function (/** @type {import('./archive.model.js').ArchiveItem} */ r) {
			return html`<button type="button" class="rec-archive-name" data-archive-open=${r.id} aria-haspopup="dialog"
				onClick=${function () { setShown(r.id); }}><span class="rec-cut" data-archive=${r.id} title=${r.title || null}>${nameOf(r)}</span>${r.playing
				? html`<span class="rec-archive-on">${t(text, 'rec.archive.playing')}</span>` : null}</button>`;
		} },
		{ id: 'channel', label: keyLabel('channel'), sortable: true, cell: function (/** @type {import('./archive.model.js').ArchiveItem} */ r) {
			return html`<span class="rec-cut" title=${r.channel || null}>${r.channel}</span>`;
		} },
		{ id: 'start', label: keyLabel('start'), sortable: true, cell: function (/** @type {import('./archive.model.js').ArchiveItem} */ r) { return r.start ? dayAndClock(r.start) : ''; } },
		{ id: 'duration', label: keyLabel('duration'), sortable: true, align: 'end', cell: function (/** @type {import('./archive.model.js').ArchiveItem} */ r) { return lengthWords(r.duration); } },
		{ id: 'size', label: keyLabel('size'), sortable: true, align: 'end', cell: function (/** @type {import('./archive.model.js').ArchiveItem} */ r) { return bytes(r.size); } },
		{ id: 'act', label: t(text, 'rec.archive.col.act'), align: 'end', cell: function (/** @type {import('./archive.model.js').ArchiveItem} */ r) {
				return html`<${ArchiveActs} row=${r} actions=${actionsOf(r)} />`;
		} },
	];

	return html`<div class="rec-archive-list">
		<${State} phase=${phase} problem=${problem}
			empty=${loaded && rows.length === 0 ? html`<span data-part="archive-empty">${t(text, 'rec.archive.empty')}</span>` : false}>
			${rows.length ? html`<${Table} columns=${columns} rows=${rows} sort=${sort}
				onSort=${function (/** @type {Web.Sort} */ next) { props.onSort(next.column); }}
				rowKey=${function (/** @type {import('./archive.model.js').ArchiveItem} */ r) { return r.id; }}
				detail=${detail}
				hasMore=${next >= 0} onNearEnd=${function () { if (next >= 0) props.onMore(next); }} />` : null}
		<//>
		${next >= 0 ? html`<div class="rec-archive-more" data-act="archive-more"><${Button} onClick=${function () { props.onMore(next); }}>${t(text, 'rec.archive.more')}<//></div>` : null}
		${rows.length ? html`<p class="hint">${t(text, 'rec.archive.count', { shown: rows.length, total: total })}</p>` : null}
		<${Dialog}
			open=${!!asked}
			title=${t(text, 'rec.archive.delete')}
			confirmLabel=${t(text, 'rec.archive.delete')}
			onCancel=${function () { setAsked(null); }}
			onConfirm=${function () {
				const one = asked;
				setAsked(null);
				if (one)
					remove(one);
			}}>
			<p data-part="archive-delete-ask">${asked ? t(text, 'rec.archive.delete.ask', { title: nameOf(asked) }) : ''}</p>
		<//>
		<${ArchiveDetails} id=${shown} actionsOf=${actionsOf} onClose=${function () { setShown(null); }} />
	</div>`;
}

/**
 * @param {import('../../ui/playing.js').Playing | null} now
 * @param {string} id
 * @returns {boolean} whether the page's player is playing this recording
 */
export function isHere(now, id) {
	return now !== null && now.source.of === 'file' && now.source.id === archiveFileHref(id);
}

/**
 * @param {{
 *   id: string | null,
 *   actionsOf: (row: import('./archive.model.js').ArchiveItem) => import('../../ui/actions.js').RowAction[],
 *   onClose: () => void
 * }} props
 * @returns {Web.Drawn}
 */
export function ArchiveDetails(props) {
	const id = props.id;
	const [shot, setShot] = useState(/** @type {Web.Snapshot<unknown> | null} */ (null));

	useEffect(function () {
		setShot(null);
		if (id === null) {
			return undefined;
		}
		return store.watch('GET', '/api/v1/recordings/archive/{id}', { params: { id: id } },
			function (/** @type {Web.Snapshot<unknown>} */ snap) { setShot(snap); });
	}, [id]);

	const held = useRef(/** @type {{ id: string, known: import('./archive.model.js').ArchiveDetails } | null} */ (null));
	const fresh = shot && shot.data ? readArchiveDetails(shot.data) : null;
	if (fresh && id !== null) {
		held.current = { id: id, known: fresh };
	}
	// Keeps the last answer while asking again, so the focus stays.
	const kept = !fresh && !(shot && shot.error) && held.current && held.current.id === id ? held.current.known : null;
	const known = fresh || kept;
	const row = known ? known.item : null;
	const title = row ? nameOf(row) : t(text, 'rec.details');
	const when = row ? [row.channel, row.start ? dayAndClock(row.start) : '', lengthWords(row.duration)]
		.filter(function (part) { return part !== ''; }).join(' \u00b7 ') : '';

	return html`<${DetailSheet} open=${id !== null} title=${title} when=${when} onClose=${props.onClose}>
		${known && row ? html`<div class="rec-details" data-archive-details=${row.id} aria-busy=${fresh ? null : 'true'}>
			${known.cover ? html`<img class="rec-details-cover" src=${archiveCoverHref(row.id)}
				alt=${t(text, 'rec.details.cover', { title: nameOf(row) })} />` : null}
			${known.description ? html`<p class="ev-text">${known.description}</p>` : null}
			${paragraphsOf(known.longDescription).map(function (line) { return html`<p class="ev-long">${line}</p>`; })}
			<${FactList} facts=${factsOf(known)} />
		</div>` : html`<${State} phase=${shot ? shot.phase : 'first'}
			problem=${shot && shot.error ? shot.error.problem : null} />`}
		<p class="ev-acts">
			${row ? props.actionsOf(row).filter(function (one) { return one.id !== 'info'; }).map(function (one) {
				return html`<button key=${one.id} type="button" class="btn" data-act=${one.id}
					onClick=${function () { props.onClose(); one.onAct(); }}>${one.label}</button>`;
			}) : null}
			<button type="button" class="btn" data-act="details-close" onClick=${props.onClose}>${t(text, 'rec.details.close')}</button>
		</p>
	<//>`;
}

/**
 * @param {import('./archive.model.js').ArchiveDetails} known
 * @returns {{ term: string, value: string }[]}
 */
function factsOf(known) {
	const made = [known.country, known.year ? String(known.year) : ''].filter(function (part) { return part !== ''; });
	return [
		{ term: t(text, 'rec.details.genre'), value: genreWord(known.genre) },
		{ term: t(text, 'rec.details.series'), value: known.series },
		{ term: t(text, 'rec.details.made'), value: made.join(' ') },
		{ term: t(text, 'rec.details.rating'), value: known.rating
			? (known.rating / 10).toLocaleString(language(), { minimumFractionDigits: 1, maximumFractionDigits: 1 }) : '' },
		{ term: t(text, 'rec.details.quality'), value: known.quality
			? t(text, 'rec.details.quality.value', { stars: known.quality }) : '' },
		{ term: t(text, 'rec.details.age'), value: known.age === kAlwaysLocked ? t(text, 'rec.details.age.always')
			: known.age ? t(text, 'rec.details.age.value', { years: known.age }) : '' },
		{ term: t(text, 'rec.details.audio'), value: known.audio.join(', ') },
	];
}

/**
 * @typedef {object} ArchiveOpen
 * @property {boolean} play
 * @property {boolean} vlc
 * @property {string} said
 */

const kFailures = ['picture', 'codec', 'browser', 'load', 'stream', 'format'];

/**
 * @param {string} why
 * @returns {string}
 */
function knownFailure(why) {
	return kFailures.indexOf(why) === -1 ? 'format' : why;
}

/**
 * @param {import('./archive.model.js').ArchiveItem} row
 * @returns {string}
 */
function nameOf(row) {
	return row.title || row.id;
}

/**
 * @param {unknown} caught
 * @param {Record<string, string>} words key per refusal code
 * @returns {string}
 */
function refusalOf(caught, words) {
	const key = words[codeOf(caught)];
	if (key) {
		return t(text, key);
	}
	const said = refusalKey(caught);
	return t(text, said);
}

/**
 * @param {unknown} caught
 * @returns {{ type?: string, title?: string, detail?: string } | null}
 */
function problemOf(caught) {
	const failure = /** @type {{ problem?: { type?: string, title?: string, detail?: string } } | null} */ (caught);
	return failure && failure.problem ? failure.problem : null;
}

/**
 * @param {unknown} caught
 * @returns {string} the refusal code, empty where the box named none
 */
function codeOf(caught) {
	const problem = problemOf(caught);
	const type = problem && problem.type ? problem.type : '';
	return type.indexOf('/errors/') === 0 ? type.slice('/errors/'.length) : '';
}

/** @returns {Web.Drawn} */
function tvMark() {
	return html`<svg viewBox="0 0 16 16" width="1em" height="1em" fill="none" stroke="currentColor"
	stroke-width="1.5" aria-hidden="true"><rect x="1.5" y="2.5" width="13" height="9" rx="1" /><path d="M5 14.5h6M8 11.5v3" /></svg>`;
}

/**
 * @param {import('./archive.model.js').ArchiveItem} row
 * @param {boolean} here
 * @param {{
 *   onInfo: (row: import('./archive.model.js').ArchiveItem) => void,
 *   onPlayHere: (row: import('./archive.model.js').ArchiveItem) => void,
 *   onStop: (row: import('./archive.model.js').ArchiveItem) => void,
 *   onPlayTv: (row: import('./archive.model.js').ArchiveItem) => void,
 *   onVlc: (row: import('./archive.model.js').ArchiveItem) => void,
 *   onDelete: (row: import('./archive.model.js').ArchiveItem) => void
 * }} on
 * @returns {import('../../ui/actions.js').RowAction[]}
 */
function archiveActions(row, here, on) {
	return [
		{ id: 'info', mark: '\u2139\ufe0e', label: t(text, 'rec.archive.info'),
			named: t(text, 'rec.archive.info.for', { title: nameOf(row) }), onAct: function () { on.onInfo(row); } },
		here
			? { id: 'play-stop', mark: '\u25a0', label: t(text, 'rec.archive.stop'), onAct: function () { on.onStop(row); } }
			: { id: 'play-here', mark: '\u25b6\ufe0e', label: t(text, 'rec.archive.here'), onAct: function () { on.onPlayHere(row); } },
		{ id: 'play-tv', mark: tvMark(), label: t(text, 'rec.archive.tv'), onAct: function () { on.onPlayTv(row); } },
		{ id: 'vlc-open', mark: '\u2193', label: t(text, 'rec.archive.vlc'), onAct: function () { on.onVlc(row); } },
		{ id: 'delete', mark: '\u2715', label: t(text, 'rec.archive.delete'), onAct: function () { on.onDelete(row); } },
	];
}

/**
 * @param {{
 *   row: import('./archive.model.js').ArchiveItem,
 *   actions: import('../../ui/actions.js').RowAction[]
 * }} props
 * @returns {Web.Drawn}
 */
function ArchiveActs(props) {
	return html`<span class="acts" data-archive-act=${props.row.id}><${RowActions}
		title=${nameOf(props.row)}
		keep=${'archive:' + props.row.id}
		actions=${props.actions} /></span>`;
}

/**
 * @param {{ href: string }} props
 * @returns {Web.Drawn}
 */
function ArchivePlay(props) {
	const slot = useRef(/** @type {HTMLDivElement | null} */ (null));
	const [now, setNow] = useState(playing.current());

	useEffect(function () {
		setNow(playing.current());
		return playing.subscribe(setNow);
	}, []);

	const ours = now === null || (now.source.of === 'file' && now.source.id === props.href);

	useEffect(function () {
		if (!ours) {
			return undefined;
		}
		playing.setSlot('page', slot.current);
		return function () { playing.setSlot('page', null); };
	}, [ours]);

	return html`<div class="rec-archive-media" ref=${slot}></div>`;
}

/**
 * Plays a recording in the page's own player.
 *
 * @param {string} id
 * @param {string} name
 * @returns {void}
 */
export function playHereNow(id, name) {
	playing.start(playing.ofFile(name, archiveFileHref(id), { how: 'demuxed', sound: false }));
}

/**
 * A recording's playlist address for a player elsewhere, null while it is
 * being made. A reader without sign in gets no token; asking would be refused.
 *
 * @param {string} id
 * @returns {{ href: string, home: boolean } | null}
 */
export function usePlaylist(id) {
	const [signed, setSigned] = useState(session.canSystem());
	const [token, setToken] = useState(/** @type {string | null} */ (null));

	useEffect(function () {
		return session.subscribe(function () { setSigned(session.canSystem()); });
	}, []);

	useEffect(function () {
		let live = true;
		if (!signed || id === '') {
			setToken('');
			return function () { live = false; };
		}
		setToken(null);
		session.mediaToken().then(function (got) {
			if (live)
				setToken(got);
		}, function () {
			if (live)
				setToken('');
		});
		return function () { live = false; };
	}, [signed, id]);

	if (token === null || id === '') {
		return null;
	}
	return { href: new URL(archivePlaylistHref(id, token), window.location.href).href, home: token === '' };
}

/**
 * @param {{ id: string }} props
 * @returns {Web.Drawn}
 */
function ArchiveVlc(props) {
	const list = usePlaylist(props.id);
	if (list === null) {
		return null;
	}
	return html`<p class="rec-archive-vlc">
		<a data-act="vlc" href=${list.href}>${t(text, 'rec.archive.vlc')}</a>
		<span class="hint">${t(text, list.home ? 'rec.archive.vlc.how.home' : 'rec.archive.vlc.how')}</span>
	</p>`;
}

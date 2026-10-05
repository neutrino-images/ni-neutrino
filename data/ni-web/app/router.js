// Routes derived from the navigation. Import targets stay literals so checks can follow them.
import { html, Router, getCurrentUrl, useState, useEffect } from './runtime.js';
import { areas, ids, areaById, visibleAreas, secondLevel, loadSecondLevel, entryById, firstEntry, labelOf } from './nav.js';
import { ensureCss } from './css.js';
import { t } from './i18n.js';
import text from './shell.text.js';
import { Topbar } from './ui/topbar.js';
import { Dock } from './ui/dock.js';
import { Float } from './ui/float.js';
import { MenuSheet } from './ui/sheet.js';
import { Toasts } from './ui/toast.js';
import { State } from './ui/state.js';
import { usePullToRefresh, PullIndicator } from './ui/pulldown.js';

const kPullCss = '/app/ui/pulldown.css';

/**
 * @param {string | null | undefined} url
 * @returns {{ area: string, entry: string, param: string }}
 */
export function matchPath(url) {
	const whole = String(url === undefined || url === null ? '/' : url);
	// split never answers an empty list.
	const path = (whole.split('?')[0] || '').split('#')[0] || '';
	const parts = path.split('/').filter(function (part) { return part !== ''; });
	return {
		area: parts[0] || '',
		entry: parts[1] || '',
		param: parts[2] === undefined ? '' : decodeURIComponent(parts[2])
	};
}

// The rule of src/httpd/apppaths.cpp, so page and server agree.
/**
 * @param {string | null | undefined} href
 * @returns {boolean}
 */
export function isAppPath(href) {
	const whole = String(href === undefined || href === null ? '' : href);
	const path = (whole.split('?')[0] || '').split('#')[0] || '';
	if (path.charAt(0) !== '/')
		return false;
	if (path === '/')
		return true;
	if (path.charAt(path.length - 1) === '/')
		return false;
	const parts = path.split('/');
	if (ids.indexOf(parts[1] || '') === -1)
		return false;
	return (parts[parts.length - 1] || '').indexOf('.') === -1;
}

// Marks rather than cancels, on the way down, so other listeners still get the click.
/**
 * @param {Event} event
 * @returns {void}
 */
function markWhatLeaves(event) {
	let node = /** @type {Node | null} */ (event.target);
	while (node) {
		const element = /** @type {HTMLElement} */ (node);
		if (element.localName === 'a') {
			const href = element.getAttribute('href');
			if (href !== null && !isAppPath(href))
				element.setAttribute('data-native', '');
			return;
		}
		node = node.parentNode;
	}
}

/**
 * @param {Web.NavArea | null} area
 * @param {string} asked
 * @returns {string} what the bars show as the open entry when only the
 *          destination was asked for
 */
export function activeEntryId(area, asked) {
	if (asked)
		return asked;
	const first = firstEntry(area);
	return first ? first.id : '';
}

/**
 * @param {{
 *   areaId: string,
 *   entry: Web.NavEntry | null,
 *   param: string,
 *   ctx: Web.Context
 * }} props
 * @returns {Web.Drawn}
 */
function Screen(props) {
	const entry = props.entry;
	const [view, setView] = useState(/** @type {{ draw: (props: any) => Web.Drawn, lead?: () => string } | null} */ (null));
	const [problem, setProblem] = useState(/** @type {Web.Shown | null} */ (null));

	useEffect(function () {
		let alive = true;
		setView(null);
		setProblem(null);

		if (!entry)
			return undefined;

		entry.load().then(function (module) {
			return module.css ? ensureCss(module.css).then(function () { return module; }) : module;
		}).then(function (module) {
			if (alive)
				setView({ draw: module.default, lead: module.lead });
		}, function (/** @type {unknown} */ failed) {
			if (alive) {
				const why = /** @type {{ message?: unknown } | null} */ (failed);
				setProblem({
					title: t(text, 'shell.failed'),
					detail: String(why && why.message ? why.message : failed),
				});
			}
		});

		return function () { alive = false; };
	}, [props.areaId, entry ? entry.id : '']);

	if (problem)
		return html`<${State} problem=${problem} />`;
	if (!view)
		return html`<${State} phase="first" />`;

	// The frame draws every screen's name; the lead is a function so it reads the chosen language.
	const said = view.lead ? view.lead() : '';

	return html`<h1 class="scr">${entry ? labelOf(entry) : ''}</h1>
		${said === '' ? null : html`<p class="scr">${said}</p>`}
		<${view.draw} param=${props.param} ctx=${props.ctx} entry=${entry} />`;
}

/**
 * @param {{ url?: string }} props
 * @returns {Web.Drawn}
 */
function NotFound(props) {
	return html`<div class="note">
		<h2>${t(text, 'shell.notfound.title')}</h2>
		<p>${t(text, 'shell.notfound.body', { path: props.url || getCurrentUrl() })}</p>
	</div>`;
}

function first() {
	const one = areas[0];
	if (!one) {
		throw new Error('router: the navigation names no destination at all');
	}
	return one;
}

/**
 * @param {{ area: Web.NavArea, entry?: string, param?: string, ctx: Web.Context, path?: string }} props
 * @returns {Web.Drawn}
 */
function AreaRoute(props) {
	const area = props.area;
	const items = secondLevel(area);
	if (!items)
		return html`<${State} phase="first" />`;

	const wanted = activeEntryId(area, props.entry || '');
	const entry = entryById(area, wanted);
	if (!entry)
		return html`<${NotFound} url=${getCurrentUrl()} />`;

	return html`<${Screen} areaId=${area.id} entry=${entry} param=${props.param} ctx=${props.ctx} />`;
}

/**
 * @param {{ ctx: Web.Context, status?: Web.ShellStatus | null }} props
 * @returns {Web.Drawn}
 */
export function Shell(props) {
	const ctx = props.ctx;
	const [url, setUrl] = useState(getCurrentUrl());
	const [menuOpen, setMenuOpen] = useState(false);
	const setResolved = useState(0)[1];

	const here = matchPath(url);
	const area = areaById(here.area) || (here.area === '' ? first() : null);
	const items = area ? secondLevel(area) : null;
	// Resolved, so a former name still marks its entry.
	const asked = area ? activeEntryId(area, here.entry) : '';
	const open = area ? entryById(area, asked) : null;
	const entryId = open ? open.id : asked;

	const pull = usePullToRefresh();

	useEffect(function () {
		if (!area || items)
			return;
		loadSecondLevel(area, ctx).then(bump, bump);
	}, [area ? area.id : '', items]);

	useEffect(function () {
		ensureCss(kPullCss);
	}, []);

	useEffect(function () {
		document.addEventListener('click', markWhatLeaves, true);
		return function () { document.removeEventListener('click', markWhatLeaves, true); };
	}, []);

	function bump() {
		setResolved(function (n) { return n + 1; });
	}

	const routes = areas.map(function (one) {
		return html`<${AreaRoute}
			key=${one.id}
			path=${'/' + one.id + '/:entry?/:param?'}
			area=${one}
			ctx=${ctx} />`;
	});

	// Routes cover every destination; the bars only what this build offers.
	const offered = visibleAreas(areas, props.status ? props.status.build : null);

	return html`<div class="shell">
		<${Topbar}
			areas=${offered}
			activeArea=${area ? area.id : ''}
			area=${area}
			items=${items}
			activeEntry=${entryId}
			status=${props.status}
			event=${props.status ? props.status.event : null} />
		<main class="content" id="content" ref=${pull.ref}>
			<${PullIndicator} shown=${pull.shown} armed=${pull.armed} busy=${pull.busy} />
			<${Router} onChange=${function (/** @type {{ url: string }} */ event) { setUrl(event.url); setMenuOpen(false); }}>
				${routes}
				<${AreaRoute} path="/" area=${first()} ctx=${ctx} />
				<${NotFound} default />
			<//>
		</main>
		<${Float} />
		<${Dock}
			areas=${offered}
			activeArea=${area ? area.id : ''}
			open=${menuOpen}
			onOpen=${function () { setMenuOpen(true); }} />
		<${MenuSheet}
			open=${menuOpen}
			areas=${offered}
			activeArea=${area ? area.id : ''}
			onClose=${function () { setMenuOpen(false); }} />
		<${Toasts} />
	</div>`;
}

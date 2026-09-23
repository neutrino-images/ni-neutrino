// The routes, derived from the navigation and not written beside it. A destination is
// reached at /<id>, a screen at /<id>/<entry>, and what the screen is about at
// /<id>/<entry>/<what>. The server answers the document for each, so every one is an
// address that can be typed, sent to somebody, or put in a check.
//
// Screens are fetched when they are first opened, with a literal as the target: a
// target assembled out of pieces is one no check can follow.
import { html, Router, getCurrentUrl, useState, useEffect } from './runtime.js';
import { areas, ids, areaById, visibleAreas, secondLevel, loadSecondLevel, entryById, firstEntry, labelOf } from './nav.js';
import { ensureCss } from './css.js';
import { t } from './i18n.js';
import text from './shell.text.js';
import { Topbar } from './ui/topbar.js';
import { Dock } from './ui/dock.js';
/* What a channel started in the browser goes on playing in once somebody has
   walked away from the screen they started it on. A part of the frame and not
   of a screen, because outliving a screen is the whole of what it is for; a
   row of the frame and not an overlay, because below the width at which the
   destinations move to the foot of the page it is a bar and stands above
   them. */
import { Float } from './ui/float.js';
import { MenuSheet } from './ui/sheet.js';
import { Toasts } from './ui/toast.js';
import { State } from './ui/state.js';

// The address as the page reads it, which is the one thing the two bars, the
// column and the router have to agree about.
/**
 * @param {string | null | undefined} url
 * @returns {{ area: string, entry: string, param: string }}
 */
export function matchPath(url) {
	const whole = String(url === undefined || url === null ? '/' : url);
	/* split never answers an empty list, so the first piece is always there.
	   Saying so once here is shorter than saying it at each of the three
	   places below, and it is the only thing about this that is not obvious. */
	const path = (whole.split('?')[0] || '').split('#')[0] || '';
	const parts = path.split('/').filter(function (part) { return part !== ''; });
	return {
		area: parts[0] || '',
		entry: parts[1] || '',
		param: parts[2] === undefined ? '' : decodeURIComponent(parts[2])
	};
}

/* WHICH ADDRESSES ARE THIS PAGE'S, which is not the same as which of them
   start with a slash. The router underneath claims all of the second kind and
   stops the browser following them, and the catch all route below claims what
   matched nothing, so the reader under /swagger/ and every file under /api/
   ended as this page's note about a screen that does not exist.

   The rule is the server's own, out of src/httpd/apppaths.cpp: a first segment
   that is one of the destinations, nothing empty after the last separator, no
   dot in the last segment. Said the same way here, so that what this page
   walks to and what the server answers with this page cannot drift apart. */
/**
 * @param {string | null | undefined} href
 * @returns {boolean}
 */
export function isAppPath(href) {
	const whole = String(href === undefined || href === null ? '' : href);
	const path = (whole.split('?')[0] || '').split('#')[0] || '';
	if (path.charAt(0) !== '/')
		return false;
	// Where the page opens, and the one address with nothing behind the slash.
	if (path === '/')
		return true;
	if (path.charAt(path.length - 1) === '/')
		return false;
	const parts = path.split('/');
	if (ids.indexOf(parts[1] || '') === -1)
		return false;
	return (parts[parts.length - 1] || '').indexOf('.') === -1;
}

/* AND A CLICK ON ONE OF THE OTHERS IS LET GO. An anchor that says it wants to
   be left alone is one the router underneath declines, and that is what is
   marked here. What the browser then does is the anchor's own business, stated
   where the anchor is: a page of its own opens in a tab of its own, a file is
   fetched without one, because a tab opened for a download is an empty window
   somebody has to close.

   On the way down, before the router underneath is asked, and marking rather
   than cancelling: a click cancelled at the top of the page is a click taken
   away from everything else listening for one. */
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

// What the bars show as the open entry when only the destination was asked
// for: the first of the second level, which is what /<id> means.
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
			// A screen names the stylesheet it needs, and it arrives before the
			// screen is drawn rather than after, so nothing is drawn twice.
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

	/* THE NAME OF THE SCREEN AND THE SENTENCE UNDER IT ARE THE FRAME'S.

	   Every screen had answered this differently: some drew their own heading,
	   some the heading of their first card, two drew none at all, and the level
	   was h1 on four of them and h2 on the rest. One place draws it now, so
	   twenty screens cannot disagree about it and none of them can forget.

	   The name is the one the bars already show, so nothing is written twice
	   and a renamed destination is renamed once. The sentence is the screen's
	   own knowledge and stays in the screen's own catalogue: a module offers it
	   as a function, because a value read at load would be read before anybody
	   has chosen a language. */
	const said = view.lead ? view.lead() : '';

	return html`<h1 class="scr">${entry ? labelOf(entry) : ''}</h1>
		${said === '' ? null : html`<p class="scr">${said}</p>`}
		<${view.draw} param=${props.param} ctx=${props.ctx} entry=${entry} />`;
}

/* No way back written at the foot of it. The bars are on screen while this is,
   and a link at the end of one screen out of twenty is a habit somebody has to
   learn for that one screen. */
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

/* The first destination, which is where the page opens and where a bad address
   sends somebody back to. The list is settled at load and is never empty, and
   this says so once rather than at each of the three places that read it. */
function first() {
	const one = areas[0];
	if (!one) {
		throw new Error('router: the navigation names no destination at all');
	}
	return one;
}

// One route per destination, each taking the two parts behind it. The table is
// the navigation read out, so a destination cannot exist in one and not the
// other.
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
	// Only a way to ask for another draw when a second level that had to be
	// asked for has arrived. What came back is kept in the navigation, where
	// both foldings read it, so there is no second copy of it here.
	const setResolved = useState(0)[1];

	const here = matchPath(url);
	const area = areaById(here.area) || (here.area === '' ? first() : null);
	const items = area ? secondLevel(area) : null;
	/* Resolved and not taken as it was asked for, so that an address under a
	   name an entry used to have marks that entry in the bars rather than
	   leaving them with nothing marked. */
	const asked = area ? activeEntryId(area, here.entry) : '';
	const open = area ? entryById(area, asked) : null;
	const entryId = open ? open.id : asked;

	// Asked for after the draw, never during it, and asked once: what comes
	// back is kept where both foldings read it.
	useEffect(function () {
		if (!area || items)
			return;
		loadSecondLevel(area, ctx).then(bump, bump);
	}, [area ? area.id : '', items]);

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

	/* The routes are all eight and the bars are what this build offers. A
	   destination the page does not put in front of anybody is still a
	   destination it walks to when the address is typed or bookmarked, which
	   is the difference between not advertising something and breaking it. */
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
		<main class="content" id="content">
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
